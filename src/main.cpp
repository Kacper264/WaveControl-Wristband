#include <cstdio>
#include <cstdint>

#include "drivers/imu.h"
#include "ia/ia.h"
#include "ota/ota.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"          // <-- uptime timestamp
#include "net/mqtt_manager.h"
#include "net/wifi_manager.h"
#include "common/strings_constants.h"
}

#define TAG_APP "APP"
#define BUTTON_PIN GPIO_NUM_7

// ---- Periods (modifiable) ----
#define HEARTBEAT_PERIOD_MS       (60 * 1000)        // 1 min
#define BATTERY_TEST_PERIOD_MS    (5 * 60 * 1000)    // 5 min

// ---- Topics (adapte si besoin) ----
// Si tu as déjà ces constantes ailleurs, supprime ces #define.
#ifndef MQTT_TOPIC_HEARTBEAT
#define MQTT_TOPIC_HEARTBEAT "device/heartbeat"
#endif

/* ================== UTILS ================== */

static uint64_t get_uptime_ms()
{
    return (uint64_t)(esp_timer_get_time() / 1000ULL); // µs -> ms
}

/* ================== STRUCT ================== */

struct AiResult {
    Move move;
    uint8_t confidence;
    uint64_t timestamp_ms; // uptime depuis boot
};

/* ================== RTOS ================== */

static QueueHandle_t ai_queue;
static TaskHandle_t acquisition_handle = nullptr;

/* ================== ACQUISITION TASK ================== */

static void acquisition_task(void *arg)
{
    int last_btn = 1;
    bool acquiring = false;

    while (true)
    {
        // ---- Trigger bouton ----
        int btn = gpio_get_level(BUTTON_PIN);
        if (!acquiring && last_btn == 1 && btn == 0) {
            ESP_LOGI(TAG_APP, "Acquisition started (button)");
            acquiring = true;
        }
        last_btn = btn;

        // ---- Trigger périodique (notification FreeRTOS) ----
        // Non bloquant : si une notif est présente, on démarre une acquisition.
        uint32_t notified = ulTaskNotifyTake(pdTRUE, 0);
        if (!acquiring && notified > 0) {
            ESP_LOGI(TAG_APP, "Acquisition started (battery test)");
            acquiring = true;
        }

        if (!acquiring) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        float ax, ay, az, gx, gy, gz;
        imu_read_filtered(&ax, &ay, &az, &gx, &gy, &gz);

        if (ai_push_sample(ax, ay, az, gx, gy, gz)) {
            ESP_LOGI(TAG_APP, "AI inference finished");
            acquiring = false;
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
            AiResult r{
                m,
                c,
                get_uptime_ms()
            };
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

        // JSON avec timestamp (uptime) pour tracer la durée de vie batterie
        snprintf(payload, sizeof(payload),
            "{"
              "\"move\":\"%s\","
              "\"confidence\":%u,"
              "\"uptime_ms\":%llu"
            "}",
            MOVE_STR[(uint8_t)r.move],
            r.confidence,
            (unsigned long long)r.timestamp_ms
        );

        esp_mqtt_client_publish(
            mqtt_get_client(),
            MQTT_TOPIC_CLASS,
            payload,
            0, 1, 0
        );
    }
}

/* ================== HEARTBEAT TASK ================== */

static void heartbeat_task(void *arg)
{
    char payload[128];

    while (true)
    {
        // Uptime dans la trame de vie aussi (pratique côté serveur)
        snprintf(payload, sizeof(payload),
                 "{"
                   "\"alive\":true,"
                   "\"uptime_ms\":%llu"
                 "}",
                 (unsigned long long)get_uptime_ms());

        esp_mqtt_client_publish(
            mqtt_get_client(),
            MQTT_TOPIC_HEARTBEAT,
            payload,
            0, 1, 0
        );

        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
}

/* ================== BATTERY AI TEST TASK ================== */

static void battery_ai_task(void *arg)
{
    while (true)
    {
        // Déclenche une acquisition/inférence IA (sans bouton).
        // Le timestamp sera ajouté au moment où le résultat IA est prêt (result_task),
        // puis publié immédiatement par mqtt_task.
        if (acquisition_handle != nullptr) {
            xTaskNotifyGive(acquisition_handle);
        }

        vTaskDelay(pdMS_TO_TICKS(BATTERY_TEST_PERIOD_MS));
    }
}

/* ================== MAIN ================== */

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init_sta();
    mqtt_init();

    // ---- IMU ----
    imu_init_hw();
    imu_calibrate();

    // ---- IA ----
    ai_init();

    // ---- BUTTON ----
    gpio_config_t btn{};
    btn.pin_bit_mask = 1ULL << BUTTON_PIN;
    btn.mode = GPIO_MODE_INPUT;
    btn.pull_up_en = GPIO_PULLUP_ENABLE;
    btn.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&btn);

    // ---- QUEUE ----
    ai_queue = xQueueCreate(1, sizeof(AiResult));

    // ---- TASKS ----
    xTaskCreate(acquisition_task, "acq",  4096, nullptr, 5, &acquisition_handle);
    xTaskCreate(result_task,      "res",  4096, nullptr, 6, nullptr);
    xTaskCreate(mqtt_task,        "mqtt", 4096, nullptr, 4, nullptr);

    // Nouvelles tâches
    xTaskCreate(heartbeat_task,   "hb",   4096, nullptr, 3, nullptr);
    xTaskCreate(battery_ai_task,  "batt", 4096, nullptr, 3, nullptr);
}
