#pragma once

#include "esp_log.h"
#include <cstdint>
#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "driver/adc.h"

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
/* GPIO                                                                       */
/* -------------------------------------------------------------------------- */
extern const gpio_num_t BUTTON_PIN;

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

// Ton topic existant
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
/* POWER / SLEEP CONFIG                                                       */
/* -------------------------------------------------------------------------- */

// Bouton existant dans ton code
extern const gpio_num_t BUTTON_PIN;

// Temps avant deep sleep après publish MQTT du résultat IA
extern const uint32_t SLEEP_AFTER_IA_MS;

// ========================
// Timing (ms)
// ========================
extern const uint32_t BATTERY_REPORT_PERIOD_MS;

/* -------------------------------------------------------------------------- */
/* BATTERY CONFIG                                                             */
/* -------------------------------------------------------------------------- */

// Période d’envoi du niveau batterie (télémétrie)
extern const uint32_t BATTERY_REPORT_PERIOD_MS;

// ADC batterie
extern const adc1_channel_t  BAT_ADC_CHANNEL;
extern const adc_atten_t     BAT_ADC_ATTEN;
extern const adc_bits_width_t BAT_ADC_WIDTH;

// Diviseur & conversion
extern const float BAT_DIVIDER_RATIO;
extern const float VBAT_MIN;
extern const float VBAT_MAX;

/* -------------------------------------------------------------------------- */
/* IA PARAMS                                                                  */
/* -------------------------------------------------------------------------- */

#define SEQ_LEN     100
#define FEATURES    9
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

// ========================
// MQTT
// ========================


