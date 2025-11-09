#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/gpio.h"

#include "strings_constants.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"

// ============== GPIO BOUTON ==============

static void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = BTN_ACTIVE_LOW ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = BTN_ACTIVE_LOW ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

static bool button_is_pressed(void)
{
    int level = gpio_get_level(BTN_GPIO);

    if (BTN_ACTIVE_LOW)
        return (level == 0);
    else
        return (level == 1);
}

// ============== TASK BOUTON ==============

static void button_task(void *pvParameters)
{
    (void)pvParameters;

    bool last_state = false;

    while (1)
    {
        bool pressed = button_is_pressed();

        if (pressed && !last_state)
        {
            ESP_LOGI(TAG_APP, "[Bouton] Appuyé -> envoi MQTT");

            esp_mqtt_client_handle_t client = mqtt_get_client();
            if (client != NULL)
            {
                esp_mqtt_client_publish(
                    client,
                    MQTT_TOPIC_BTN,
                    "Bouton appuyé !",
                    0,
                    1,
                    0
                );
            }

            vTaskDelay(pdMS_TO_TICKS(500)); // anti-rebond
        }

        last_state = pressed;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============== app_main =====================

void app_main(void)
{
    // Init NVS (obligatoire avant WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG_APP, "Démarrage WaveControl (ESP-IDF)");

    wifi_init_sta();
    mqtt_init();
    button_init();

    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
}
