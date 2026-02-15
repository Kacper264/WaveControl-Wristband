#include "task/task.h"

#include <cstdio>
#include <cstdint>

#include "drivers/imu.h"
#include "ia/ia.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "net/mqtt_manager.h"
#include "common/strings_constants.h"
}

#include "battery/battery.h"
#include "power/power.h"

#define TAG_APP "APP"

#ifndef BUTTON_PIN
#define BUTTON_PIN GPIO_NUM_7
#endif

#ifndef MQTT_TOPIC_HEARTBEAT
#define MQTT_TOPIC_HEARTBEAT "device/heartbeat"
#endif

#ifndef BATTERY_REPORT_PERIOD_MS
#define BATTERY_REPORT_PERIOD_MS (60 * 1000)
#endif

// 5 minutes après publish MQTT IA
#ifndef SLEEP_AFTER_IA_MS
#define SLEEP_AFTER_IA_MS (5 * 60 * 1000)
#endif

struct AiResult {
    Move move;
    uint8_t confidence;
};

static QueueHandle_t ai_queue;
static TaskHandle_t acquisition_handle = nullptr;

static void acquisition_task(void *arg)
{
    (void)arg;
    int last_btn = 1;

    bool acquiring = power_manager_woke_from_button();
    if (acquiring) {
        ESP_LOGI(TAG_APP, "Acquisition started (wake-up button)");
        power_manager_disarm_sleep();
    }

    while (true)
    {
        int btn = gpio_get_level((gpio_num_t)BUTTON_PIN);
        if (!acquiring && last_btn == 1 && btn == 0) {
            ESP_LOGI(TAG_APP, "Acquisition started (button)");
            acquiring = true;
            power_manager_disarm_sleep();
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
            // le timer de sleep est armé après publication MQTT du résultat IA (mqtt_task)
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void result_task(void *arg)
{
    (void)arg;
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

static void mqtt_task(void *arg)
{
    (void)arg;
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

        // Arme le deep sleep dans 5 minutes
        power_manager_arm_sleep();
    }
}

static void battery_report_task(void *arg)
{
    (void)arg;
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

void app_tasks_start(void)
{
    ai_queue = xQueueCreate(1, sizeof(AiResult));

    xTaskCreate(acquisition_task,   "acq",  4096, nullptr, 5, &acquisition_handle);
    xTaskCreate(result_task,        "res",  4096, nullptr, 6, nullptr);
    xTaskCreate(mqtt_task,          "mqtt", 4096, nullptr, 4, nullptr);
    xTaskCreate(battery_report_task,"hb",   4096, nullptr, 3, nullptr);
}
