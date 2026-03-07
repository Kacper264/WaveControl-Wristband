#pragma once

#include "include.h"

/* -------------------------------------------------------------------------- */
/* IA PARAMS                                                                  */
/* -------------------------------------------------------------------------- */

#define SEQ_LEN     100
#define FEATURES    6
#define INPUT_SIZE  (SEQ_LEN * FEATURES)
#define OUTPUT_SIZE 5

/* -------------------------------------------------------------------------- */
/* MOVES                                                                      */
/* -------------------------------------------------------------------------- */

enum class Move : uint8_t
{
    CircleLeft = 0,
    Down,
    Left,
    Right,
    Up
};

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* GPIO / POWER                                                               */
/* -------------------------------------------------------------------------- */

extern const gpio_num_t BUTTON_PIN;
extern const uint32_t SLEEP_AFTER_IA_MS;

/* -------------------------------------------------------------------------- */
/* WIFI                                                                       */
/* -------------------------------------------------------------------------- */

extern const char *WIFI_SSID;
extern const char *WIFI_PASS;
extern const wifi_auth_mode_t WIFI_AUTH_MODE;
extern const bool WIFI_PMF_CAPABLE;
extern const bool WIFI_PMF_REQUIRED;

/* -------------------------------------------------------------------------- */
/* MQTT / OTA                                                                 */
/* -------------------------------------------------------------------------- */

extern const char *MQTT_BROKER_URI;
extern const char *MQTT_USERNAME;
extern const char *MQTT_PASSWORD;

extern const char *MQTT_TOPIC_HEALTH;
extern const char *MQTT_TOPIC_CLASS;
extern const char *MQTT_TOPIC_OTA;

extern const char *OTA_BASE_URL;

/* -------------------------------------------------------------------------- */
/* TASK CONFIG                                                                */
/* -------------------------------------------------------------------------- */

extern const uint16_t ACQ_TASK_STACK;
extern const uint16_t BATTERY_TASK_STACK;
extern const uint16_t NEOPIXEL_TASK_STACK;

extern const UBaseType_t ACQ_TASK_PRIO;
extern const UBaseType_t BATTERY_TASK_PRIO;
extern const UBaseType_t NEOPIXEL_TASK_PRIO;

extern const char *ACQ_TASK_NAME;
extern const char *BATTERY_TASK_NAME;

extern const uint32_t ACQUISITION_PERIOD_MS;
extern const uint32_t BUTTON_POLL_PERIOD_MS;
extern const uint16_t ACQUISITION_SAMPLE_COUNT;

extern const uint16_t BATTERY_PAYLOAD_SIZE;
extern const uint8_t AI_QUEUE_LENGTH;
extern const uint16_t AI_PAYLOAD_SIZE;

/* -------------------------------------------------------------------------- */
/* BATTERY CONFIG                                                             */
/* -------------------------------------------------------------------------- */

extern const uint32_t BATTERY_REPORT_PERIOD_MS;

extern const adc1_channel_t BAT_ADC_CHANNEL;
extern const adc_atten_t BAT_ADC_ATTEN;
extern const adc_bits_width_t BAT_ADC_WIDTH;

extern const float BAT_DIVIDER_RATIO;
extern const float VBAT_MIN;
extern const float VBAT_MAX;

extern const uint8_t BAT_ADC_SAMPLES;
extern const uint16_t BAT_ADC_STABILIZE_DELAY_US;
extern const uint32_t BAT_ADC_VREF_MV;

extern const float BATTERY_FILTER_ALPHA;
extern const float SECTOR_DETECTION_THRESHOLD;

/* -------------------------------------------------------------------------- */
/* IMU / I2C CONFIG                                                           */
/* -------------------------------------------------------------------------- */

extern const char *TAG_IMU;

extern const gpio_num_t I2C_MASTER_SCL_IO;
extern const gpio_num_t I2C_MASTER_SDA_IO;
extern const i2c_port_t I2C_MASTER_NUM;
extern const uint32_t I2C_MASTER_FREQ_HZ;

extern const uint8_t LSM6_ADDR;
extern const uint8_t LIS3MDL_ADDR;

extern const uint8_t LSM6_CTRL1_XL;
extern const uint8_t LSM6_CTRL2_G;
extern const uint8_t LSM6_CTRL3_C;
extern const uint8_t LSM6_OUTX_L_G;
extern const uint8_t LSM6_OUTX_L_A;
extern const uint8_t LIS3MDL_OUT_X_L;

extern const uint16_t IMU_CALIB_SAMPLES;
extern const uint32_t IMU_CALIB_DELAY_MS;

/* -------------------------------------------------------------------------- */
/* NEOPIXEL CONFIG                                                            */
/* -------------------------------------------------------------------------- */

extern const gpio_num_t NEOPIXEL_GPIO;
extern const uint8_t NEOPIXEL_COUNT;

extern const uint8_t NEOPIXEL_IDLE_R;
extern const uint8_t NEOPIXEL_IDLE_G;
extern const uint8_t NEOPIXEL_IDLE_B;

extern const uint8_t NEOPIXEL_QUEUE_LEN;
extern const uint32_t NEOPIXEL_RMT_RESOLUTION_HZ;

/* -------------------------------------------------------------------------- */
/* STRINGS / LABELS                                                           */
/* -------------------------------------------------------------------------- */

extern const char *MOVE_STR[];

#ifdef __cplusplus
}
#endif

/* -------------------------------------------------------------------------- */
/* IMU RUNTIME DATA                                                           */
/* -------------------------------------------------------------------------- */

extern int ax;
extern int ay;
extern int az;

extern int gx;
extern int gy;
extern int gz;

extern int ax_cal;
extern int ay_cal;
extern int az_cal;

extern int gx_cal;
extern int gy_cal;
extern int gz_cal;