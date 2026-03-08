// Standard library
#include <cstdint>
#include <cstdio>
#include <cstring>

// ESP-IDF / FreeRTOS C headers
extern "C" {
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_adc_cal.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_https_ota.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "led_strip.h"
}

// ESP-IDF / C++ headers
#include "esp_heap_caps.h"

// Projet
#include "battery/battery.h"
#include "common/strings_constants.h"
#include "drivers/imu.h"
#include "drivers/neopixel.h"
#include "ia/ia.h"
#include "ia/model_data.h"
#include "ia/run_model.h"
#include "net/mqtt_manager.h"
#include "net/wifi_manager.h"
#include "ota/ota.h"
#include "power/power.h"
#include "task/task.h"

// TensorFlow Lite Micro
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"