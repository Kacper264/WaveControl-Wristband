#include "drivers/imu.h"
#include "ia/ia.h"

extern "C" {
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "net/wifi_manager.h"
#include "net/mqtt_manager.h"
#include "common/strings_constants.h"
}

#include "battery/battery.h"
#include "power/power.h"
#include "task/task.h"

#define TAG_APP "APP"

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init_sta();
    mqtt_init();
    
    ESP_LOGI(TAG_APP, "I2C IMU starting...");
    vTaskDelay(pdMS_TO_TICKS(200));
    imu_init_hw();
    //imu_calibrate();
    ESP_LOGI(TAG_APP, "I2C IMU initialized and calibrated");
    ai_init();

    gpio_config_t btn{};
    btn.pin_bit_mask = 1ULL << BUTTON_PIN;
    btn.mode = GPIO_MODE_INPUT;
    btn.pull_up_en = GPIO_PULLUP_ENABLE;
    btn.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&btn);

    battery_init();
    power_manager_init();

    app_tasks_start();

    ESP_LOGI(TAG_APP, "System started");
}
