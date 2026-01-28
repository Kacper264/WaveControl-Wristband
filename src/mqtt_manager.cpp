#include <cstring>

extern "C" {
#include "mqtt_manager.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "mqtt_client.h"

#include "strings_constants.h"
}

/* -------------------------------------------------------------------------- */

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_mqtt_client = nullptr;

/* -------------------------------------------------------------------------- */

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)event_base;
    (void)event_id;
    (void)event_data;

    switch (static_cast<esp_mqtt_event_id_t>(event_id))
    {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "Avant connexion broker");
            break;

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connecté");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT déconnecté");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erreur MQTT");
            break;

        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */

extern "C" void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg{};

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;
#else
    mqtt_cfg.uri = MQTT_BROKER_URI;
#endif

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client)
    {
        ESP_LOGE(TAG, "Init MQTT impossible");
        return;
    }

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            s_mqtt_client,
            static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
            mqtt_event_handler,
            nullptr));

    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));

    ESP_LOGI(TAG, "Client MQTT démarré");
}

/* -------------------------------------------------------------------------- */

extern "C" esp_mqtt_client_handle_t mqtt_get_client(void)
{
    return s_mqtt_client;
}
