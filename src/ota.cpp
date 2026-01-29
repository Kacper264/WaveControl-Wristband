#include "ota.h"

#include <string.h>
#include <stdio.h>

#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"

#define TAG_OTA "OTA"

#define OTA_TOPIC "device/ota"
#define OTA_BASE_URL "https://github.com/TON_USER/TON_REPO/releases/latest/download/"

static void start_ota(const char *version)
{
    char url[256];

    snprintf(url, sizeof(url),
             OTA_BASE_URL "firmware_v%s.bin",
             version);

    ESP_LOGI(TAG_OTA, "OTA URL: %s", url);

    esp_http_client_config_t http_cfg = {};
    http_cfg.url = url;
    http_cfg.timeout_ms = 15000;

    esp_https_ota_config_t ota_cfg = {};
    ota_cfg.http_config = &http_cfg;

    esp_err_t ret = esp_https_ota(&ota_cfg);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG_OTA, "OTA success → reboot");
        esp_restart();
    } else {
        ESP_LOGE(TAG_OTA, "OTA failed");
    }
}


void ota_handle_mqtt(const char *topic, const char *payload)
{
    if (strcmp(topic, OTA_TOPIC) != 0)
        return;

    ESP_LOGI(TAG_OTA, "OTA request version: %s", payload);

    start_ota(payload);
}
