#include "include.h"

#define TAG_IMU "IMU"

/* -------------------------------------------------------------------------- */
/* FONCTIONS DE COMMUNICATION I2C                                             */
/* -------------------------------------------------------------------------- */

/*
 * Écrit une valeur dans un registre d'un périphérique I2C.
 * Utilisé pour configurer les capteurs IMU (accéléro / gyro / mag).
 */
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

/*
 * Lecture en rafale (burst read) de plusieurs octets depuis un registre I2C.
 * Permet de lire les données des capteurs en une seule transaction.
 */
esp_err_t read_burst(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(
        I2C_MASTER_NUM,
        dev_addr,
        &reg_addr,
        1,
        data,
        len,
        pdMS_TO_TICKS(100)
    );
}

/* -------------------------------------------------------------------------- */
/* INITIALISATION DU MATÉRIEL IMU                                             */
/* -------------------------------------------------------------------------- */

void imu_init_hw()
{
    // 1. Initialisation du bus I2C
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    // 2. Configuration du LSM6 (accéléromètre + gyroscope)
    write_reg(LSM6_ADDR, LSM6_CTRL1_XL, 0x3C); // configuration accel
    write_reg(LSM6_ADDR, LSM6_CTRL2_G, 0x4C);  // configuration gyro
    write_reg(LSM6_ADDR, LSM6_CTRL3_C, 0x04);  // configuration générale

    // 3. Configuration du magnétomètre LIS3MDL
    write_reg(LIS3MDL_ADDR, 0x20, 0x70);
    write_reg(LIS3MDL_ADDR, 0x21, 0x00);
    write_reg(LIS3MDL_ADDR, 0x22, 0x00);
}

/* -------------------------------------------------------------------------- */
/* LECTURE DES DONNÉES BRUTES IMU                                             */
/* -------------------------------------------------------------------------- */

void imu_read_raw()
{
    int ax_temp=0, ay_temp=0, az_temp=0;
    int gx_temp=0, gy_temp=0, gz_temp=0;

    uint8_t data[6];

    // 1. Lecture de l'accéléromètre
    if (read_burst(LSM6_ADDR, LSM6_OUTX_L_A, data, 6) == ESP_OK) {
        ax_temp = (int16_t)((data[1] << 8) | data[0]) >> 4;
        ay_temp = (int16_t)((data[3] << 8) | data[2]) >> 4;
        az_temp = (int16_t)((data[5] << 8) | data[4]) >> 4;
    }

    // 2. Lecture du gyroscope
    if (read_burst(LSM6_ADDR, LSM6_OUTX_L_G, data, 6) == ESP_OK) {
        gx_temp = (int16_t)((data[1] << 8) | data[0]);
        gy_temp = (int16_t)((data[3] << 8) | data[2]);
        gz_temp = (int16_t)((data[5] << 8) | data[4]);
    }

    // 3. Application des offsets de calibration
    ax = ax_temp - ax_cal;
    ay = ay_temp - ay_cal;
    az = az_temp - az_cal;

    gx = gx_temp - gx_cal;
    gy = gy_temp - gy_cal;
    gz = gz_temp - gz_cal;
}

/* -------------------------------------------------------------------------- */
/* CALIBRATION IMU                                                            */
/* -------------------------------------------------------------------------- */

void imu_calibrate()
{
    const int N = IMU_CALIB_SAMPLES;

    int ax_off = 0, ay_off = 0, az_off = 0;
    int gx_off = 0, gy_off = 0, gz_off = 0;

    ESP_LOGI(TAG_IMU, "Calibration...");

    // Moyenne de plusieurs mesures pour estimer l'offset des capteurs
    for (int i = 0; i < N; i++)
    {
        imu_read_raw();

        ax_off += ax; ay_off += ay; az_off += az;
        gx_off += gx; gy_off += gy; gz_off += gz;

        vTaskDelay(pdMS_TO_TICKS(IMU_CALIB_DELAY_MS));
    }

    // Calcul de la moyenne
    ax_off/=N; ay_off/=N; az_off/=N;
    gx_off/=N; gy_off/=N; gz_off/=N;

    // Sauvegarde des offsets
    ax_cal = ax_off; ay_cal = ay_off; az_cal = az_off;
    gx_cal = gx_off; gy_cal = gy_off; gz_cal = gz_off;

    ESP_LOGI(TAG_IMU, "Calibration done");
}