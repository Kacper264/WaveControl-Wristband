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

struct AiResult {
    Move move;
    uint8_t confidence;
};

typedef struct {
    AiResult results[OUTPUT_SIZE];
    uint8_t count;
} AiBatch;

static QueueHandle_t ai_queue;

// Handle de la task d'acquisition (pour la réveiller depuis l'ISR)
static TaskHandle_t s_acq_task = NULL;

// ISR bouton: réveille la task via notification
static void IRAM_ATTR button_isr_handler(void *arg)
{
    (void)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (s_acq_task) {
        vTaskNotifyGiveFromISR(s_acq_task, &xHigherPriorityTaskWoken);
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// Init bouton en interruption (pull-up + front descendant = appui -> niveau bas)
static void button_init_isr(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;      // appui -> 1->0 (si pull-up)
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << (int)BUTTON_PIN;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Service ISR (à installer 1 seule fois dans tout le projet)
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)BUTTON_PIN, button_isr_handler, NULL));
}

// Task: dort (0% CPU) jusqu'à un appui bouton, puis fait l'acquisition jusqu'à fin inference
static void acquisition_task(void *arg)
{
    (void)arg;
    s_acq_task = xTaskGetCurrentTaskHandle();

    while (true)
    {
        // Bloqué ici tant qu'on n'appuie pas sur le bouton (ne tourne pas en fond)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG_APP, "Acquisition started (button)");

        // Anti-rebond simple
        vTaskDelay(pdMS_TO_TICKS(50));
        // Optionnel: vider les notifications accumulées si double appui très rapide
        ulTaskNotifyTake(pdTRUE, 0);

        // power_manager_disarm_sleep(); // si tu veux conserver ton comportement

        // Boucle d'acquisition active jusqu'à ce que ai_push_sample() retourne true
        for (;;)
        {
            float ax, ay, az, gx, gy, gz, mx, my, mz;
            imu_read_raw(&ax, &ay, &az, &gx, &gy, &gz, &mx, &my, &mz);

            ESP_LOGI("AI", "ax=%.3f ay=%.3f az=%.3f gx=%.3f gy=%.3f gz=%.3f",
                     ax, ay, az, gx, gy, gz);

            // IMPORTANT: tu m'as dit que ai_push_sample attend du brut -> on envoie RAW
            if (ai_push_sample(ax, ay, az, gx, gy, gz, mx, my, mz)) {
                ESP_LOGI(TAG_APP, "AI inference finished");
                break; // stop acquisition, retour en attente d'un prochain appui
            }

            vTaskDelay(pdMS_TO_TICKS(20)); // 50 Hz
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
    neopixel_set_pixel(0, 0, 255, 0); // vert d'idle sur la première LED
    neopixel_set_pixel(1, 0, 0, 255);  // bleu d'idle sur la deuxième LED

    ai_queue = xQueueCreate(1, sizeof(AiResult));
    ESP_LOGI(TAG_APP, "AI result queue created");
    button_init_isr();
    xTaskCreate(acquisition_task,   "acq",  4096, nullptr, 5, nullptr);
    xTaskCreate(result_task,        "res",  4096, nullptr, 6, nullptr);
    //xTaskCreate(mqtt_task,          "mqtt", 4096, nullptr, 4, nullptr);
    //xTaskCreate(battery_report_task,"hb",   4096, nullptr, 3, nullptr);
}
