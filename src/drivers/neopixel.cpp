#include "include.h"

static constexpr const char* TAG_LED = "NEOPIXEL";

enum class LedCmdType : uint8_t {
    SET_ONE,
    SET_ALL,
    BLINK_ONE_BLUE,
    BLINK_ONE_GREEN,
    OFF_ALL,
    SET_IDLE_ONE,
    SET_IDLE_ALL,
};

struct LedCmd {
    LedCmdType type;
    int16_t index;
    uint8_t r, g, b;
    uint32_t on_ms;
    uint32_t off_ms;
    uint32_t duration_ms;
};

struct Rgb {
    uint8_t r, g, b;
};

/* Ressources internes du module */
static QueueHandle_t s_led_queue = nullptr;
static led_strip_handle_t s_strip = nullptr;
static uint32_t s_num_leds = 1;
static Rgb* s_idle_colors = nullptr;

/*
 * Applique effectivement les changements sur le ruban.
 */
static void refresh()
{
    if (!s_strip) return;
    ESP_ERROR_CHECK(led_strip_refresh(s_strip));
}

/*
 * Met à jour une LED précise.
 */
static void set_one(uint32_t i, uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_strip) return;
    if (i >= s_num_leds) return;

    ESP_ERROR_CHECK(led_strip_set_pixel(s_strip, i, r, g, b));
}

/*
 * Met à jour toutes les LEDs avec la même couleur.
 */
static void set_all(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_strip) return;

    for (uint32_t i = 0; i < s_num_leds; ++i) {
        ESP_ERROR_CHECK(led_strip_set_pixel(s_strip, i, r, g, b));
    }
}

/*
 * Applique la couleur idle d'une LED précise.
 */
static void apply_idle_one(uint32_t i)
{
    if (!s_strip || !s_idle_colors) return;
    if (i >= s_num_leds) return;

    ESP_ERROR_CHECK(led_strip_set_pixel(
        s_strip, i,
        s_idle_colors[i].r,
        s_idle_colors[i].g,
        s_idle_colors[i].b
    ));
}

/*
 * Applique les couleurs idle à toutes les LEDs.
 */
static void apply_idle_all()
{
    if (!s_strip || !s_idle_colors) return;

    for (uint32_t i = 0; i < s_num_leds; ++i) {
        apply_idle_one(i);
    }
}

/*
 * Tâche dédiée au pilotage des LEDs.
 */
static void led_task(void* arg)
{
    (void)arg;
    LedCmd cmd;

    apply_idle_all();
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

                case LedCmdType::SET_IDLE_ONE:
                    if (s_idle_colors && cmd.index >= 0 && (uint32_t)cmd.index < s_num_leds) {
                        s_idle_colors[cmd.index] = {cmd.r, cmd.g, cmd.b};
                        apply_idle_one((uint32_t)cmd.index);
                        refresh();
                    }
                    break;

                case LedCmdType::SET_IDLE_ALL:
                    if (s_idle_colors) {
                        for (uint32_t i = 0; i < s_num_leds; ++i) {
                            s_idle_colors[i] = {cmd.r, cmd.g, cmd.b};
                        }
                        apply_idle_all();
                        refresh();
                    }
                    break;

                case LedCmdType::BLINK_ONE_BLUE:
                    if (cmd.index >= 0 && (uint32_t)cmd.index < s_num_leds) {
                        uint32_t i = (uint32_t)cmd.index;

                        set_one(i, 0, 0, 255);
                        refresh();
                        vTaskDelay(pdMS_TO_TICKS(cmd.on_ms));

                        set_one(i, 0, 0, 0);
                        refresh();
                        vTaskDelay(pdMS_TO_TICKS(cmd.off_ms));

                        apply_idle_one(i);
                        refresh();
                    }
                    break;

                case LedCmdType::BLINK_ONE_GREEN:
                    if (cmd.index >= 0 && (uint32_t)cmd.index < s_num_leds) {
                        uint32_t i = (uint32_t)cmd.index;

                        // Si duration_ms == 0, on garde l'ancien comportement : un seul blink
                        if (cmd.duration_ms == 0) {
                            set_one(i, 0, 255, 0);
                            refresh();
                            vTaskDelay(pdMS_TO_TICKS(cmd.on_ms));

                            set_one(i, 0, 0, 0);
                            refresh();
                            vTaskDelay(pdMS_TO_TICKS(cmd.off_ms));

                            apply_idle_one(i);
                            refresh();
                        } else {
                            TickType_t start = xTaskGetTickCount();
                            TickType_t duration_ticks = pdMS_TO_TICKS(cmd.duration_ms);

                            while ((xTaskGetTickCount() - start) < duration_ticks) {
                                set_one(i, 0, 255, 0);
                                refresh();
                                vTaskDelay(pdMS_TO_TICKS(cmd.on_ms));

                                // Recheck durée après le ON
                                if ((xTaskGetTickCount() - start) >= duration_ticks) {
                                    break;
                                }

                                set_one(i, 0, 0, 0);
                                refresh();
                                vTaskDelay(pdMS_TO_TICKS(cmd.off_ms));
                            }

                            apply_idle_one(i);
                            refresh();
                        }
                    }
                    break;

                case LedCmdType::OFF_ALL:
                    set_all(0, 0, 0);
                    refresh();
                    break;
            }
        }
    }
}

