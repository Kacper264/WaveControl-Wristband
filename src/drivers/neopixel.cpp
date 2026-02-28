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
    SET_RGB,
    BLINK_BLUE,
    BLINK_RED,
    OFF,
};

struct LedCmd {
    LedCmdType type;
    uint8_t r, g, b;
    uint32_t on_ms;
    uint32_t off_ms;
};

static QueueHandle_t s_led_queue = nullptr;
static led_strip_handle_t s_strip = nullptr;

// État “idle” (vert) mémorisé pour revenir après blink
static uint8_t s_idle_r = 0;
static uint8_t s_idle_g = 40; // un vert pas trop agressif
static uint8_t s_idle_b = 0;

static void apply_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_strip) return;
    ESP_ERROR_CHECK(led_strip_set_pixel(s_strip, 0, r, g, b));
    ESP_ERROR_CHECK(led_strip_refresh(s_strip));
}

static void led_task(void* arg)
{
    (void)arg;
    LedCmd cmd;

    // état initial: vert
    apply_rgb(s_idle_r, s_idle_g, s_idle_b);

    while (true) {
        if (xQueueReceive(s_led_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.type) {
                case LedCmdType::SET_RGB:
                    apply_rgb(cmd.r, cmd.g, cmd.b);
                    break;

                case LedCmdType::BLINK_BLUE:
                    // ON bleu
                    apply_rgb(0, 0, 255);
                    vTaskDelay(pdMS_TO_TICKS(cmd.on_ms));
                    // OFF (ou retour idle direct)
                    apply_rgb(0, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(cmd.off_ms));
                    // Retour idle vert
                    apply_rgb(s_idle_r, s_idle_g, s_idle_b);
                    break;

                case LedCmdType::BLINK_RED:
                    // ON rouge
                    apply_rgb(255, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(cmd.on_ms));
                    // OFF
                    apply_rgb(0, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(cmd.off_ms));
                    // Retour idle vert
                    apply_rgb(s_idle_r, s_idle_g, s_idle_b);
                    break;
                    
                case LedCmdType::OFF:
                    apply_rgb(0, 0, 0);
                    break;
            }
        }
    }
}

void neopixel_init(int gpio, uint32_t num_leds)
{
    if (s_led_queue) return; // déjà init

    s_led_queue = xQueueCreate(8, sizeof(LedCmd));
    if (!s_led_queue) {
        ESP_LOGE(TAG_LED, "Failed to create LED queue");
        return;
    }

    // Config minimale compatible avec ton header actuel
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = num_leds,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip));

    xTaskCreate(led_task, "neopixel", 3072, nullptr, 2, nullptr);
    ESP_LOGI(TAG_LED, "NeoPixel task started (GPIO=%d, leds=%ld)", gpio, num_leds);
}

void neopixel_set_idle_green()
{
    s_idle_r = 0;
    s_idle_g = 40;
    s_idle_b = 0;

    if (!s_led_queue) return;
    LedCmd cmd{LedCmdType::SET_RGB, s_idle_r, s_idle_g, s_idle_b, 0, 0};
    xQueueSend(s_led_queue, &cmd, 0);
}

void neopixel_blink_blue(uint32_t on_ms, uint32_t off_ms)
{
    if (!s_led_queue) return;
    LedCmd cmd{};
    cmd.type = LedCmdType::BLINK_BLUE;
    cmd.on_ms = on_ms;
    cmd.off_ms = off_ms;
    xQueueSend(s_led_queue, &cmd, 0);
}

void neopixel_blink_red(uint32_t on_ms, uint32_t off_ms)
{
    if (!s_led_queue) return;
    LedCmd cmd{};
    cmd.type = LedCmdType::BLINK_RED;
    cmd.on_ms = on_ms;
    cmd.off_ms = off_ms;
    xQueueSend(s_led_queue, &cmd, 0);
}

void neopixel_off()
{
    if (!s_led_queue) return;
    LedCmd cmd{LedCmdType::OFF, 0, 0, 0, 0, 0};
    xQueueSend(s_led_queue, &cmd, 0);
}