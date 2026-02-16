#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// init du timer sleep
void power_manager_init(void);

// à appeler quand on démarre une acquisition (empêche le sleep)
void power_manager_disarm_sleep(void);

// à appeler après publication MQTT du résultat IA (arme sleep dans 5 min)
void power_manager_arm_sleep(void);

// indique si on vient d'un réveil bouton (deep sleep ext0/ext1)
bool power_manager_woke_from_button(void);

#ifdef __cplusplus
}
#endif
