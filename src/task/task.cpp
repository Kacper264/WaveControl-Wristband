#include "task/task.h"

#include <cstdio>
#include <cstdint>

#include "drivers/imu.h"
#include "drivers/neopixel.h"
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

#define NEOPIXEL_GPIO 8
#define NEOPIXEL_COUNT 2

#define TAG_APP "APP"
static float imu_buffer[INPUT_SIZE];
static float output_buffer[OUTPUT_SIZE];

static uint16_t sample_index = 0;

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

/*
static void acquisition_task(void *arg)
{
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 20ms = 50Hz
    TickType_t xLastWakeTime; 
    (void)arg;
    int last_btn = 1;

    bool acquiring = power_manager_woke_from_button();
    if (acquiring) {
        ESP_LOGI(TAG_APP, "Acquisition started (wake-up button)");
        xLastWakeTime = xTaskGetTickCount(); // Initialise ici
        power_manager_disarm_sleep();
    }

    while (true)
    {
        int btn = gpio_get_level((gpio_num_t)BUTTON_PIN);
        
        // Détection de l'appui sur le bouton
        if (!acquiring && last_btn == 1 && btn == 0) {
            ESP_LOGI(TAG_APP, "Acquisition started (button)");
            acquiring = true;
            // IMPORTANT : On synchronise le chrono au moment de l'appui
            xLastWakeTime = xTaskGetTickCount(); 
            power_manager_disarm_sleep();
        }
        last_btn = btn;

        if (!acquiring) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        //imu_read_raw(&ax, &ay, &az, &gx, &gy, &gz, &mx, &my, &mz);
        imu_read_raw();
        ESP_LOGI("AI", "Sample ax=%.3f ay=%.3f az=%.3f gx=%.3f gy=%.3f gz=%.3f",

         ax, ay, az, gx, gy, gz);
        // Optionnel : Réduis le log pour ne pas ralentir la tâche
        //ESP_LOGD("AI", "Sample ax=%.3f gx=%.3f", ax, gx);

        if (ai_push_sample(ax, ay, az, gx, gy, gz)) {
            ESP_LOGI(TAG_APP, "AI inference finished");
            acquiring = false;
        }

        // Attend exactement le temps restant pour atteindre les 20ms
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

*/

static void acquisition_task(void *arg)
{
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 20ms = 50Hz
    TickType_t xLastWakeTime; 
    (void)arg;
    int last_btn = 1;

    bool acquiring = power_manager_woke_from_button();
    if (acquiring) {
        ESP_LOGI(TAG_APP, "Acquisition started (wake-up button)");
        xLastWakeTime = xTaskGetTickCount(); // Initialise ici
        power_manager_disarm_sleep();
    }

    while (true)
    {
        int btn = gpio_get_level((gpio_num_t)BUTTON_PIN);
        
        // Détection de l'appui sur le bouton
        if (!acquiring && last_btn == 1 && btn == 0) {
            //printf("IMU initialized and calibrated : %f, %f, %f,%f,%f,%f \n", ax_cal, ay_cal, az_cal, gx_cal, gy_cal, gz_cal);
            printf("#GESTE_START\n");
            //printf("ax;ay;az;gx;gy;gz\n");
            acquiring = true;
            // IMPORTANT : On synchronise le chrono au moment de l'appui
            xLastWakeTime = xTaskGetTickCount(); 
            power_manager_disarm_sleep();
        }
        last_btn = btn;

        if (!acquiring) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if(acquiring) {
            // Lire les données de l'IMU
            for(int i = 0; i < 101; i++) {
                xLastWakeTime = xTaskGetTickCount(); 
                //imu_read_raw(&ax, &ay, &az, &gx, &gy, &gz, &mx, &my, &mz);
                imu_read_raw();

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
                    sample_index=0;
                }
                
                printf("%d;%d;%d;%d;%d;%d\n", ax, ay, az, gx, gy, gz);
                // Optionnel : Réduis le log pour ne pas ralentir la tâche
                //ESP_LOGD("AI", "Sample ax=%.3f gx=%.3f", ax, gx);

        // Attend exactement le temps restant pour atteindre les 20ms
            vTaskDelayUntil(&xLastWakeTime, xFrequency);

            }
            acquiring = false;
            printf("#GESTE_END\n");
            ai_run_inference(& imu_buffer[0], & output_buffer[0]);
            process_inference_result(& output_buffer[0]);


        }    
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
                // ESP_LOGI(TAG_APP, "New best move: %s with confidence %u%%",
                //          MOVE_STR[best_idx],
                //          batch.results[i].confidence);
            }
        }

        // Construire le payload : seulement le mouvement
        snprintf(payload, sizeof(payload),
            "%s",
            MOVE_STR[best_idx]
        );

        //neopixel_blink_blue(120, 80);

        // Publish MQTT
        esp_mqtt_client_publish(
            mqtt_get_client(),
            MQTT_TOPIC_CLASS,
            payload,
            0, 1, 0
        );

        // Deep sleep
        //power_manager_arm_sleep();
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

        //neopixel_blink_blue(120, 80);

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
    neopixel_init(NEOPIXEL_GPIO, NEOPIXEL_COUNT);
    neopixel_set_pixel(0, 0, 40, 0); // vert d'idle sur la première LED
    neopixel_set_pixel(1, 0, 0, 60);  // bleu d'idle sur la deuxième LED

    ai_queue = xQueueCreate(1, sizeof(AiResult));
    ESP_LOGI(TAG_APP, "AI result queue created");
    xTaskCreate(acquisition_task,   "acq",  4096, nullptr, 5, &acquisition_handle);
    xTaskCreate(result_task,        "res",  4096, nullptr, 6, nullptr);
    xTaskCreate(mqtt_task,          "mqtt", 4096, nullptr, 4, nullptr);
    xTaskCreate(battery_report_task,"hb",   4096, nullptr, 3, nullptr);
}
