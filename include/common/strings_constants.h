#pragma once

#include "esp_log.h"
#include <cstdint>
#include "freertos/FreeRTOS.h"


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

/* -------------------------------------------------------------------------- */
/* TASK CONFIG                                                                */
/* -------------------------------------------------------------------------- */

extern const uint16_t ACQ_TASK_STACK;
extern const uint16_t AI_TASK_STACK;
extern const uint16_t MQTT_TASK_STACK;

extern const UBaseType_t ACQ_TASK_PRIO;
extern const UBaseType_t AI_TASK_PRIO;
extern const UBaseType_t MQTT_TASK_PRIO;

/* -------------------------------------------------------------------------- */
/* IA PARAMS                                                                  */
/* -------------------------------------------------------------------------- */

#define SEQ_LEN     100
#define FEATURES    6
#define INPUT_SIZE  (SEQ_LEN * FEATURES)
#define OUTPUT_SIZE 6

/* -------------------------------------------------------------------------- */
/* MOVES                                                                      */
/* -------------------------------------------------------------------------- */

enum class Move : uint8_t
{
    CircleLeft = 0,
    CircleRight,
    Down,
    Left,
    Right,
    Up
};

extern const char *MOVE_STR[];

#ifdef __cplusplus
}
#endif
