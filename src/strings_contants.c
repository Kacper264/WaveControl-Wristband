#include "strings_constants.h"

/* -------------------------------------------------------------------------- */
/* Application                                                                */
/* -------------------------------------------------------------------------- */

const char *TAG_APP = "WaveControl";

/* -------------------------------------------------------------------------- */
/* Wi-Fi                                                                      */
/* -------------------------------------------------------------------------- */

const char *WIFI_SSID = "......";
const char *WIFI_PASS = "......";

/* -------------------------------------------------------------------------- */
/* MQTT                                                                       */
/* -------------------------------------------------------------------------- */

const char *MQTT_BROKER_URI = "mqtt://192.168.1.164:1883";

/* Topics généraux */
const char *MQTT_TOPIC_SUB    = "xiao/sub";
const char *MQTT_TOPIC_HEALTH = "xiao/health";

/* Topics domotique */
const char *MQTT_TOPIC_BTN        = "home/button";
const char *MQTT_TOPIC_LUM        = "home/lum_X/set";
const char *MQTT_TOPIC_BRIGHT     = "home/lum_X/brightness/set";
const char *MQTT_TOPIC_COLOR      = "home/lum_X/color/set";
const char *MQTT_TOPIC_HS_COLOR   = "home/lum_X/hs_color/set";
const char *MQTT_TOPIC_COLOR_TEMP = "home/lum_X/color_temp/set";
const char *MQTT_TOPIC_PRISE      = "home/prise_X/set";
