#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Initialise le NeoPixel + démarre la tâche interne
void neopixel_init(int gpio, uint32_t num_leds);

// LED état : verte (idle)
void neopixel_set_idle_green();

// --- NOUVEAU : commandes par LED / globales ---

// Met une couleur sur un pixel (index 0..num_leds-1)
void neopixel_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);

// Met une couleur sur toutes les LEDs
void neopixel_set_all(uint8_t r, uint8_t g, uint8_t b);

// Optionnel: éteindre toutes les LEDs
void neopixel_off();

#ifdef __cplusplus
}
#endif