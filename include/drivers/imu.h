#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void imu_init_hw();
void imu_calibrate();
void imu_read_raw(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz
);

#ifdef __cplusplus
}
#endif