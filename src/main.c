#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "strings_constants.h"

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#define HEARTBEAT_PERIOD_MS   (60 * 1000)
#define HEARTBEAT_TASK_STACK  4096
#define HEARTBEAT_TASK_PRIO   5

static const char *TAG = "heartbeat";

/* -------------------------------------------------------------------------- */
/* Tâche heartbeat                                                             */
/* -------------------------------------------------------------------------- */

static void heartbeat_task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        esp_mqtt_client_handle_t client = mqtt_get_client();

        if (client != NULL)
        {
            int64_t uptime_ms = esp_timer_get_time() / 1000;
            float battery_level = 100.0f; // Valeur simulée

            char payload[128];

            snprintf(payload, sizeof(payload),
                     "{"
                        "\"health\":{"
                            "\"uptime_ms\":%lld,"
                            "\"battery_level\":%.2f"
                        "}"
                     "}",
                     (long long)uptime_ms,
                     battery_level);

            ESP_LOGD(TAG, "Envoi heartbeat MQTT : %s", payload);

            esp_mqtt_client_publish(
                client,
                MQTT_TOPIC_HEALTH,
                payload,
                0,      // longueur auto
                1,      // QoS
                0       // non retained
            );
        }

        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
}

/* -------------------------------------------------------------------------- */
/* Point d'entrée                                                              */
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

    xTaskCreate(
        heartbeat_task,
        "heartbeat_task",
        HEARTBEAT_TASK_STACK,
        NULL,
        HEARTBEAT_TASK_PRIO,
        NULL
    );
}
