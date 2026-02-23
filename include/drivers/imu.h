#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware init (I2C + capteurs) */
void imu_init_hw();

/* Calibration offsets (device immobile) */
void imu_calibrate();

/* === API legacy (6 axes) : compatible avec ton acquisition_task === */
void imu_read_raw(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz);

void imu_read_filtered(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz);

/* === Optionnel (9 axes) si tu veux plus tard === */
void imu_read_raw_9dof(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz);

void imu_read_filtered_9dof(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz);

#ifdef __cplusplus
}
#endif