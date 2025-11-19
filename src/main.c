#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "driver/gpio.h"

#include "strings_constants.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"

static void heartbeat_task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        esp_mqtt_client_handle_t client = mqtt_get_client();
        if (client != NULL)
        {
            // Temps d'utilisation
            int64_t now_us = esp_timer_get_time();
            int64_t now_ms = now_us / 1000;

            // Batterie
            //float battery_v = battery_read_voltage();

            char payload[128];
            snprintf(payload, sizeof(payload),
            "{"
                "\"health\": {"
                "\"uptime_ms\": %lld,"
                "\"battery_level\": %.2f"
                "}"
            "}",
            (long long)now_ms,
            100.0f /* battery_v */);

            DEBUG_PRINT("[Heartbeat] Envoi trame de vie MQTT : %s", payload);

            esp_mqtt_client_publish(
                client,
                MQTT_TOPIC_HEALTH,
                payload,
                0,
                1,
                0
            );
        }

        // Attendre 60 secondes
        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
    }
}

void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    wifi_init_sta();
    mqtt_init();

    xTaskCreate(heartbeat_task, "heartbeat_task", 4096, NULL, 5, NULL);
}