/*
 * Initialise le driver NeoPixel, crée la file de commandes
 * et démarre la tâche dédiée.
 */
void neopixel_init(int gpio, uint32_t num_leds)
{
    if (s_led_queue) return;

    s_num_leds = num_leds;

    s_led_queue = xQueueCreate(NEOPIXEL_QUEUE_LEN, sizeof(LedCmd));
    if (!s_led_queue) {
        ESP_LOGE(TAG_LED, "Failed to create LED queue");
        return;
    }

    s_idle_colors = (Rgb*)calloc(num_leds, sizeof(Rgb));
    if (!s_idle_colors) {
        ESP_LOGE(TAG_LED, "Failed to allocate idle colors");
        vQueueDelete(s_led_queue);
        s_led_queue = nullptr;
        return;
    }

    for (uint32_t i = 0; i < s_num_leds; ++i) {
        s_idle_colors[i] = {NEOPIXEL_IDLE_R, NEOPIXEL_IDLE_G, NEOPIXEL_IDLE_B};
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = num_leds,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = NEOPIXEL_RMT_RESOLUTION_HZ,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip));

    xTaskCreate(led_task, "neopixel", NEOPIXEL_TASK_STACK, nullptr, NEOPIXEL_TASK_PRIO, nullptr);
    ESP_LOGI(TAG_LED, "NeoPixel task started (GPIO=%d, leds=%lu)", gpio, (unsigned long)num_leds);
}

/*
 * Demande la mise à jour d'une LED spécifique.
 */
void neopixel_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led_queue) return;

    LedCmd cmd{};
    cmd.type = LedCmdType::SET_ONE;
    cmd.index = (int16_t)index;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.duration_ms = 0;

    xQueueSend(s_led_queue, &cmd, 0);
}

/*
 * Demande la mise à jour de toutes les LEDs.
 */
void neopixel_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led_queue) return;

    LedCmd cmd{};
    cmd.type = LedCmdType::SET_ALL;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.duration_ms = 0;

    xQueueSend(s_led_queue, &cmd, 0);
}

/*
 * Définit la couleur idle d'un seul pixel.
 */
void neopixel_set_idle_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led_queue) return;

    LedCmd cmd{};
    cmd.type = LedCmdType::SET_IDLE_ONE;
    cmd.index = (int16_t)index;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.duration_ms = 0;

    xQueueSend(s_led_queue, &cmd, 0);
}

/*
 * Définit la couleur idle de tous les pixels.
 */
void neopixel_set_idle_all(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led_queue) return;

    LedCmd cmd{};
    cmd.type = LedCmdType::SET_IDLE_ALL;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.duration_ms = 0;

    xQueueSend(s_led_queue, &cmd, 0);
}

/*
 * Fait clignoter en bleu un seul neopixel.
 */
void neopixel_blink_blue(uint32_t index, uint32_t on_ms, uint32_t off_ms)
{
    if (!s_led_queue) return;

    LedCmd cmd{};
    cmd.type = LedCmdType::BLINK_ONE_BLUE;
    cmd.index = (int16_t)index;
    cmd.on_ms = on_ms;
    cmd.off_ms = off_ms;
    cmd.duration_ms = 0;

    xQueueSend(s_led_queue, &cmd, 0);
}

/*
 * Fait clignoter en vert un seul neopixel.
 */
void neopixel_blink_green(uint32_t index, uint32_t on_ms, uint32_t off_ms)
{
    if (!s_led_queue) return;

    LedCmd cmd{};
    cmd.type = LedCmdType::BLINK_ONE_GREEN;
    cmd.index = (int16_t)index;
    cmd.on_ms = on_ms;
    cmd.off_ms = off_ms;
    cmd.duration_ms = 0;

    xQueueSend(s_led_queue, &cmd, 0);
}
/*
 * Fait clignoter en vert un seul neopixel durant le chargement.
 */
void neopixel_blink_green_loading(uint32_t index, uint32_t on_ms, uint32_t off_ms)
{
    if (!s_led_queue) return;

    LedCmd cmd{};
    cmd.type = LedCmdType::BLINK_ONE_GREEN;
    cmd.index = (int16_t)index;
    cmd.on_ms = on_ms;
    cmd.off_ms = off_ms;
    cmd.duration_ms = 60000; // 1 minute

    xQueueSend(s_led_queue, &cmd, 0);
}

/*
 * Éteint toutes les LEDs.
 */
void neopixel_off()
{
    if (!s_led_queue) return;

    LedCmd cmd{};
    cmd.type = LedCmdType::OFF_ALL;

    xQueueSend(s_led_queue, &cmd, 0);
}