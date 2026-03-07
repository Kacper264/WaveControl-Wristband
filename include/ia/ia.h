#pragma once
#include <stdint.h>
#include "common/strings_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

void ai_init();

/* pousse un sample IMU, retourne true si inference déclenchée */
void process_inference_result(float* output_buffer);
void ai_run_inference(float* imu_buffer, float* output_buffer);

#ifdef __cplusplus
}
#endif
