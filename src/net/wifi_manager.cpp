#include "include.h"

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

static const char *TAG = "wifi";

/* -------------------------------------------------------------------------- */
/* État Wi-Fi                                                                 */
/* -------------------------------------------------------------------------- */

/* Indique si le module est actuellement connecté au réseau */
static volatile bool s_wifi_connected = false;

/* -------------------------------------------------------------------------- */
/* Event handler                                                              */
/* -------------------------------------------------------------------------- */

/*
 * Gestionnaire d'événements Wi-Fi et réseau IP.
 * Cette fonction est appelée par ESP-IDF lorsqu'un événement
 * lié au Wi-Fi se produit (démarrage, déconnexion, obtention d'IP).
 */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            // Le Wi-Fi en mode station démarre → tentative de connexion
            ESP_LOGI(TAG, "STA start → connexion");
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            // Perte de connexion → tentative de reconnexion automatique
            ESP_LOGW(TAG, "Déconnecté → reconnexion");
            s_wifi_connected = false;
            esp_wifi_connect();
        }
    }
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        // Adresse IP obtenue via DHCP
        ESP_LOGI(TAG, "IP obtenue");
        s_wifi_connected = true;
    }
}

/* -------------------------------------------------------------------------- */
/* Initialisation Wi-Fi                                                       */
/* -------------------------------------------------------------------------- */

/*
 * Initialise le Wi-Fi en mode station et configure
 * les identifiants du réseau.
 */
extern "C" void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Création de l'interface réseau Wi-Fi station
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    // Enregistrement des gestionnaires d'événements Wi-Fi
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        nullptr,
        nullptr));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        wifi_event_handler,
        nullptr,
        nullptr));

    wifi_config_t wifi_cfg{};
    /* zero-init propre C++ */

    // Copie du SSID et du mot de passe
    strncpy(reinterpret_cast<char*>(wifi_cfg.sta.ssid),
            WIFI_SSID,
            sizeof(wifi_cfg.sta.ssid) - 1);

    strncpy(reinterpret_cast<char*>(wifi_cfg.sta.password),
            WIFI_PASS,
            sizeof(wifi_cfg.sta.password) - 1);

    // Paramètres de sécurité
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_MODE;
    wifi_cfg.sta.pmf_cfg.capable = WIFI_PMF_CAPABLE;
    wifi_cfg.sta.pmf_cfg.required = WIFI_PMF_REQUIRED;

    // Configuration du mode station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    // Démarrage du Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialisé");
}

/* -------------------------------------------------------------------------- */
/* État de connexion                                                          */
/* -------------------------------------------------------------------------- */

/*
 * Permet aux autres modules de savoir si le Wi-Fi est connecté.
 */
extern "C" bool wifi_is_connected(void)
{
    return s_wifi_connected;
}