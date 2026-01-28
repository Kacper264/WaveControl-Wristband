#include <cstdio>
#include <cstring>
#include <cstdint>
#include "ia/run_model.h"
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "strings_constants.h"
}

/* -------------------------------------------------------------------------- */
/* CONFIG                                                                     */
/* -------------------------------------------------------------------------- */

constexpr uint16_t ACQ_TASK_STACK  = 4096;
constexpr uint16_t AI_TASK_STACK   = 8192;
constexpr uint16_t MQTT_TASK_STACK = 2048;

constexpr UBaseType_t ACQ_TASK_PRIO  = 6;
constexpr UBaseType_t AI_TASK_PRIO   = 5;
constexpr UBaseType_t MQTT_TASK_PRIO = 4;

static const char *TAG = "APP";

/* -------------------------------------------------------------------------- */
/* IA PARAMS                                                                  */
/* -------------------------------------------------------------------------- */

constexpr uint16_t SEQ_LEN   = 100;
constexpr uint16_t FEATURES = 6;
constexpr uint16_t INPUT_SIZE  = SEQ_LEN * FEATURES;
constexpr uint16_t OUTPUT_SIZE = 6;

/* -------------------------------------------------------------------------- */
/* MOVES                                                                      */
/* -------------------------------------------------------------------------- */

enum class Move : uint8_t
{
    CircleLeft = 0,
    CircleRight,
    Down,
    Left,
    Right,
    Up
};

static constexpr const char *MOVE_STR[OUTPUT_SIZE] = {
    "circle_left",
    "circle_right",
    "down",
    "left",
    "right",
    "up"
};

/* -------------------------------------------------------------------------- */
/* TYPES                                                                      */
/* -------------------------------------------------------------------------- */

struct AcqData
{
    float ax, ay, az;
    float gx, gy, gz;
};

struct AiResult
{
    Move    move;
    uint8_t confidence;
};

/* -------------------------------------------------------------------------- */
/* RTOS                                                                       */
/* -------------------------------------------------------------------------- */

static QueueHandle_t acq_queue;
static QueueHandle_t ai_queue;

/* -------------------------------------------------------------------------- */
/* BUFFERS                                                                    */
/* -------------------------------------------------------------------------- */

static float imu_buffer[INPUT_SIZE];
static float output_buffer[OUTPUT_SIZE];
static uint16_t sample_index = 0;

/* -------------------------------------------------------------------------- */
/* ACQUISITION TASK                                                           */
/* -------------------------------------------------------------------------- */

static void acquisition_task(void *arg)
{
    (void)arg;

    AcqData data{};

    const TickType_t period = pdMS_TO_TICKS(20);  // 50Hz

    while (true)
    {
        /* --------------------------------------------------
           👉 Remplace ici par ta lecture IMU réelle
           -------------------------------------------------- */

        data.ax = 0.1f;
        data.ay = 0.2f;
        data.az = 0.3f;
        data.gx = 0.01f;
        data.gy = 0.02f;
        data.gz = 0.03f;

        xQueueSend(acq_queue, &data, 0);

        vTaskDelay(period);
    }
}

/* -------------------------------------------------------------------------- */
/* AI TASK                                                                    */
/* -------------------------------------------------------------------------- */

static void ai_task(void *arg)
{
    (void)arg;

    AcqData data{};
    AiResult result{};

    ESP_LOGI(TAG, "AI task started");

    if (!init_model())
    {
        ESP_LOGE(TAG, "MODEL INIT FAILED");
        vTaskDelete(nullptr);
    }

    while (true)
    {
        xQueueReceive(acq_queue, &data, portMAX_DELAY);

        imu_buffer[sample_index++] = data.ax;
        imu_buffer[sample_index++] = data.ay;
        imu_buffer[sample_index++] = data.az;
        imu_buffer[sample_index++] = data.gx;
        imu_buffer[sample_index++] = data.gy;
        imu_buffer[sample_index++] = data.gz;

        if (sample_index >= INPUT_SIZE)
        {
            sample_index = 0;

            if (run_inference(imu_buffer, output_buffer))
            {
                uint8_t max_idx = 0;
                float max_val = output_buffer[0];

                for (uint8_t i = 1; i < OUTPUT_SIZE; i++)
                {
                    if (output_buffer[i] > max_val)
                    {
                        max_val = output_buffer[i];
                        max_idx = i;
                    }
                }

                result.move = static_cast<Move>(max_idx);
                result.confidence = (uint8_t)(max_val * 100.0f);

                ESP_LOGI(TAG, "AI → %s (%.2f)",
                         MOVE_STR[max_idx], max_val);

                xQueueOverwrite(ai_queue, &result);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* MQTT TASK                                                                  */
/* -------------------------------------------------------------------------- */

static void mqtt_task(void *arg)
{
    (void)arg;

    AiResult result{};
    char payload[32];

    while (true)
    {
        xQueueReceive(ai_queue, &result, portMAX_DELAY);

        uint8_t idx = static_cast<uint8_t>(result.move);

        snprintf(payload, sizeof(payload), "%s", MOVE_STR[idx]);

        esp_mqtt_client_handle_t client = mqtt_get_client();
        if (client)
        {
            ESP_LOGI(TAG, "MQTT → %s", payload);

            esp_mqtt_client_publish(
                client,
                MQTT_TOPIC_CLASS,
                payload,
                0,
                2,
                0
            );
        }
    }
}

/* -------------------------------------------------------------------------- */
/* MAIN                                                                       */
/* -------------------------------------------------------------------------- */

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    wifi_init_sta();
    mqtt_init();

    acq_queue = xQueueCreate(5, sizeof(AcqData));
    ai_queue  = xQueueCreate(1, sizeof(AiResult));

    xTaskCreate(acquisition_task, "acq_task",
                ACQ_TASK_STACK, nullptr, ACQ_TASK_PRIO, nullptr);

    xTaskCreate(ai_task, "ai_task",
                AI_TASK_STACK, nullptr, AI_TASK_PRIO, nullptr);

    xTaskCreate(mqtt_task, "mqtt_task",
                MQTT_TASK_STACK, nullptr, MQTT_TASK_PRIO, nullptr);
}
