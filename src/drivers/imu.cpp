#include "drivers/imu.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include <cstdio>
#include <cstdint>
#include "common/strings_constants.h"

#define TAG_IMU "IMU"

/* ===== Hardware ===== */

#define I2C_MASTER_SCL_IO           6     
#define I2C_MASTER_SDA_IO           5      
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000 

// Adresses
#define LSM6_ADDR                   0x6B    
#define LIS3MDL_ADDR                0x1E    

// Registres
#define LSM6_CTRL1_XL               0x10
#define LSM6_CTRL2_G                0x11
#define LSM6_CTRL3_C                0x12
#define LSM6_OUTX_L_G               0x22
#define LSM6_OUTX_L_A               0x28
#define LIS3MDL_OUT_X_L             0x28



// --- FONCTIONS DE COMMUNICATION ---

esp_err_t write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t read_burst(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, dev_addr, &reg_addr, 1, data, len, pdMS_TO_TICKS(100));
}

// --- INITIALISATION ---

void imu_init_hw()
{
    // 1. Init I2C (Version compatible C++)
    i2c_config_t conf = {}; // On initialise tout à zéro d'abord
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    // 2. Init LSM6
    write_reg(LSM6_ADDR, LSM6_CTRL1_XL, 0x3C);
    write_reg(LSM6_ADDR, LSM6_CTRL2_G, 0x4C);
    write_reg(LSM6_ADDR, LSM6_CTRL3_C, 0x04);

    // 3. Init LIS3MDL
    write_reg(LIS3MDL_ADDR, 0x20, 0x70);
    write_reg(LIS3MDL_ADDR, 0x21, 0x00);
    write_reg(LIS3MDL_ADDR, 0x22, 0x00);
}

void imu_read_raw()
{
    int ax_temp=0, ay_temp=0, az_temp=0, gx_temp=0, gy_temp=0, gz_temp=0;

    uint8_t data[6];

    // 1. Lecture Accel
    if (read_burst(LSM6_ADDR, LSM6_OUTX_L_A, data, 6) == ESP_OK) {
        ax_temp = (int16_t)((data[1] << 8) | data[0]) >> 4;
        ay_temp = (int16_t)((data[3] << 8) | data[2]) >> 4;
        az_temp = (int16_t)((data[5] << 8) | data[4]) >> 4;
    }

    // 2. Lecture Gyro
    if (read_burst(LSM6_ADDR, LSM6_OUTX_L_G, data, 6) == ESP_OK) {
        gx_temp = (int16_t)((data[1] << 8) | data[0]);
        gy_temp = (int16_t)((data[3] << 8) | data[2]);
        gz_temp = (int16_t)((data[5] << 8) | data[4]);      

    }


    // Appliquer les offsets de calibration
    ax = ax_temp - ax_cal;
    ay = ay_temp - ay_cal;
    az = az_temp - az_cal;
    gx = gx_temp - gx_cal;
    gy = gy_temp - gy_cal;
    gz = gz_temp - gz_cal;

}

void imu_calibrate()
{
    
    const int N = 500;

    int ax_off = 0, ay_off = 0, az_off = 0;
    int gx_off = 0, gy_off = 0, gz_off = 0;

    ESP_LOGI(TAG_IMU, "Calibration...");

    for (int i = 0; i < N; i++)
    {
        imu_read_raw();

        ax_off += ax; ay_off += ay; az_off += az;
        gx_off += gx; gy_off += gy; gz_off += gz;

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    ax_off/=N; ay_off/=N; az_off/=N;
    gx_off/=N; gy_off/=N; gz_off/=N;

    ax_cal = ax_off; ay_cal = ay_off; az_cal = az_off;
    gx_cal = gx_off; gy_cal = gy_off; gz_cal = gz_off;

    ESP_LOGI(TAG_IMU, "Calibration done");
}

/*

void imu_read_raw(

    float *ax, float *ay, float *az,

    float *gx, float *gy, float *gz,

    float *mx, float *my, float *mz)

{

    uint8_t data[6];

    // 1. Lecture Accel
    if (read_burst(LSM6_ADDR, LSM6_OUTX_L_A, data, 6) == ESP_OK) {
        accel_x = (int16_t)((data[1] << 8) | data[0]) >> 4;
        accel_y = (int16_t)((data[3] << 8) | data[2]) >> 4;
        accel_z = (int16_t)((data[5] << 8) | data[4]) >> 4;
    }

    // 2. Lecture Gyro
    if (read_burst(LSM6_ADDR, LSM6_OUTX_L_G, data, 6) == ESP_OK) {
        gyro_x = (int16_t)((data[1] << 8) | data[0]);
        gyro_y = (int16_t)((data[3] << 8) | data[2]);
        gyro_z = (int16_t)((data[5] << 8) | data[4]);
    }

    // 3. Lecture Mag (avec auto-increment bit 0x80 pour LIS3MDL)
    if (read_burst(LIS3MDL_ADDR, (LIS3MDL_OUT_X_L | 0x80), data, 6) == ESP_OK) {
        mag_x = (int16_t)((data[1] << 8) | data[0]);
        mag_y = (int16_t)((data[3] << 8) | data[2]);
        mag_z = (int16_t)((data[5] << 8) | data[4]);
    }


    *gx = (float)gyro_x; *gy = (float)gyro_y; *gz = (float)gyro_z;

    *ax = (float)accel_x; *ay = (float)accel_y; *az = (float)accel_z;

    *mx = (float)mag_x; *my = (float)mag_y; *mz = (float)mag_z;

}
    
*/