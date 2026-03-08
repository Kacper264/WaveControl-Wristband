#include "include.h"

#define TAG_AI "AI"

/* Dernier résultat d'inférence conservé en mémoire */
static Move last_move;
static uint8_t last_conf;
static bool result_ready = false;

/*
 * Initialise le module IA et charge le modèle.
 */
void ai_init()
{
    if (!init_model()) {
        ESP_LOGE(TAG_AI, "MODEL INIT FAILED");
        return;
    }

    result_ready = false;
}

/*
 * Lance l'inférence sur la fenêtre IMU fournie.
 * Le résultat brut est écrit dans output_buffer.
 */
void ai_run_inference(float* imu_buffer, float* output_buffer)
{
    run_inference(imu_buffer, output_buffer);
}

/*
 * Analyse la sortie du modèle :
 * - recherche la classe la plus probable
 * - mémorise le dernier résultat
 * - affiche les scores
 * - publie le mouvement détecté sur MQTT
 */
void process_inference_result(float* output_buffer)
{
    char payload[AI_PAYLOAD_SIZE];

    // Recherche de la classe avec le score le plus élevé
    uint8_t best = 0;
    for (int i = 1; i < OUTPUT_SIZE; i++)
        if (output_buffer[i] > output_buffer[best])
            best = i;

    // Sauvegarde du dernier résultat
    last_move = (Move)best;
    last_conf = (uint8_t)(output_buffer[best] * 100);
    result_ready = true;

    ESP_LOGI(TAG_AI, "===== AI RESULT =====");

    // Affichage détaillé des probabilités pour chaque mouvement
    for (int i = 0; i < OUTPUT_SIZE; i++)
    {
        float pct = output_buffer[i] * 100.0f;

        ESP_LOGI(TAG_AI, "%-12s : %.2f%% %s",
                 MOVE_STR[i],
                 pct,
                 (i == best) ? "<-- BEST" : "");

        // Publication MQTT du mouvement prédit
        neopixel_blink_blue(0, 200, 200);
        if (i == best) {
            snprintf(payload, sizeof(payload), "%s", MOVE_STR[i]);

            esp_mqtt_client_publish(
                mqtt_get_client(),
                MQTT_TOPIC_CLASS,
                payload,
                0, 2, 1
            );
        }

    }

    ESP_LOGI(TAG_AI, "Prediction: %s (%.2f%%)",
             MOVE_STR[best],
             output_buffer[best] * 100.0f);

    ESP_LOGI(TAG_AI, "====================");
}