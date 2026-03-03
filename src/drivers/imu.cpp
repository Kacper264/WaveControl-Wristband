#include "drivers/imu.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include <cstdio>
#include <cstdint>
#include <cmath>

#define TAG_IMU "IMU"

/* ===== Hardware ===== */

#define I2C_PORT I2C_NUM_0
#define SDA_PIN 5
#define SCL_PIN 6
#define I2C_FREQ 100000

// Acc/Gyro (LSM6 / bloc AG du LSM9DS1)
#define LSM6_ADDR 0x6B

// Magnetometer (bloc M du LSM9DS1, très souvent 0x1E)
#define MAG_ADDR  0x1E

/* ===== Registers (AG) ===== */

#define REG_CTRL1_XL   0x10
#define REG_CTRL2_G    0x11
#define REG_CTRL3_C    0x12

#define REG_OUTX_L_G   0x22   // gyro start
#define REG_OUTX_L_A   0x28   // accel start

/* ===== Registers (MAG - LSM9DS1) =====
   Note: auto-increment via bit7 de l'adresse de registre (reg | 0x80) */
#define REG_CTRL_REG1_M 0x20
#define REG_CTRL_REG2_M 0x21
#define REG_CTRL_REG3_M 0x22
#define REG_OUT_X_L_M   0x28  // mag start

/* ===== Filter ===== */

static const float alpha_acc  = 0.2f;
static const float alpha_gyro = 0.2f;
static const float alpha_mag  = 0.2f;

/* ===== State ===== */

static float ax_off, ay_off, az_off;
static float gx_off, gy_off, gz_off;
static float mx_off, my_off, mz_off;

static float ax_f, ay_f, az_f;
static float gx_f, gy_f, gz_f;
static float mx_f, my_f, mz_f;

/* ===== Low level ===== */

static esp_err_t i2c_write_addr(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return i2c_master_write_to_device(
        I2C_PORT, addr, data, 2, pdMS_TO_TICKS(100)
    );
}

static esp_err_t i2c_read_addr(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(
        I2C_PORT, addr, &reg, 1, buf, len, pdMS_TO_TICKS(100)
    );
}

static esp_err_t i2c_write(uint8_t reg, uint8_t val)
{
    return i2c_write_addr(LSM6_ADDR, reg, val);
}

static esp_err_t i2c_read(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_read_addr(LSM6_ADDR, reg, buf, len);
}

/* ===== Raw read ===== */

static inline int16_t u8_to_i16(const uint8_t *p)
{
    return (int16_t)((p[1] << 8) | p[0]);
}

/* ===== Public API ===== */


void imu_init_hw()
{
    ESP_LOGI(TAG_IMU, "IMU initialization...");

    i2c_config_t conf{};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = SDA_PIN;
    conf.scl_io_num = SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ;

    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);

    // AG: auto-increment des registres (IF_INC=1)
    i2c_write(REG_CTRL3_C, 0x04);

    // AG: config d'origine
    i2c_write(REG_CTRL1_XL, 0x3C);
    i2c_write(REG_CTRL2_G,  0x4C);

    // MAG (LSM9DS1):
    // - CTRL_REG3_M = 0x00 -> mode continu
    // - CTRL_REG1_M : ODR + perf (valeur raisonnable par défaut)
    // - CTRL_REG2_M : full-scale (par défaut 4 gauss si 0x00)
    i2c_write_addr(MAG_ADDR, REG_CTRL_REG3_M, 0x00); // continuous-conversion
    i2c_write_addr(MAG_ADDR, REG_CTRL_REG1_M, 0x70); // ex: ODR ~ 10Hz + perf (selon datasheet)
    i2c_write_addr(MAG_ADDR, REG_CTRL_REG2_M, 0x00); // ±4 gauss

    ESP_LOGI(TAG_IMU, "IMU initialized (AG + MAG)");
}

