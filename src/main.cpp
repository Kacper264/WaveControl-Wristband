#include <cstdio>
#include <cstdint>

#include "drivers/imu.h"
#include "ia/ia.h"
#include "ota/ota.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/rtc_io.h"
#include "esp_adc_cal.h"

#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "net/mqtt_manager.h"
#include "net/wifi_manager.h"
#include "common/strings_constants.h"
}

#define TAG_APP "APP"
#define BUTTON_PIN GPIO_NUM_7

// ---- Periods (modifiable) ----
#define BATTERY_REPORT_PERIOD_S   60
#define BATTERY_REPORT_PERIOD_MS  (BATTERY_REPORT_PERIOD_S * 1000)

// ---- Sleep policy ----
#define SLEEP_AFTER_IA_MS         (5 * 60 * 1000)  // 5 minutes après publish MQTT IA

// ---- Topics ----
#ifndef MQTT_TOPIC_HEARTBEAT
#define MQTT_TOPIC_HEARTBEAT "device/heartbeat"
#endif

/* ================== BATTERY CONFIG (A ADAPTER) ================== */
#define BAT_ADC_CHANNEL      ADC1_CHANNEL_0
#define BAT_ADC_ATTEN        ADC_ATTEN_DB_11
#define BAT_ADC_WIDTH        ADC_WIDTH_BIT_12
#define BAT_DIVIDER_RATIO    2.0f
#define VBAT_MIN             3.30f
#define VBAT_MAX             4.20f

/* ================== BATTERY UTILS ================== */

static esp_adc_cal_characteristics_t adc_chars;

static float clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static float battery_read_voltage()
{
    int raw = adc1_get_raw((adc1_channel_t)BAT_ADC_CHANNEL);
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

    float v_adc = (float)mv / 1000.0f;
    float v_bat = v_adc * BAT_DIVIDER_RATIO;

    return v_bat;
}

static uint8_t battery_read_percent()
{
    float v = battery_read_voltage();
    float pct = (v - VBAT_MIN) * 100.0f / (VBAT_MAX - VBAT_MIN);
    pct = clampf(pct, 0.0f, 100.0f);
    return (uint8_t)(pct + 0.5f);
}

/* ================== STRUCT ================== */

struct AiResult {
    Move move;
    uint8_t confidence;
};

/* ================== RTOS ================== */

static QueueHandle_t ai_queue;
static TaskHandle_t acquisition_handle = nullptr;

// Timer sleep (one-shot)
static TimerHandle_t sleep_timer = nullptr;

// Flag: au boot, démarre acquisition direct (réveil bouton)
static bool start_acq_on_boot = false;

/* ================== SLEEP ================== */

static void configure_button_wakeup()
{
    // Bouton en pull-up, actif à 0 => réveil quand niveau = 0
    // EXT0 nécessite un RTC GPIO (GPIO7 OK sur ESP32-S3).
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // Optionnel mais recommandé pour éviter flottement en deep sleep
    rtc_gpio_init(BUTTON_PIN);
    rtc_gpio_set_direction(BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(BUTTON_PIN);
    rtc_gpio_pulldown_dis(BUTTON_PIN);

    esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0);
}

static void go_to_deep_sleep()
{
    ESP_LOGI(TAG_APP, "No activity -> entering deep sleep (wake on button)");
    configure_button_wakeup();

    // Petite pause pour laisser les logs sortir (optionnel)
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_deep_sleep_start();
}

static void sleep_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    go_to_deep_sleep();
}

static void arm_sleep_timer()
{
    if (sleep_timer) {
        // (re)déclenche dans 5 minutes
        xTimerStop(sleep_timer, 0);
        xTimerChangePeriod(sleep_timer, pdMS_TO_TICKS(SLEEP_AFTER_IA_MS), 0);
        xTimerStart(sleep_timer, 0);
    }
}

static void disarm_sleep_timer()
{
    if (sleep_timer) {
        xTimerStop(sleep_timer, 0);
    }
}

/* ================== ACQUISITION TASK ================== */

