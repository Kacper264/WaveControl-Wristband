#include "common/strings_constants.h"

/* -------------------------------------------------------------------------- */
/* GPIO / POWER                                                               */
/* -------------------------------------------------------------------------- */

const gpio_num_t BUTTON_PIN = GPIO_NUM_7;
const uint32_t SLEEP_AFTER_IA_MS = 5 * 60 * 1000;

/* -------------------------------------------------------------------------- */
/* WIFI                                                                       */
/* -------------------------------------------------------------------------- */

const char *WIFI_SSID = "filrougestation";
const char *WIFI_PASS = "filrougestation";

const wifi_auth_mode_t WIFI_AUTH_MODE = WIFI_AUTH_WPA2_PSK;
const bool WIFI_PMF_CAPABLE = true;
const bool WIFI_PMF_REQUIRED = false;

/* -------------------------------------------------------------------------- */
/* MQTT / OTA                                                                 */
/* -------------------------------------------------------------------------- */

const char *MQTT_BROKER_URI = "mqtt://192.168.50.27:1883";
const char *MQTT_USERNAME = "fil_rouge";
const char *MQTT_PASSWORD = "lolipop";

const char *MQTT_TOPIC_HEALTH = "home/wristband/power";
const char *MQTT_TOPIC_CLASS = "home/wristband/move";
const char *MQTT_TOPIC_OTA = "device/ota";

const char *OTA_BASE_URL = "https://github.com/TON_USER/TON_REPO/releases/latest/download/";

/* -------------------------------------------------------------------------- */
/* TASK CONFIG                                                                */
/* -------------------------------------------------------------------------- */

const uint16_t ACQ_TASK_STACK = 4096;
const uint16_t BATTERY_TASK_STACK = 4096;
const uint16_t NEOPIXEL_TASK_STACK = 3072;

const UBaseType_t ACQ_TASK_PRIO = 6;
const UBaseType_t BATTERY_TASK_PRIO = 3;
const UBaseType_t NEOPIXEL_TASK_PRIO = 2;

const char *ACQ_TASK_NAME = "acq";
const char *BATTERY_TASK_NAME = "hb";

const uint32_t ACQUISITION_PERIOD_MS = 20;
const uint32_t BUTTON_POLL_PERIOD_MS = 10;
const uint16_t ACQUISITION_SAMPLE_COUNT = 101;

const uint16_t BATTERY_PAYLOAD_SIZE = 160;
const uint8_t AI_QUEUE_LENGTH = 1;
const uint16_t AI_PAYLOAD_SIZE = 120;

/* -------------------------------------------------------------------------- */
/* BATTERY CONFIG                                                             */
/* -------------------------------------------------------------------------- */

const uint32_t BATTERY_REPORT_PERIOD_MS = 15 * 1000;

const adc1_channel_t BAT_ADC_CHANNEL = ADC1_CHANNEL_0;
const adc_atten_t BAT_ADC_ATTEN = ADC_ATTEN_DB_12;
const adc_bits_width_t BAT_ADC_WIDTH = ADC_WIDTH_BIT_12;

const float BAT_DIVIDER_RATIO = 2.0f;
const float VBAT_MIN = 3.30f;
const float VBAT_MAX = 4.20f;

const uint8_t BAT_ADC_SAMPLES = 32;
const uint16_t BAT_ADC_STABILIZE_DELAY_US = 200;
const uint32_t BAT_ADC_VREF_MV = 1100;

const float BATTERY_FILTER_ALPHA = 0.2f;
const float SECTOR_DETECTION_THRESHOLD = 0.001f;

/* -------------------------------------------------------------------------- */
/* IMU / I2C CONFIG                                                           */
/* -------------------------------------------------------------------------- */

const char *TAG_IMU = "IMU";

const gpio_num_t I2C_MASTER_SCL_IO = GPIO_NUM_6;
const gpio_num_t I2C_MASTER_SDA_IO = GPIO_NUM_5;
const i2c_port_t I2C_MASTER_NUM = I2C_NUM_0;
const uint32_t I2C_MASTER_FREQ_HZ = 100000;

const uint8_t LSM6_ADDR = 0x6B;
const uint8_t LIS3MDL_ADDR = 0x1E;

const uint8_t LSM6_CTRL1_XL = 0x10;
const uint8_t LSM6_CTRL2_G = 0x11;
const uint8_t LSM6_CTRL3_C = 0x12;
const uint8_t LSM6_OUTX_L_G = 0x22;
const uint8_t LSM6_OUTX_L_A = 0x28;
const uint8_t LIS3MDL_OUT_X_L = 0x28;

const uint16_t IMU_CALIB_SAMPLES = 500;
const uint32_t IMU_CALIB_DELAY_MS = 5;

/* -------------------------------------------------------------------------- */
/* NEOPIXEL CONFIG                                                            */
/* -------------------------------------------------------------------------- */

const gpio_num_t NEOPIXEL_GPIO = GPIO_NUM_2;
const uint8_t NEOPIXEL_COUNT = 2;

const uint8_t NEOPIXEL_IDLE_R = 0;
const uint8_t NEOPIXEL_IDLE_G = 0;
const uint8_t NEOPIXEL_IDLE_B = 0;

const uint8_t NEOPIXEL_QUEUE_LEN = 8;
const uint32_t NEOPIXEL_RMT_RESOLUTION_HZ = 10 * 1000 * 1000;

/* -------------------------------------------------------------------------- */
/* STRINGS / LABELS                                                           */
/* -------------------------------------------------------------------------- */

const char *MOVE_STR[OUTPUT_SIZE] = {
    "circle_left",
    "down",
    "left",
    "right",
    "up"
};

/* -------------------------------------------------------------------------- */
/* IMU RUNTIME DATA                                                           */
/* -------------------------------------------------------------------------- */

int ax = 0;
int ay = 0;
int az = 0;

int gx = 0;
int gy = 0;
int gz = 0;

int ax_cal = 0;
int ay_cal = 0;
int az_cal = 0;

int gx_cal = 0;
int gy_cal = 0;
int gz_cal = 0;