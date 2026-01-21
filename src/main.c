#include <stdio.h>
#include <string.h>
#include <stdbool.h>

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

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#define ACQ_TASK_STACK    2048
#define ACQ_TASK_PRIO     6

#define AI_TASK_STACK     8192
#define AI_TASK_PRIO      5

#define MQTT_TASK_STACK   2048
#define MQTT_TASK_PRIO    4

static const char *TAG = "APP";

/* -------------------------------------------------------------------------- */
/* IA / Mouvement                                                             */
/* -------------------------------------------------------------------------- */

#define NB_CLASSES 6

/* ⚠️ ORDRE STRICT (identique à la sortie IA) */
typedef enum
{
    MOVE_CIRCLE_LEFT = 0,
    MOVE_CIRCLE_RIGHT,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_UP
} move_t;

/* Table en FLASH (zéro RAM) */
static const char *move_str_table[NB_CLASSES] = {
    "circle_left",
    "circle_right",
    "down",
    "left",
    "right",
    "up"
};

/* -------------------------------------------------------------------------- */
/* Types                                                                      */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint16_t sample;     /* Donnée brute acquisition */
} acq_data_t;

typedef struct
{
    move_t  move;        /* Mouvement détecté */
    uint8_t confidence;  /* Conservée en interne (non envoyée) */
} ai_result_t;

/* -------------------------------------------------------------------------- */
/* Primitives FreeRTOS                                                        */
/* -------------------------------------------------------------------------- */

static EventGroupHandle_t app_evt;
static QueueHandle_t acq_queue;
static QueueHandle_t ai_queue;

#define EVT_ACQ_TRIGGER BIT0

/* -------------------------------------------------------------------------- */
/* Tâche acquisition (placeholder bouton)                                     */
/* -------------------------------------------------------------------------- */

static void acquisition_task(void *pvParameters)
{
    (void)pvParameters;
    acq_data_t data;

    while (1)
    {
        /* Attente déclenchement (bouton plus tard) */
        xEventGroupWaitBits(
            app_evt,
            EVT_ACQ_TRIGGER,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );

        /* Acquisition minimale */
        data.sample = 123;   /* Placeholder */

        xQueueOverwrite(acq_queue, &data);
    }
}

/* -------------------------------------------------------------------------- */
/* Tâche IA : inference + max + mapping                                       */
/* -------------------------------------------------------------------------- */

static void ai_task(void *pvParameters)
{
    (void)pvParameters;

    acq_data_t data;
    ai_result_t result;

    float scores[NB_CLASSES];   /* Buffer IA local (stack uniquement) */

    while (1)
    {
        xQueueReceive(acq_queue, &data, portMAX_DELAY);

        /* -------- Inference IA (EXEMPLE) -------- */
        /* À remplacer par ton vrai modèle */
        scores[0] = 0.12f;  // circle_left
        scores[1] = 0.05f;  // circle_right
        scores[2] = 0.08f;  // down
        scores[3] = 0.10f;  // left
        scores[4] = 0.55f;  // right
        scores[5] = 0.10f;  // up

        /* -------- Recherche du max -------- */
        uint8_t max_idx = 0;
        float max_val = scores[0];

        for (uint8_t i = 1; i < NB_CLASSES; i++)
        {
            if (scores[i] > max_val)
            {
                max_val = scores[i];
                max_idx = i;
            }
        }

        /* -------- Mapping mouvement -------- */
        result.move = (move_t)max_idx;
        result.confidence = (uint8_t)(max_val * 100.0f); /* interne */

        xQueueOverwrite(ai_queue, &result);
    }
}

/* -------------------------------------------------------------------------- */
/* Tâche MQTT : envoi UNIQUEMENT du mouvement                                 */
/* -------------------------------------------------------------------------- */

static void mqtt_task(void *pvParameters)
{
    (void)pvParameters;

    ai_result_t result;
    char payload[32];   /* Ultra léger */

    while (1)
    {
        xQueueReceive(ai_queue, &result, portMAX_DELAY);

        /* Payload = mouvement uniquement */
        snprintf(payload, sizeof(payload),
                 "%s",
                 move_str_table[result.move]);

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
/* Point d'entrée                                                             */
/* -------------------------------------------------------------------------- */

void app_main(void)
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

    /* Primitives FreeRTOS */
    app_evt   = xEventGroupCreate();
    acq_queue = xQueueCreate(1, sizeof(acq_data_t));
    ai_queue  = xQueueCreate(1, sizeof(ai_result_t));

    /* Création des tâches */
    xTaskCreate(acquisition_task, "acq_task",  ACQ_TASK_STACK,  NULL, ACQ_TASK_PRIO,  NULL);
    xTaskCreate(ai_task,          "ai_task",   AI_TASK_STACK,   NULL, AI_TASK_PRIO,   NULL);
    xTaskCreate(mqtt_task,        "mqtt_task", MQTT_TASK_STACK,NULL, MQTT_TASK_PRIO,NULL);

    /* Déclenchement TEST (remplacé par bouton GPIO plus tard) */
    xEventGroupSetBits(app_evt, EVT_ACQ_TRIGGER);
}