void imu_calibrate()
{
    // "Comme Pololu" : on moyenne à l'arrêt pour obtenir des offsets.
    // Pour l'accéléro : on force Z à valoir +1g quand l'IMU est immobile.
    // Comme on ne connaît pas ici la sensibilité exacte (LSB/g), on estime 1g
    // à partir de la norme moyenne du vecteur accélération pendant la calibration.

    const int N = 500;                 // Pololu fait souvent ~500..1000 échantillons
    const TickType_t dt = pdMS_TO_TICKS(20);

    float ax_sum = 0, ay_sum = 0, az_sum = 0;
    float gx_sum = 0, gy_sum = 0, gz_sum = 0;
    float mx_sum = 0, my_sum = 0, mz_sum = 0;

    float one_g_sum = 0;               // estimation "1g" en counts bruts

    ESP_LOGI(TAG_IMU, "Calibration (Pololu-like)... keep IMU still");

    for (int i = 0; i < N; i++)
    {
        float ax, ay, az, gx, gy, gz, mx, my, mz;
        imu_read_raw(&ax, &ay, &az, &gx, &gy, &gz, &mx, &my, &mz);

        ax_sum += ax; ay_sum += ay; az_sum += az;
        gx_sum += gx; gy_sum += gy; gz_sum += gz;
        mx_sum += mx; my_sum += my; mz_sum += mz;

        // norme accel ~ 1g à l'arrêt (si pas d'accélération linéaire)
        const float norm = std::sqrt(ax*ax + ay*ay + az*az);
        one_g_sum += norm;

        vTaskDelay(dt);
    }

    const float ax_avg = ax_sum / N;
    const float ay_avg = ay_sum / N;
    const float az_avg = az_sum / N;

    const float gx_avg = gx_sum / N;
    const float gy_avg = gy_sum / N;
    const float gz_avg = gz_sum / N;

    const float mx_avg = mx_sum / N;
    const float my_avg = my_sum / N;
    const float mz_avg = mz_sum / N;

    const float one_g = one_g_sum / N; // "1g" estimé en unités brutes

    // Gyro: à l'arrêt => 0 dps, donc offset = moyenne
    gx_off = gx_avg;
    gy_off = gy_avg;
    gz_off = gz_avg;

    // Accel:
    // X et Y doivent être ~0g à plat => offset = moyenne
    ax_off = ax_avg;
    ay_off = ay_avg;

    // Z doit être ~ +1g (ou -1g selon montage).
    // On choisit le signe du "1g" à partir du signe mesuré sur Z.
    const float z_sign = (az_avg >= 0.0f) ? 1.0f : -1.0f;
    az_off = az_avg - z_sign * one_g;

    // Mag: hard-iron grossier (moyenne). Si tu ne veux "comme Pololu" strict,
    // tu peux commenter ces 3 lignes.
    mx_off = mx_avg;
    my_off = my_avg;
    mz_off = mz_avg;

    // Réinitialisation des états filtrés (comme tu faisais)
    ax_f = ay_f = az_f = 0;
    gx_f = gy_f = gz_f = 0;
    mx_f = my_f = mz_f = 0;

    ESP_LOGI(TAG_IMU,
             "Calibration done. Offsets: "
             "A(%.2f, %.2f, %.2f) G(%.2f, %.2f, %.2f) M(%.2f, %.2f, %.2f) one_g=%.2f",
             ax_off, ay_off, az_off,
             gx_off, gy_off, gz_off,
             mx_off, my_off, mz_off,
             one_g);
}

/* Nouvelle signature: ajoute mx,my,mz */
void imu_read_raw(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz)
{
    uint8_t gbuf[6];
    uint8_t abuf[6];
    uint8_t mbuf[6];

    i2c_read(REG_OUTX_L_G, gbuf, 6);
    i2c_read(REG_OUTX_L_A, abuf, 6);

    // MAG: auto-increment via bit7 de l'adresse (reg | 0x80)
    i2c_read_addr(MAG_ADDR, (uint8_t)(REG_OUT_X_L_M | 0x80), mbuf, 6);

    int16_t gx_r = u8_to_i16(&gbuf[0]);
    int16_t gy_r = u8_to_i16(&gbuf[2]);
    int16_t gz_r = u8_to_i16(&gbuf[4]);

    int16_t ax_r = u8_to_i16(&abuf[0]);
    ax_r >>= 4;
    int16_t ay_r = u8_to_i16(&abuf[2]);
    ay_r >>= 4;
    int16_t az_r = u8_to_i16(&abuf[4]);
    az_r >>= 4;

    int16_t mx_r = u8_to_i16(&mbuf[0]);
    int16_t my_r = u8_to_i16(&mbuf[2]);
    int16_t mz_r = u8_to_i16(&mbuf[4]);

    *gx = gx_r; *gy = gy_r; *gz = gz_r;
    *ax = ax_r; *ay = ay_r; *az = az_r;
    *mx = mx_r; *my = my_r; *mz = mz_r;
}

void imu_read_calibrated(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz)
{
    float rax, ray, raz, rgx, rgy, rgz, rmx, rmy, rmz;
    imu_read_raw(&rax, &ray, &raz, &rgx, &rgy, &rgz, &rmx, &rmy, &rmz);

    *ax = rax - ax_off;
    *ay = ray - ay_off;
    *az = raz - az_off;

    *gx = rgx - gx_off;
    *gy = rgy - gy_off;
    *gz = rgz - gz_off;

    *mx = rmx - mx_off;
    *my = rmy - my_off;
    *mz = rmz - mz_off;
}