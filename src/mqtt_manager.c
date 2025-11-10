#include "mqtt_manager.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"

#include "strings_constants.h"

static esp_mqtt_client_handle_t s_mqtt_client = NULL;

/**
 * @brief Callback principal pour les événements MQTT
 */
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
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG_APP, "[MQTT] Connecté au broker");

            // --> S'abonner à un topic dès la connexion
            ESP_LOGI(TAG_APP, "[MQTT] Souscription au topic: %s", MQTT_TOPIC_SUB);
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_SUB, 0);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG_APP, "[MQTT] Déconnecté du broker");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG_APP, "[MQTT] Abonnement confirmé (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG_APP, "[MQTT] Désabonnement confirmé (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG_APP, "[MQTT] Message publié (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG_APP, "[MQTT] Message reçu !");
            ESP_LOGI(TAG_APP, "Topic : %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG_APP, "Données : %.*s", event->data_len, event->data);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG_APP, "[MQTT] Erreur MQTT détectée");
            break;

        default:
            ESP_LOGD(TAG_APP, "[MQTT] Événement non géré : %d", event_id);
            break;
    }
}

/**
 * @brief Initialise et démarre le client MQTT
 */
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

    ESP_LOGI(TAG_APP, "[MQTT] Initialisation terminée");
}

/**
 * @brief Retourne le handle du client MQTT (utilisé par d'autres modules)
 */
esp_mqtt_client_handle_t mqtt_get_client(void)
{
    return s_mqtt_client;
}
