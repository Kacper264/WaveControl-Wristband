#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

void neopixel_init(int gpio, uint32_t num_leds);

void neopixel_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
void neopixel_set_all(uint8_t r, uint8_t g, uint8_t b);

void neopixel_set_idle_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
void neopixel_set_idle_all(uint8_t r, uint8_t g, uint8_t b);

void neopixel_blink_blue(uint32_t index, uint32_t on_ms, uint32_t off_ms);
void neopixel_blink_green(uint32_t index, uint32_t on_ms, uint32_t off_ms);

void neopixel_off();

#ifdef __cplusplus
}
#endif