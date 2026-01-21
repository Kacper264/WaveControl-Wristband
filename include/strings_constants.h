#pragma once

// Active les logs si DEBUG est défini
#define DEBUG 0

#if DEBUG
    #define DEBUG_PRINT(...) ESP_LOGI(TAG_APP, __VA_ARGS__)
#else
    #define DEBUG_PRINT(...)
#endif

extern const char *TAG_APP;

// WiFi
extern const char *WIFI_SSID;
extern const char *WIFI_PASS;

// MQTT
extern const char *MQTT_BROKER_URI;
extern const char *MQTT_TOPIC_CLASS;
extern const char *MQTT_TOPIC_HEALTH;