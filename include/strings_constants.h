#pragma once

#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Debug                                                                      */
/* -------------------------------------------------------------------------- */

#define DEBUG 0

#if DEBUG
    #define DEBUG_PRINT(...) ESP_LOGI(TAG_APP, __VA_ARGS__)
#else
    #define DEBUG_PRINT(...)
#endif

/* -------------------------------------------------------------------------- */
/* Application                                                                */
/* -------------------------------------------------------------------------- */

extern const char *TAG_APP;

/* -------------------------------------------------------------------------- */
/* Wi-Fi                                                                      */
/* -------------------------------------------------------------------------- */

extern const char *WIFI_SSID;
extern const char *WIFI_PASS;

/* -------------------------------------------------------------------------- */
/* MQTT                                                                       */
/* -------------------------------------------------------------------------- */

extern const char *MQTT_BROKER_URI;
extern const char *MQTT_TOPIC_CLASS;
extern const char *MQTT_TOPIC_HEALTH;

#ifdef __cplusplus
}
#endif
