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
const char* MQTT_TOPIC_HEALTH = "xiao/health";

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