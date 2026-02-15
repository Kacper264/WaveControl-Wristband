#include "power/power.h"
#include "common/strings_constants.h"
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
}

static const char *TAG_PWR = "POWER";

static TimerHandle_t sleep_timer = nullptr;
static uint32_t g_sleep_after_ms = 0;
static int g_button_gpio = -1;
static bool g_woke_from_button = false;

static void configure_button_wakeup()
{
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    rtc_gpio_init((gpio_num_t)g_button_gpio);
    rtc_gpio_set_direction((gpio_num_t)g_button_gpio, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en((gpio_num_t)g_button_gpio);
    rtc_gpio_pulldown_dis((gpio_num_t)g_button_gpio);

    // bouton pull-up, actif bas => wake quand niveau = 0
    esp_sleep_enable_ext0_wakeup((gpio_num_t)g_button_gpio, 0);
}

static void go_to_deep_sleep()
{
    ESP_LOGI(TAG_PWR, "Entering deep sleep (wake on button GPIO=%d)", g_button_gpio);
    configure_button_wakeup();
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_deep_sleep_start();
}

static void sleep_timer_cb(TimerHandle_t t)
{
    (void)t;
    go_to_deep_sleep();
}

void power_manager_init(void)
{
    g_sleep_after_ms = SLEEP_AFTER_IA_MS;
    g_button_gpio = BUTTON_PIN;

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    g_woke_from_button = (cause == ESP_SLEEP_WAKEUP_EXT0);

    sleep_timer = xTimerCreate(
        "sleep_t",
        pdMS_TO_TICKS(g_sleep_after_ms),
        pdFALSE,     // one-shot
        nullptr,
        sleep_timer_cb
    );
}

bool power_manager_woke_from_button(void)
{
    return g_woke_from_button;
}

void power_manager_arm_sleep(void)
{
    if (!sleep_timer) return;
    xTimerStop(sleep_timer, 0);
    xTimerChangePeriod(sleep_timer, pdMS_TO_TICKS(g_sleep_after_ms), 0);
    xTimerStart(sleep_timer, 0);
}

void power_manager_disarm_sleep(void)
{
    if (!sleep_timer) return;
    xTimerStop(sleep_timer, 0);
}
