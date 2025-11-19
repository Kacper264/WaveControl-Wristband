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
extern const char *MQTT_TOPIC_SUB;
extern const char *MQTT_TOPIC_HEALTH;
extern const char *MQTT_TOPIC_BTN;
extern const char *MQTT_TOPIC_LUM;
extern const char *MQTT_TOPIC_BRIGHT;
extern const char *MQTT_TOPIC_COLOR;
extern const char *MQTT_TOPIC_HS_COLOR;
extern const char *MQTT_TOPIC_COLOR_TEMP;
extern const char *MQTT_TOPIC_PRISE;
  
