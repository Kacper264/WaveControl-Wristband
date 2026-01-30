#include <cstring>

extern "C" {
#include "net/mqtt_manager.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "mqtt_client.h"

#include "common/strings_constants.h"
}

#include "ota/ota.h"

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

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "Avant connexion broker");
            break;

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connecté");

            // 👇 abonnement OTA ici
            esp_mqtt_client_subscribe(
                s_mqtt_client,
                "device/ota",
                1
            );
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT déconnecté");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erreur MQTT");
            break;

        case MQTT_EVENT_DATA:
        {
            // ----- extraction propre topic + payload -----
            char topic[event->topic_len + 1];
            char payload[event->data_len + 1];

            memcpy(topic, event->topic, event->topic_len);
            memcpy(payload, event->data, event->data_len);

            topic[event->topic_len] = '\0';
            payload[event->data_len] = '\0';

            ESP_LOGI(TAG, "MQTT RX [%s] -> %s", topic, payload);

            // ----- hook OTA -----
            ota_handle_mqtt(topic, payload);

            break;
        }

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
            (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
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
