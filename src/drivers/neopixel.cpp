#include "drivers/neopixel.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "led_strip.h"
}

static constexpr const char* TAG_LED = "NEOPIXEL";

enum class LedCmdType : uint8_t {
    SET_ONE,     // une LED
    SET_ALL,     // toutes les LEDs
    BLINK_ALL_BLUE,
    BLINK_ALL_RED,
    OFF_ALL,
};

struct LedCmd {
    LedCmdType type;
    int16_t index;   // index LED (0..n-1), utilisé pour SET_ONE. Ignoré sinon.
    uint8_t r, g, b;
    uint32_t on_ms;
    uint32_t off_ms;
};

static QueueHandle_t s_led_queue = nullptr;
static led_strip_handle_t s_strip = nullptr;
static uint32_t s_num_leds = 1;

// Idle mémorisé (optionnel)
static uint8_t s_idle_r = 0;
static uint8_t s_idle_g = 40;
static uint8_t s_idle_b = 0;

static void refresh()
{
    if (!s_strip) return;
    ESP_ERROR_CHECK(led_strip_refresh(s_strip));
}

static void set_one(uint32_t i, uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_strip) return;
    if (i >= s_num_leds) return;
    ESP_ERROR_CHECK(led_strip_set_pixel(s_strip, i, r, g, b));
}

static void set_all(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_strip) return;
    for (uint32_t i = 0; i < s_num_leds; ++i) {
        ESP_ERROR_CHECK(led_strip_set_pixel(s_strip, i, r, g, b));
    }
}

static void led_task(void* arg)
{
    (void)arg;
    LedCmd cmd;

    // état initial: vert sur toutes
    set_all(s_idle_r, s_idle_g, s_idle_b);
    refresh();

    while (true) {
        if (xQueueReceive(s_led_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.type) {

                case LedCmdType::SET_ONE:
                    if (cmd.index >= 0 && (uint32_t)cmd.index < s_num_leds) {
                        set_one((uint32_t)cmd.index, cmd.r, cmd.g, cmd.b);
                        refresh();
                    }
                    break;

                case LedCmdType::SET_ALL:
                    set_all(cmd.r, cmd.g, cmd.b);
                    refresh();
                    break;

                case LedCmdType::BLINK_ALL_BLUE:
                    set_all(0, 0, 255);
                    refresh();
                    vTaskDelay(pdMS_TO_TICKS(cmd.on_ms));

                    set_all(0, 0, 0);
                    refresh();
                    vTaskDelay(pdMS_TO_TICKS(cmd.off_ms));

                    set_all(s_idle_r, s_idle_g, s_idle_b);
                    refresh();
                    break;

                case LedCmdType::BLINK_ALL_RED:
                    set_all(255, 0, 0);
                    refresh();
                    vTaskDelay(pdMS_TO_TICKS(cmd.on_ms));

                    set_all(0, 0, 0);
                    refresh();
                    vTaskDelay(pdMS_TO_TICKS(cmd.off_ms));

                    set_all(s_idle_r, s_idle_g, s_idle_b);
                    refresh();
                    break;

                case LedCmdType::OFF_ALL:
                    set_all(0, 0, 0);
                    refresh();
                    break;
            }
        }
    }
}

void neopixel_init(int gpio, uint32_t num_leds)
{
    if (s_led_queue) return;

    s_num_leds = num_leds;

    s_led_queue = xQueueCreate(8, sizeof(LedCmd));
    if (!s_led_queue) {
        ESP_LOGE(TAG_LED, "Failed to create LED queue");
        return;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = num_leds,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip));

    xTaskCreate(led_task, "neopixel", 3072, nullptr, 2, nullptr);
    ESP_LOGI(TAG_LED, "NeoPixel task started (GPIO=%d, leds=%ld)", gpio, num_leds);
}

// ✅ Nouvelle API : une couleur par LED
void neopixel_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led_queue) return;
    LedCmd cmd{};
    cmd.type = LedCmdType::SET_ONE;
    cmd.index = (int16_t)index;
    cmd.r = r; cmd.g = g; cmd.b = b;
    xQueueSend(s_led_queue, &cmd, 0);
}

// ✅ Optionnel : toutes les LEDs
void neopixel_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led_queue) return;
    LedCmd cmd{};
    cmd.type = LedCmdType::SET_ALL;
    cmd.r = r; cmd.g = g; cmd.b = b;
    xQueueSend(s_led_queue, &cmd, 0);
}

void neopixel_set_idle_green()
{
    s_idle_r = 0;
    s_idle_g = 40;
    s_idle_b = 0;

    // mets l'idle sur toutes (tu peux changer ça si tu veux)
    neopixel_set_all(s_idle_r, s_idle_g, s_idle_b);
}

void neopixel_blink_blue(uint32_t on_ms, uint32_t off_ms)
{
    if (!s_led_queue) return;
    LedCmd cmd{};
    cmd.type = LedCmdType::BLINK_ALL_BLUE;
    cmd.on_ms = on_ms;
    cmd.off_ms = off_ms;
    xQueueSend(s_led_queue, &cmd, 0);
}

void neopixel_blink_red(uint32_t on_ms, uint32_t off_ms)
{
    if (!s_led_queue) return;
    LedCmd cmd{};
    cmd.type = LedCmdType::BLINK_ALL_RED;
    cmd.on_ms = on_ms;
    cmd.off_ms = off_ms;
    xQueueSend(s_led_queue, &cmd, 0);
}

void neopixel_off()
{
    if (!s_led_queue) return;
    LedCmd cmd{};
    cmd.type = LedCmdType::OFF_ALL;
    xQueueSend(s_led_queue, &cmd, 0);
}