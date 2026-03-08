#include "include.h"

/* -------------------------------------------------------------------------- */

static const char *TAG = "MQTT";

/* Handle global du client MQTT */
static esp_mqtt_client_handle_t s_mqtt_client = nullptr;

/* -------------------------------------------------------------------------- */

/*
 * Gestionnaire d'événements MQTT.
 * Cette fonction est appelée par ESP-IDF pour tous les événements
 * liés au client MQTT (connexion, réception de messages, erreurs, etc.).
 */
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

            // Abonnement aux topics nécessaires (ex: OTA)
            esp_mqtt_client_subscribe(
                s_mqtt_client,
                MQTT_TOPIC_OTA,
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
            // ----- extraction du topic et du payload -----
            char topic[event->topic_len + 1];
            char payload[event->data_len + 1];

            memcpy(topic, event->topic, event->topic_len);
            memcpy(payload, event->data, event->data_len);

            topic[event->topic_len] = '\0';
            payload[event->data_len] = '\0';

            ESP_LOGI(TAG, "MQTT RX [%s] -> %s", topic, payload);

            // Transmission du message au module OTA
            ota_handle_mqtt(topic, payload);

            break;
        }

        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */

/*
 * Initialise et démarre le client MQTT.
 * Configure l'URI du broker et les identifiants d'authentification.
 */
extern "C" void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg{};
    
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;
    mqtt_cfg.credentials.username = MQTT_USERNAME;
    mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
#else
    mqtt_cfg.uri = MQTT_BROKER_URI;
    mqtt_cfg.username = MQTT_USERNAME;
    mqtt_cfg.password = MQTT_PASSWORD;
#endif

    // Création du client MQTT
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client)
    {
        ESP_LOGE(TAG, "Init MQTT impossible");
        return;
    }

    // Enregistrement du gestionnaire d'événements
    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            s_mqtt_client,
            (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
            mqtt_event_handler,
            nullptr));

    // Démarrage du client
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));

    ESP_LOGI(TAG, "Client MQTT démarré (auth activée)");
}

/* -------------------------------------------------------------------------- */

/*
 * Retourne le handle du client MQTT pour permettre
 * aux autres modules de publier des messages.
 */
extern "C" esp_mqtt_client_handle_t mqtt_get_client(void)
{
    return s_mqtt_client;
}