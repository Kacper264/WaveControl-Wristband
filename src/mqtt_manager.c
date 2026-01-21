#include <string.h>

#include "mqtt_manager.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "mqtt_client.h"

#include "strings_constants.h"

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

static const char *TAG = "mqtt";

/* -------------------------------------------------------------------------- */
/* Client MQTT global                                                         */
/* -------------------------------------------------------------------------- */

static esp_mqtt_client_handle_t s_mqtt_client = NULL;

/* -------------------------------------------------------------------------- */
/* Callback événements MQTT                                                   */
/* -------------------------------------------------------------------------- */

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)event_base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "Avant connexion au broker");
            break;

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connecté au broker");

            ESP_LOGI(TAG, "Souscription au topic : %s", MQTT_TOPIC_SUB);
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_SUB, 1);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Déconnecté du broker");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Message reçu");
            ESP_LOGD(TAG, "Topic : %.*s", event->topic_len, event->topic);
            ESP_LOGD(TAG, "Payload : %.*s", event->data_len, event->data);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erreur MQTT");
            break;

        default:
            /* Autres événements ignorés */
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* Initialisation MQTT                                                        */
/* -------------------------------------------------------------------------- */

void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = { 0 };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;
#else
    mqtt_cfg.uri = MQTT_BROKER_URI;
#endif

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL)
    {
        ESP_LOGE(TAG, "Impossible d'initialiser le client MQTT");
        return;
    }

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            s_mqtt_client,
            ESP_EVENT_ANY_ID,
            mqtt_event_handler,
            NULL));

    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));

    ESP_LOGI(TAG, "Client MQTT démarré");
}

/* -------------------------------------------------------------------------- */
/* Accesseur client MQTT                                                      */
/* -------------------------------------------------------------------------- */

esp_mqtt_client_handle_t mqtt_get_client(void)
{
    return s_mqtt_client;
}
