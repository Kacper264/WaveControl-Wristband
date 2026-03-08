#include "include.h"

/*
 * Lance la procédure OTA à partir d'une version donnée.
 * Construit l'URL du firmware puis démarre la mise à jour via HTTPS.
 */
static void start_ota(const char *version)
{
    char url[256];

    // Construction de l'URL du firmware à télécharger
    snprintf(url, sizeof(url), OTA_BASE_URL, "firmware_v%s.bin", version);

    // Configuration du client HTTP utilisé pour télécharger le firmware
    esp_http_client_config_t http_cfg = {};
    http_cfg.url = url;
    http_cfg.timeout_ms = 15000;

    // Configuration OTA avec le client HTTP
    esp_https_ota_config_t ota_cfg = {};
    ota_cfg.http_config = &http_cfg;

    // Lance la mise à jour OTA
    esp_err_t ret = esp_https_ota(&ota_cfg);

    // Si la mise à jour est réussie, redémarre l'ESP pour lancer le nouveau firmware
    if (ret == ESP_OK) {
        esp_restart();
    }
}

/*
 * Callback appelé lors de la réception d'un message MQTT.
 * Si le topic correspond au topic OTA, la mise à jour est déclenchée.
 * Le payload contient la version du firmware à télécharger.
 */
void ota_handle_mqtt(const char *topic, const char *payload)
{
    // Ignore les messages qui ne concernent pas l'OTA
    if (strcmp(topic, MQTT_TOPIC_OTA) != 0)
        return;

    // Lance la mise à jour OTA avec la version reçue
    start_ota(payload);
}