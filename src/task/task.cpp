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


struct AiResult {
    Move move;
    uint8_t confidence;
};

typedef struct {
    AiResult results[OUTPUT_SIZE];
    uint8_t count;
} AiBatch;

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
    AiBatch batch;
    char payload[160];

    while (true)
    {
        xQueueReceive(ai_queue, &batch, portMAX_DELAY);

        // Chercher l'index du max
        uint8_t best_idx = 0;

        for (uint8_t i = 1; i < OUTPUT_SIZE; i++)
        {
            if (batch.results[i].confidence > batch.results[best_idx].confidence)
            {
                best_idx = i;
            }
        }

        // Construire le payload : seulement le mouvement
        snprintf(payload, sizeof(payload),
            "%s",
            MOVE_STR[best_idx]
        );

        // Publish MQTT
        esp_mqtt_client_publish(
            mqtt_get_client(),
            MQTT_TOPIC_CLASS,
            payload,
            0, 1, 0
        );

        // Deep sleep
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

        bool sect = is_on_sector(vbat);

        snprintf(payload, sizeof(payload),
                 "{"
                   "\"bat_lvl\":%u,"
                   "\"sect\":%s"
                 "}",
                 pct,
                 sect ? "true" : "false"
        );

        esp_mqtt_client_publish(
            mqtt_get_client(),
            MQTT_TOPIC_HEALTH,
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
