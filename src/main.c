#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

#include "driver/gpio.h"

// --------- CONFIG UTILISATEUR ---------
#define WIFI_SSID       "Freebox-35CC49"
#define WIFI_PASS       "Mama1980"

#define MQTT_BROKER_URI "mqtt://192.168.1.164:1883"
#define MQTT_TOPIC_BTN  "xiao/test"

#define BTN_GPIO        0      // à adapter selon ton câblage
#define BTN_ACTIVE_LOW  1      // 1 si pull-up + bouton vers GND
// -------------------------------------

static const char *TAG = "WaveControl";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool wifi_connected = false;

// ================= WIFI =================

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "WiFi déconnecté, reconnexion...");
        wifi_connected = false;
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "WiFi connecté, IP obtenue");
        wifi_connected = true;
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            // SSID & PASS copiés tels quels
        },
    };

    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA initialisé, connexion en cours...");
}

// ================= MQTT =================

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;
    (void)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connecté au broker MQTT");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT déconnecté");
            break;
        default:
            break;
    }
}


static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = { 0 };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    // ESP-IDF 5.x+
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;
#else
    // ESP-IDF 4.x (ancien style)
    mqtt_cfg.uri = MQTT_BROKER_URI;
#endif

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}


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

// ============== TASK PRINCIPALE ==============

static void button_task(void *pvParameters)
{
    bool last_state = false;

    while (1)
    {
        bool pressed = button_is_pressed();

        // front descendant / montant selon config
        if (pressed && !last_state)
        {
            ESP_LOGI(TAG, "[Bouton] Appuyé -> envoi MQTT");

            if (mqtt_client != NULL)
            {
                esp_mqtt_client_publish(
                    mqtt_client,
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

    ESP_LOGI(TAG, "Démarrage WaveControl (ESP-IDF)");

    wifi_init_sta();
    mqtt_init();
    button_init();

    // Task de gestion du bouton
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
}
