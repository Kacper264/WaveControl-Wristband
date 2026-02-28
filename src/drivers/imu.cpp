#include "drivers/imu.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include <cstdio>
#include <cstdint>

#define TAG_IMU "IMU"

/* ===== Hardware ===== */

#define I2C_PORT I2C_NUM_0
#define SDA_PIN 4
#define SCL_PIN 5
#define I2C_FREQ 100000

#define LSM6_ADDR 0x6A

/* ===== Registers ===== */

#define REG_CTRL1_XL   0x10
#define REG_CTRL2_G    0x11
#define REG_CTRL3_C    0x12

#define REG_OUTX_L_G   0x22   // gyro start (then TEMP, then ACC)
#define REG_OUTX_L_A   0x28
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

static void imu_dump_regs()
{
    uint8_t who=0, c1=0, c2=0, c3=0;
    i2c_read(0x0F, &who, 1);      // WHO_AM_I
    i2c_read(0x10, &c1,  1);      // CTRL1_XL
    i2c_read(0x11, &c2,  1);      // CTRL2_G
    i2c_read(0x12, &c3,  1);      // CTRL3_C

    printf("WHO_AM_I=0x%02X CTRL1_XL=0x%02X CTRL2_G=0x%02X CTRL3_C=0x%02X\r\n",
           who, c1, c2, c3);
}

/* ===== Raw read ===== */

static inline int16_t u8_to_i16(const uint8_t *p)
{
    return (int16_t)((p[1] << 8) | p[0]);
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

    // IMPORTANT : auto-increment des registres (IF_INC=1)
    i2c_write(REG_CTRL3_C, 0x04);

    // Ta config d'origine (ODR/FS). (Tu peux mettre 0x40 pour 104Hz si tu veux)
    i2c_write(REG_CTRL1_XL, 0x60);
    i2c_write(REG_CTRL2_G,  0x60);

    ESP_LOGI(TAG_IMU, "IMU initialized");
    imu_dump_regs();
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

void imu_read_raw(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz)
{
    uint8_t gbuf[6];
    uint8_t abuf[6];

    i2c_read(REG_OUTX_L_G, gbuf, 6);
    i2c_read(REG_OUTX_L_A, abuf, 6);

    int16_t gx_r = u8_to_i16(&gbuf[0]);
    int16_t gy_r = u8_to_i16(&gbuf[2]);
    int16_t gz_r = u8_to_i16(&gbuf[4]);

    int16_t ax_r = u8_to_i16(&abuf[0]);
    int16_t ay_r = u8_to_i16(&abuf[2]);
    int16_t az_r = u8_to_i16(&abuf[4]);

    *gx = (float)gx_r; *gy = (float)gy_r; *gz = (float)gz_r;
    *ax = (float)ax_r; *ay = (float)ay_r; *az = (float)az_r;

    printf("RAW -> ACC: %d %d %d | GYRO: %d %d %d\r\n",
           ax_r, ay_r, az_r, gx_r, gy_r, gz_r);
}

void imu_read_filtered(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz)
{
    float rax, ray, raz, rgx, rgy, rgz;

    // printf("\n===== IMU FILTER DEBUG =====\n");

    // 1. Lecture brute
    imu_read_raw(&rax,&ray,&raz,&rgx,&rgy,&rgz);

    // printf("RAW  -> ACC: %.2f %.2f %.2f | GYRO: %.2f %.2f %.2f\n",
    //        rax, ray, raz, rgx, rgy, rgz);

    // 2. Offsets
    // printf("OFFSETS -> ACC: %.2f %.2f %.2f | GYRO: %.2f %.2f %.2f\n",
    //        ax_off, ay_off, az_off, gx_off, gy_off, gz_off);

    // 3. Application des offsets
    rax -= ax_off; ray -= ay_off; raz -= az_off;
    rgx -= gx_off; rgy -= gy_off; rgz -= gz_off;

    // printf("CORR -> ACC: %.2f %.2f %.2f | GYRO: %.2f %.2f %.2f\n",
    //        rax, ray, raz, rgx, rgy, rgz);

    // 4. Valeurs filtrées précédentes
    // printf("PREV FILTER -> ACC: %.2f %.2f %.2f | GYRO: %.2f %.2f %.2f\n",
    //        ax_f, ay_f, az_f, gx_f, gy_f, gz_f);

    // 5. Filtrage (low-pass)
    ax_f = alpha_acc  * rax + (1 - alpha_acc)  * ax_f;
    ay_f = alpha_acc  * ray + (1 - alpha_acc)  * ay_f;
    az_f = alpha_acc  * raz + (1 - alpha_acc)  * az_f;

    gx_f = alpha_gyro * rgx + (1 - alpha_gyro) * gx_f;
    gy_f = alpha_gyro * rgy + (1 - alpha_gyro) * gy_f;
    gz_f = alpha_gyro * rgz + (1 - alpha_gyro) * gz_f;

    // printf("FILTERED -> ACC: %.2f %.2f %.2f | GYRO: %.2f %.2f %.2f\n",
    //        ax_f, ay_f, az_f, gx_f, gy_f, gz_f);

    // 6. Sortie
    *ax = ax_f;
    *ay = ay_f;
    *az = az_f;
    *gx = gx_f;
    *gy = gy_f;
    *gz = gz_f;

    // printf("OUTPUT -> ACC: %.2f %.2f %.2f | GYRO: %.2f %.2f %.2f\n",
    //        *ax, *ay, *az, *gx, *gy, *gz);

    // printf("============================\n");
}