static void acquisition_task(void *arg)
{
    int last_btn = 1;
    bool acquiring = start_acq_on_boot;  // si réveil bouton -> acquisition directe

    if (acquiring) {
        ESP_LOGI(TAG_APP, "Acquisition started (wake-up button)");
        disarm_sleep_timer();
    }

    while (true)
    {
        int btn = gpio_get_level(BUTTON_PIN);
        if (!acquiring && last_btn == 1 && btn == 0) {
            ESP_LOGI(TAG_APP, "Acquisition started (button)");
            acquiring = true;

            // éviter de dormir en plein traitement
            disarm_sleep_timer();
        }
        last_btn = btn;

        if (!acquiring) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        float ax, ay, az, gx, gy, gz;
        imu_read_filtered(&ax, &ay, &az, &gx, &gy, &gz);

        if (ai_push_sample(ax, ay, az, gx, gy, gz)) {
            ESP_LOGI(TAG_APP, "AI inference finished");
            acquiring = false;
            // NOTE: on arme le timer de sleep après l’envoi MQTT du résultat IA (dans mqtt_task)
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ================== RESULT TASK ================== */

static void result_task(void *arg)
{
    Move m;
    uint8_t c;

    while (true)
    {
        if (ai_get_result(&m, &c))
        {
            AiResult r{ m, c };
            xQueueOverwrite(ai_queue, &r);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ================== MQTT TASK ================== */

static void mqtt_task(void *arg)
{
    AiResult r{};
    char payload[160];

    while (true)
    {
        xQueueReceive(ai_queue, &r, portMAX_DELAY);

        snprintf(payload, sizeof(payload),
            "{"
              "\"move\":\"%s\","
              "\"confidence\":%u"
            "}",
            MOVE_STR[(uint8_t)r.move],
            r.confidence
        );

        esp_mqtt_client_publish(
            mqtt_get_client(),
            MQTT_TOPIC_CLASS,
            payload,
            0, 1, 0
        );

        // >>> Après envoi du résultat IA : armement timer sleep 5 min
        arm_sleep_timer();
    }
}

/* ================== BATTERY REPORT TASK ================== */

static void heartbeat_task(void *arg)
{
    char payload[160];

    while (true)
    {
        float vbat = battery_read_voltage();
        uint8_t pct = battery_read_percent();

        snprintf(payload, sizeof(payload),
                 "{"
                   "\"battery\":{"
                     "\"voltage\":%.3f,"
                     "\"percent\":%u"
                   "}"
                 "}",
                 vbat,
                 pct);

        esp_mqtt_client_publish(
            mqtt_get_client(),
            MQTT_TOPIC_HEARTBEAT,
            payload,
            0, 1, 0
        );

        vTaskDelay(pdMS_TO_TICKS(BATTERY_REPORT_PERIOD_MS));
    }
}

/* ================== MAIN ================== */

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());

    // Détecte si on vient d’un deep sleep sur bouton
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    start_acq_on_boot = (cause == ESP_SLEEP_WAKEUP_EXT0);

    // WiFi / MQTT
    wifi_init_sta();
    mqtt_init();

    // IMU / IA
    imu_init_hw();
    imu_calibrate();
    ai_init();

    // BUTTON (mode normal)
    gpio_config_t btn{};
    btn.pin_bit_mask = 1ULL << BUTTON_PIN;
    btn.mode = GPIO_MODE_INPUT;
    btn.pull_up_en = GPIO_PULLUP_ENABLE;
    btn.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&btn);

    // ADC battery
    adc1_config_width(BAT_ADC_WIDTH);
    adc1_config_channel_atten((adc1_channel_t)BAT_ADC_CHANNEL, BAT_ADC_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT_1, BAT_ADC_ATTEN, BAT_ADC_WIDTH, 1100, &adc_chars);

    // Queue IA
    ai_queue = xQueueCreate(1, sizeof(AiResult));

    // Timer sleep (one-shot)
    sleep_timer = xTimerCreate(
        "sleep_t",
        pdMS_TO_TICKS(SLEEP_AFTER_IA_MS),
        pdFALSE,        // one-shot
        nullptr,
        sleep_timer_cb
    );

    // Tasks
    xTaskCreate(acquisition_task, "acq",  4096, nullptr, 5, &acquisition_handle);
    xTaskCreate(result_task,      "res",  4096, nullptr, 6, nullptr);
    xTaskCreate(mqtt_task,        "mqtt", 4096, nullptr, 4, nullptr);
    xTaskCreate(heartbeat_task,   "hb",   4096, nullptr, 3, nullptr);

    // Si tu veux dormir “par défaut” tant qu’il n’y a jamais eu de résultat IA,
    // tu peux armer ici aussi. Sinon, le timer ne partira qu’après un résultat IA.
    // arm_sleep_timer();
}
