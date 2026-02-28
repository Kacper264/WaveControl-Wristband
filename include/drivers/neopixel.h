#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Initialise le NeoPixel + démarre la tâche interne
void neopixel_init(int gpio, uint32_t num_leds);

// LED état : verte (idle)
void neopixel_set_idle_green();

// Blink bleu non bloquant (la tâche LED gère le timing)
void neopixel_blink_blue(uint32_t on_ms = 120, uint32_t off_ms = 80);
void neopixel_blink_red(uint32_t on_ms = 120, uint32_t off_ms = 80);

// Optionnel: éteindre
void neopixel_off();

#ifdef __cplusplus
}
#endif