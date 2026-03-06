#include "ia/ia.h"
#include "ia/run_model.h"
#include "esp_log.h"
#include "net/mqtt_manager.h"
#include "common/strings_constants.h"
#define TAG_AI "AI"


static uint16_t sample_index = 0;
static Move last_move;
static uint8_t last_conf;
static bool result_ready = false;

void ai_init()
{
    if (!init_model()) {
        ESP_LOGE(TAG_AI, "MODEL INIT FAILED");
        return;
    }
    sample_index = 0;
    result_ready = false;
}

void ai_run_inference(float* imu_buffer, float* output_buffer)
{
    run_inference(imu_buffer, output_buffer);
}

void process_inference_result(float* output_buffer)
{
    char payload[120];

    // ===== Trouver la meilleure classe =====
    uint8_t best = 0;
    for (int i = 1; i < OUTPUT_SIZE; i++)
        if (output_buffer[i] > output_buffer[best])
            best = i;

    last_move = (Move)best;
    last_conf = (uint8_t)(output_buffer[best] * 100);
    result_ready = true;

    // ===== PRINT COMPLET =====
    ESP_LOGI(TAG_AI, "===== AI RESULT =====");

    for (int i = 0; i < OUTPUT_SIZE; i++)
    {
        float pct = output_buffer[i] * 100.0f;

        ESP_LOGI(TAG_AI, "%-12s : %.2f%% %s",
                 MOVE_STR[i],
                 pct,
                 (i == best) ? "<-- BEST" : "");

        if (i == best) {
            snprintf(payload, sizeof(payload),
                "%s",
                MOVE_STR[i]
            );
            // ESP_LOGI(TAG_AI, "Publishing MQTT: %s", payload);
            //neopixel_blink_blue(120, 80);

            // Publish MQTT
            esp_mqtt_client_publish(
                mqtt_get_client(),
                MQTT_TOPIC_CLASS,
                payload,
                0, 1, 0
            );
        }
    }

    ESP_LOGI(TAG_AI, "Prediction: %s (%.2f%%)",
             MOVE_STR[best],
             output_buffer[best] * 100.0f);

    ESP_LOGI(TAG_AI, "====================");
}

/*
bool ai_push_sample(
    float ax, float ay, float az,
    float gx, float gy, float gz, 
    float mx, float my, float mz)
{
    imu_buffer[sample_index++] = gx;
    imu_buffer[sample_index++] = gy;
    imu_buffer[sample_index++] = gz;
    imu_buffer[sample_index++] = ax;
    imu_buffer[sample_index++] = ay;
    imu_buffer[sample_index++] = az;
    imu_buffer[sample_index++] = mx;
    imu_buffer[sample_index++] = my;
    imu_buffer[sample_index++] = mz;

    if (sample_index < INPUT_SIZE)
        return false;

    sample_index = 0;

    run_inference(imu_buffer, output_buffer);

    // ===== Trouver la meilleure classe =====
    uint8_t best = 0;
    for (int i = 1; i < OUTPUT_SIZE; i++)
        if (output_buffer[i] > output_buffer[best])
            best = i;

    last_move = (Move)best;
    last_conf = (uint8_t)(output_buffer[best] * 100);
    result_ready = true;

    // ===== PRINT COMPLET =====
    ESP_LOGI(TAG_AI, "===== AI RESULT =====");

    for (int i = 0; i < OUTPUT_SIZE; i++)
    {
        float pct = output_buffer[i] * 100.0f;

        ESP_LOGI(TAG_AI, "%-12s : %.2f%% %s",
                 MOVE_STR[i],
                 pct,
                 (i == best) ? "<-- BEST" : "");
    }

    ESP_LOGI(TAG_AI, "Prediction: %s (%.2f%%)",
             MOVE_STR[best],
             output_buffer[best] * 100.0f);

    ESP_LOGI(TAG_AI, "====================");

    return true;
}
*/

bool ai_get_result(Move *move, uint8_t *confidence)
{
    if (!result_ready)
        return false;

    *move = last_move;
    *confidence = last_conf;
    result_ready = false;
    return true;
}
