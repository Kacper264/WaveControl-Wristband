#include "include.h"

#define TAG_APP "APP"

/* Buffers utilisés pour l'acquisition IMU et la sortie du modèle IA */
static float imu_buffer[INPUT_SIZE];
static float output_buffer[OUTPUT_SIZE];

/* Position courante d'écriture dans le buffer IMU */
static uint16_t sample_index = 0;

/*
 * Résultat simplifié d'une prédiction IA.
 * Peut être utilisé pour stocker un mouvement et sa confiance.
 */
struct AiResult {
    Move move;
    uint8_t confidence;
};

/*
 * Lot de résultats IA.
 * Prévu pour transporter plusieurs résultats si nécessaire.
 */
typedef struct {
    AiResult results[OUTPUT_SIZE];
    uint8_t count;
} AiBatch;

/* Ressources internes des tâches */
static QueueHandle_t ai_queue;
static TaskHandle_t acquisition_handle = nullptr;

/*
 * Tâche d'acquisition :
 * - attend un appui bouton
 * - lit une séquence complète de données IMU
 * - lance l'inférence IA
 * - traite le résultat
 */
static void acquisition_task(void *arg)
{
    const TickType_t xFrequency = pdMS_TO_TICKS(ACQUISITION_PERIOD_MS); // période d'échantillonnage
    TickType_t xLastWakeTime;
    (void)arg;

    int last_btn = 1;

    // Si le réveil vient du bouton, on démarre directement une acquisition
    bool acquiring = power_manager_woke_from_button();
    if (acquiring) {
        ESP_LOGI(TAG_APP, "Acquisition started (wake-up button)");
        xLastWakeTime = xTaskGetTickCount();
        power_manager_disarm_sleep();
    }

    while (true)
    {
        int btn = gpio_get_level((gpio_num_t)BUTTON_PIN);

        // Détection du front descendant du bouton
        if (!acquiring && last_btn == 1 && btn == 0) {
            printf("#GESTE_START\n");
            acquiring = true;
            xLastWakeTime = xTaskGetTickCount();
            power_manager_disarm_sleep();
        }
        last_btn = btn;

        // Attente active légère tant qu'aucune acquisition n'est demandée
        if (!acquiring) {
            vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_PERIOD_MS));
            continue;
        }

        if (acquiring) {
            // Acquisition d'une fenêtre complète de mesures IMU
            for (int i = 0; i < ACQUISITION_SAMPLE_COUNT; i++) {
                xLastWakeTime = xTaskGetTickCount();
                imu_read_raw();

                // Remplissage du buffer d'entrée du modèle
                if (sample_index + FEATURES <= INPUT_SIZE) {
                    imu_buffer[sample_index++] = ax;
                    imu_buffer[sample_index++] = ay;
                    imu_buffer[sample_index++] = az;
                    imu_buffer[sample_index++] = gx;
                    imu_buffer[sample_index++] = gy;
                    imu_buffer[sample_index++] = gz;
                }
                else
                {
                    sample_index = 0;
                }

                // Sortie série pour debug / acquisition de dataset
                printf("%d;%d;%d;%d;%d;%d\n", ax, ay, az, gx, gy, gz);

                // Maintient une période d'échantillonnage fixe
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
            }

            acquiring = false;
            printf("#GESTE_END\n");

            // Lancement de l'inférence puis traitement du résultat
            ai_run_inference(&imu_buffer[0], &output_buffer[0]);
            process_inference_result(&output_buffer[0]);
        }
    }
}

/*
 * Tâche de télémétrie batterie :
 * mesure la batterie, construit un payload JSON
 * puis publie l'état sur MQTT à intervalle régulier.
 */
static void battery_report_task(void *arg)
{
    (void)arg;
    char payload[BATTERY_PAYLOAD_SIZE];

    while (true)
    {
        neopixel_blink_blue(0, 500, 500);
        float vbat = battery_read_voltage();
        uint8_t pct = battery_read_percent();

        bool sect = is_on_sector(vbat);
        neopixel_blink_blue(0, 500, 500);
        snprintf(payload, sizeof(payload),
                 "{"
                   "\"bat_lvl\":%u,"
                   "\"sect\":%s"
                 "}",
                 pct,
                 sect ? "true" : "false"
        );
        if (sect == true && pct < 100) {
            neopixel_blink_green_loading(1, 200, 200);
        }
        esp_mqtt_client_publish(
            mqtt_get_client(),
            MQTT_TOPIC_HEALTH,
            payload,
            0, 2, 1
        );
        neopixel_blink_blue(0, 500, 500);
        vTaskDelay(pdMS_TO_TICKS(BATTERY_REPORT_PERIOD_MS));
    }
}

/*
 * Initialise les périphériques liés aux tâches applicatives
 * puis démarre les tâches FreeRTOS principales.
 */
void app_tasks_start(void)
{
    neopixel_init(NEOPIXEL_GPIO, NEOPIXEL_COUNT);
    //neopixel_set_pixel(0, 0, 255, 0);
    neopixel_set_pixel(1, 0, 0, 255);

    ai_queue = xQueueCreate(AI_QUEUE_LENGTH, sizeof(AiResult));
    ESP_LOGI(TAG_APP, "AI result queue created");

    xTaskCreate(acquisition_task, ACQ_TASK_NAME, ACQ_TASK_STACK, nullptr, ACQ_TASK_PRIO, &acquisition_handle);
    xTaskCreate(battery_report_task, BATTERY_TASK_NAME, BATTERY_TASK_STACK, nullptr, BATTERY_TASK_PRIO, nullptr);
}