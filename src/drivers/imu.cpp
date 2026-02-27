#include "drivers/imu.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "esp_log.h"

#define TAG_IMU "IMU"

/* ===== Hardware ===== */

#define I2C_PORT I2C_NUM_0
#define SDA_PIN 4
#define SCL_PIN 5
#define I2C_FREQ 100000

#define LSM6_ADDR 0x6A

/* ===== Registers ===== */

#define REG_CTRL1_XL  0x10
#define REG_CTRL2_G   0x11
#define REG_OUTX_L_G  0x22

/* ===== Filter ===== */

static const float alpha_acc  = 0.2f;
static const float alpha_gyro = 0.2f;

/* ===== State ===== */

static float ax_off, ay_off, az_off;
static float gx_off, gy_off, gz_off;

static float ax_f, ay_f, az_f;
static float gx_f, gy_f, gz_f;

/* ===== Low level ===== */

static esp_err_t i2c_write(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return i2c_master_write_to_device(
        I2C_PORT, LSM6_ADDR, data, 2, pdMS_TO_TICKS(100)
    );
}

static esp_err_t i2c_read(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(
        I2C_PORT, LSM6_ADDR, &reg, 1, buf, len, pdMS_TO_TICKS(100)
    );
}

/* ===== Raw read ===== */

static void imu_read_raw(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz)
{
    uint8_t buf[12];
    i2c_read(REG_OUTX_L_G, buf, 12);

    int16_t gx_r = buf[1]<<8 | buf[0];
    int16_t gy_r = buf[3]<<8 | buf[2];
    int16_t gz_r = buf[5]<<8 | buf[4];

    int16_t ax_r = buf[7]<<8 | buf[6];
    int16_t ay_r = buf[9]<<8 | buf[8];
    int16_t az_r = buf[11]<<8 | buf[10];

    *gx = gx_r; *gy = gy_r; *gz = gz_r;
    *ax = ax_r; *ay = ay_r; *az = az_r;
        // Affichage des valeurs
    printf("Gyro:  gx=%d gy=%d gz=%d\r\n", gx_r, gy_r, gz_r);
    printf("Accel: ax=%d ay=%d az=%d\r\n", ax_r, ay_r, az_r);
}

/* ===== Public API ===== */

void imu_init_hw()
{
    i2c_config_t conf{};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = SDA_PIN;
    conf.scl_io_num = SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ;

    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);

    i2c_write(REG_CTRL1_XL, 0x60);
    i2c_write(REG_CTRL2_G,  0x60);

    ESP_LOGI(TAG_IMU, "IMU initialized");
}

void imu_calibrate()
{
    const int N = 500;

    ax_off = ay_off = az_off = 0;
    gx_off = gy_off = gz_off = 0;

    ESP_LOGI(TAG_IMU, "Calibration...");

    for (int i = 0; i < N; i++)
    {
        float ax, ay, az, gx, gy, gz;
        imu_read_raw(&ax,&ay,&az,&gx,&gy,&gz);

        ax_off += ax; ay_off += ay; az_off += az;
        gx_off += gx; gy_off += gy; gz_off += gz;

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    ax_off/=N; ay_off/=N; az_off/=N;
    gx_off/=N; gy_off/=N; gz_off/=N;

    ax_f = ay_f = az_f = 0;
    gx_f = gy_f = gz_f = 0;

    ESP_LOGI(TAG_IMU, "Calibration done");
}

void imu_read_filtered(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz)
{
    float rax, ray, raz, rgx, rgy, rgz;

    imu_read_raw(&rax,&ray,&raz,&rgx,&rgy,&rgz);

    rax -= ax_off; ray -= ay_off; raz -= az_off;
    rgx -= gx_off; rgy -= gy_off; rgz -= gz_off;

    ax_f = alpha_acc  * rax + (1 - alpha_acc)  * ax_f;
    ay_f = alpha_acc  * ray + (1 - alpha_acc)  * ay_f;
    az_f = alpha_acc  * raz + (1 - alpha_acc)  * az_f;

    gx_f = alpha_gyro * rgx + (1 - alpha_gyro) * gx_f;
    gy_f = alpha_gyro * rgy + (1 - alpha_gyro) * gy_f;
    gz_f = alpha_gyro * rgz + (1 - alpha_gyro) * gz_f;

    *ax = ax_f;
    *ay = ay_f;
    *az = az_f;
    *gx = gx_f;
    *gy = gy_f;
    *gz = gz_f;
}