#include "mqtt_manager.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"

#include "strings_constants.h"

static esp_mqtt_client_handle_t s_mqtt_client = NULL;

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_BEFORE_CONNECT:
            DEBUG_PRINT("[MQTT] Avant connexion au broker");
            break;

        case MQTT_EVENT_CONNECTED:
            DEBUG_PRINT("[MQTT] Connecté au broker");


            DEBUG_PRINT("[MQTT] Souscription au topic: %s", MQTT_TOPIC_SUB);
            break;

        case MQTT_EVENT_DATA:
            DEBUG_PRINT("[MQTT] Message reçu !");
            break;

        default:
            /* Unhandled events can be ignored or logged for debugging */
            break;
    }
}


void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = { 0 };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;
#else
    mqtt_cfg.uri = MQTT_BROKER_URI;
#endif

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    // Enregistre le callback global
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        s_mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL));

    // Démarre le client
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));

}

/**
 * @brief Retourne le handle du client MQTT (utilisé par d'autres modules)
 */
esp_mqtt_client_handle_t mqtt_get_client(void)
{
    return s_mqtt_client;
}
