#include "common/strings_constants.h"

/* -------------------------------------------------------------------------- */
/* Application                                                                */
/* -------------------------------------------------------------------------- */

const char* TAG_APP = "WaveControl";

/* -------------------------------------------------------------------------- */
/* Wi-Fi                                                                      */
/* -------------------------------------------------------------------------- */

const char* WIFI_SSID = "......";
const char* WIFI_PASS = "......";

/* -------------------------------------------------------------------------- */
/* MQTT                                                                       */
/* -------------------------------------------------------------------------- */

const char* MQTT_BROKER_URI = "mqtt://192.168.1.164:1883";

/* Topics généraux */
const char* MQTT_TOPIC_HEALTH = "home/wristband/battery";

/* Topics domotique */
const char* MQTT_TOPIC_CLASS = "home/wristband/move";

/* -------------------------------------------------------------------------- */
/* TASK CONFIG                                                                */
/* -------------------------------------------------------------------------- */

const uint16_t ACQ_TASK_STACK  = 4096;
const uint16_t AI_TASK_STACK   = 8192;
const uint16_t MQTT_TASK_STACK = 2048;

const UBaseType_t ACQ_TASK_PRIO  = 6;
const UBaseType_t AI_TASK_PRIO   = 5;
const UBaseType_t MQTT_TASK_PRIO = 4;

/* -------------------------------------------------------------------------- */
/* POWER / SLEEP CONFIG                                                       */
/* -------------------------------------------------------------------------- */

// Bouton utilisé pour démarrer acquisition + réveil deep sleep
const gpio_num_t BUTTON_PIN = GPIO_NUM_7;

// 5 minutes après publication MQTT du résultat IA -> deep sleep
const uint32_t SLEEP_AFTER_IA_MS = 5 * 60 * 1000;

/* -------------------------------------------------------------------------- */
/* BATTERY CONFIG                                                             */
/* -------------------------------------------------------------------------- */

// Envoi niveau batterie toutes les 60 secondes
const uint32_t BATTERY_REPORT_PERIOD_MS = 60 * 1000;

// Channel ADC réellement connecté à la mesure batterie
const adc1_channel_t BAT_ADC_CHANNEL = ADC1_CHANNEL_0;

// Atténuation / résolution ADC
const adc_atten_t BAT_ADC_ATTEN = ADC_ATTEN_DB_11;
const adc_bits_width_t BAT_ADC_WIDTH = ADC_WIDTH_BIT_12;

// Ratio du pont diviseur
const float BAT_DIVIDER_RATIO = 0.5f;

// Plage LiPo approximative (pour %)
const float VBAT_MIN = 3.30f;
const float VBAT_MAX = 4.20f;

/* -------------------------------------------------------------------------- */
/* MOVES                                                                      */
/* -------------------------------------------------------------------------- */

const char *MOVE_STR[OUTPUT_SIZE] = {
    "circle_left",
    "circle_right",
    "down",
    "left",
    "right",
    "up"
};
