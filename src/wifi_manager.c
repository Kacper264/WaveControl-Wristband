#include <string.h>
#include <stdbool.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"

#include "wifi_manager.h"
#include "strings_constants.h"

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

static const char *TAG = "wifi";

/* -------------------------------------------------------------------------- */
/* État Wi-Fi (volatile = modifié depuis callbacks)                           */
/* -------------------------------------------------------------------------- */

static volatile bool s_wifi_connected = false;

/* -------------------------------------------------------------------------- */
/* Gestion des événements Wi-Fi / IP                                          */
/* -------------------------------------------------------------------------- */

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
            ESP_LOGI(TAG, "STA start → connexion");
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            ESP_LOGW(TAG, "Déconnecté → reconnexion");
            s_wifi_connected = false;
            esp_wifi_connect();
        }
    }
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "IP obtenue");
        s_wifi_connected = true;
    }
}

/* -------------------------------------------------------------------------- */
/* Initialisation Wi-Fi                                                       */
/* -------------------------------------------------------------------------- */

void wifi_init_sta(void)
{
    /* Init réseau (idempotent si déjà appelé) */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    /* Init Wi-Fi */
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    /* Enregistrement handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL,
        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        wifi_event_handler,
        NULL,
        NULL));

    /* Configuration STA */
    wifi_config_t wifi_cfg = { 0 };

    /* Copie sécurisée + terminaison garantie */
    strncpy((char *)wifi_cfg.sta.ssid,
            WIFI_SSID,
            sizeof(wifi_cfg.sta.ssid) - 1);

    strncpy((char *)wifi_cfg.sta.password,
            WIFI_PASS,
            sizeof(wifi_cfg.sta.password) - 1);

    /* Sécurité minimale correcte */
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    /* Mode STA uniquement (RAM minimale) */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialisé");
}

/* -------------------------------------------------------------------------- */
/* Accesseur état Wi-Fi                                                       */
/* -------------------------------------------------------------------------- */

bool wifi_is_connected(void)
{
    return s_wifi_connected;
}