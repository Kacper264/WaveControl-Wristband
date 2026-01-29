#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void imu_init_hw();
void imu_calibrate();

void imu_read_filtered(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz
);

#ifdef __cplusplus
}
#endif
