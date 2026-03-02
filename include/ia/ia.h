#pragma once
#include <stdint.h>
#include "common/strings_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

void ai_init();

/* pousse un sample IMU, retourne true si inference déclenchée */
bool ai_push_sample(
    float ax, float ay, float az,
    float gx, float gy, float gz,
    float mx, float my, float mz
);

/* récupère dernier résultat */
bool ai_get_result(Move *move, uint8_t *confidence);

#ifdef __cplusplus
}
#endif
