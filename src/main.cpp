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
#include "net/mqtt_manager.h"
#include "net/wifi_manager.h"
#include "common/strings_constants.h"
}

#define TAG_APP "APP"
#define BUTTON_PIN GPIO_NUM_7

/* ================== STRUCT ================== */

struct AiResult {
    Move move;
    uint8_t confidence;
};

/* ================== RTOS ================== */

static QueueHandle_t ai_queue;

/* ================== ACQUISITION TASK ================== */

static void acquisition_task(void *arg)
{
    int last_btn = 1;
    bool acquiring = false;

    while (true)
    {
        int btn = gpio_get_level(BUTTON_PIN);

        if (!acquiring && last_btn == 1 && btn == 0) {
            ESP_LOGI(TAG_APP, "Acquisition started");
            acquiring = true;
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

/* ================== RESULT TASK ================== */

static void result_task(void *arg)
{
    Move m;
    uint8_t c;

    while (true)
    {
        if (ai_get_result(&m, &c))
        {
            AiResult r{m, c};
            xQueueOverwrite(ai_queue, &r);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ================== MQTT TASK ================== */

static void mqtt_task(void *arg)
{
    AiResult r{};
    char payload[32];

    while (true)
    {
        xQueueReceive(ai_queue, &r, portMAX_DELAY);

        snprintf(payload, sizeof(payload),
                 "%s", MOVE_STR[(uint8_t)r.move]);

        esp_mqtt_client_publish(
            mqtt_get_client(),
            MQTT_TOPIC_CLASS,
            payload,
            0, 1, 0
        );
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
    xTaskCreate(acquisition_task, "acq", 4096, nullptr, 5, nullptr);
    xTaskCreate(result_task,      "res", 4096, nullptr, 6, nullptr);
    xTaskCreate(mqtt_task,        "mqtt",4096, nullptr, 4, nullptr);
}
