#include "include.h"

#define TAG "APP"

/*
 * Point d'entrée principal de l'application ESP-IDF.
 */
extern "C" void app_main()
{
    // Initialise la mémoire non volatile (utilisée notamment par le WiFi)
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialisation réseau
    wifi_init_sta();
    mqtt_init();
    
    // Initialisation et calibration de l'IMU
    vTaskDelay(pdMS_TO_TICKS(200));
    imu_init_hw();
    imu_calibrate();

    // Initialisation du module IA
    ESP_LOGW(TAG, "DRAM libre avant alloc : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGW(TAG, "PSRAM libre avant alloc: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ai_init();
    // init_model();
    ESP_LOGW(TAG, "DRAM libre après alloc : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGW(TAG, "PSRAM libre après alloc: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    // Configuration du bouton utilisateur
    gpio_config_t btn{};
    btn.pin_bit_mask = 1ULL << BUTTON_PIN;
    btn.mode = GPIO_MODE_INPUT;
    btn.pull_up_en = GPIO_PULLUP_ENABLE;
    btn.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&btn);

    // Initialisation gestion batterie et alimentation
    //battery_init();
    //power_manager_init();

    // Démarrage des tâches applicatives
    app_tasks_start();
}