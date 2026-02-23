#include "drivers/imu.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG_IMU "IMU"

/* ===== Hardware ===== */

#define I2C_PORT I2C_NUM_0
#define SDA_PIN  5
#define SCL_PIN  6
#define I2C_FREQ 400000   // comme ton ancien code (tu peux mettre 100000 si besoin)

/* I2C addresses (7-bit) */
#define LSM6_ADDR    0x6B
#define LIS3MDL_ADDR 0x1E

/* ===== Registers ===== */
/* LSM6 */
#define REG_CTRL1_XL   0x10
#define REG_CTRL2_G    0x11
#define REG_OUTX_L_G   0x22   // gyro start, then accel follows if contiguous on your LSM6 variant

/* LIS3MDL */
#define REG_LIS3_CTRL1   0x20
#define REG_LIS3_CTRL2   0x21
#define REG_LIS3_CTRL3   0x22
#define REG_LIS3_OUT_X_L 0x28

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

static esp_err_t i2c_write(uint8_t dev, uint8_t reg, uint8_t val)
{
    uint8_t data[2] = { reg, val };
    return i2c_master_write_to_device(
        I2C_PORT, dev, data, 2, pdMS_TO_TICKS(100)
    );
}

static esp_err_t i2c_read(uint8_t dev, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(
        I2C_PORT, dev, &reg, 1, buf, len, pdMS_TO_TICKS(100)
    );
}

static inline int16_t le_u8_to_i16(uint8_t lo, uint8_t hi)
{
    return (int16_t)(((uint16_t)hi << 8) | lo);
}

/* ===== Raw read ===== */

static void imu_read_raw_internal(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz)
{
    // LSM6: read 12 bytes from OUTX_L_G (gyro + accel)
    uint8_t b[12];
    if (i2c_read(LSM6_ADDR, REG_OUTX_L_G, b, 12) == ESP_OK)
    {
        int16_t gx_r = le_u8_to_i16(b[0],  b[1]);
        int16_t gy_r = le_u8_to_i16(b[2],  b[3]);
        int16_t gz_r = le_u8_to_i16(b[4],  b[5]);

        // Accel: some LSM6 deliver 16-bit; in your earlier “new code” you used >>4.
        // To keep behavior stable for your ML model, we keep >>4 here too.
        int16_t ax_r = (int16_t)(le_u8_to_i16(b[6],  b[7])  >> 4);
        int16_t ay_r = (int16_t)(le_u8_to_i16(b[8],  b[9])  >> 4);
        int16_t az_r = (int16_t)(le_u8_to_i16(b[10], b[11]) >> 4);

        *gx = (float)gx_r; *gy = (float)gy_r; *gz = (float)gz_r;
        *ax = (float)ax_r; *ay = (float)ay_r; *az = (float)az_r;
    }

    // LIS3MDL: read 6 bytes from OUT_X_L with auto-increment
    uint8_t m[6];
    if (i2c_read(LIS3MDL_ADDR, (uint8_t)(REG_LIS3_OUT_X_L | 0x80), m, 6) == ESP_OK)
    {
        int16_t mx_r = le_u8_to_i16(m[0], m[1]);
        int16_t my_r = le_u8_to_i16(m[2], m[3]);
        int16_t mz_r = le_u8_to_i16(m[4], m[5]);

        *mx = (float)mx_r; *my = (float)my_r; *mz = (float)mz_r;
    }
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

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0));

    // LSM6 init (valeurs de ton nouveau code)
    ESP_ERROR_CHECK(i2c_write(LSM6_ADDR, REG_CTRL1_XL, 0x3C));
    ESP_ERROR_CHECK(i2c_write(LSM6_ADDR, REG_CTRL2_G,  0x4C));

    // LIS3MDL init (valeurs de ton nouveau code)
    ESP_ERROR_CHECK(i2c_write(LIS3MDL_ADDR, REG_LIS3_CTRL1, 0x70));
    ESP_ERROR_CHECK(i2c_write(LIS3MDL_ADDR, REG_LIS3_CTRL2, 0x00));
    ESP_ERROR_CHECK(i2c_write(LIS3MDL_ADDR, REG_LIS3_CTRL3, 0x00));

    ESP_LOGI(TAG_IMU, "IMU initialized");
}

void imu_calibrate()
{
    const int N = 500;

    ax_off = ay_off = az_off = 0;
    gx_off = gy_off = gz_off = 0;
    mx_off = my_off = mz_off = 0;

    ESP_LOGI(TAG_IMU, "Calibration...");

    for (int i = 0; i < N; i++)
    {
        float ax, ay, az, gx, gy, gz, mx, my, mz;
        imu_read_raw_internal(&ax,&ay,&az,&gx,&gy,&gz,&mx,&my,&mz);

        ax_off += ax; ay_off += ay; az_off += az;
        gx_off += gx; gy_off += gy; gz_off += gz;
        mx_off += mx; my_off += my; mz_off += mz;

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    ax_off/=N; ay_off/=N; az_off/=N;
    gx_off/=N; gy_off/=N; gz_off/=N;
    mx_off/=N; my_off/=N; mz_off/=N;

    ax_f = ay_f = az_f = 0;
    gx_f = gy_f = gz_f = 0;
    mx_f = my_f = mz_f = 0;

    ESP_LOGI(TAG_IMU, "Calibration done");
}

/* ===== Legacy 6 axes API ===== */

void imu_read_raw(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz)
{
    float mx, my, mz;
    imu_read_raw_internal(ax,ay,az,gx,gy,gz,&mx,&my,&mz);
}

void imu_read_filtered(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz)
{
    float rax, ray, raz, rgx, rgy, rgz;
    float rmx, rmy, rmz;

    imu_read_raw_internal(&rax,&ray,&raz,&rgx,&rgy,&rgz,&rmx,&rmy,&rmz);

    rax -= ax_off; ray -= ay_off; raz -= az_off;
    rgx -= gx_off; rgy -= gy_off; rgz -= gz_off;
    rmx -= mx_off; rmy -= my_off; rmz -= mz_off;

    ax_f = alpha_acc  * rax + (1 - alpha_acc)  * ax_f;
    ay_f = alpha_acc  * ray + (1 - alpha_acc)  * ay_f;
    az_f = alpha_acc  * raz + (1 - alpha_acc)  * az_f;

    gx_f = alpha_gyro * rgx + (1 - alpha_gyro) * gx_f;
    gy_f = alpha_gyro * rgy + (1 - alpha_gyro) * gy_f;
    gz_f = alpha_gyro * rgz + (1 - alpha_gyro) * gz_f;

    // mag filtered too (not returned in legacy, but kept for 9DOF API)
    mx_f = alpha_mag  * rmx + (1 - alpha_mag)  * mx_f;
    my_f = alpha_mag  * rmy + (1 - alpha_mag)  * my_f;
    mz_f = alpha_mag  * rmz + (1 - alpha_mag)  * mz_f;

    *ax = ax_f; *ay = ay_f; *az = az_f;
    *gx = gx_f; *gy = gy_f; *gz = gz_f;
}

/* ===== Optional 9DOF API ===== */

void imu_read_raw_9dof(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz)
{
    imu_read_raw_internal(ax,ay,az,gx,gy,gz,mx,my,mz);
}

void imu_read_filtered_9dof(
    float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz)
{
    float rax, ray, raz, rgx, rgy, rgz, rmx, rmy, rmz;

    imu_read_raw_internal(&rax,&ray,&raz,&rgx,&rgy,&rgz,&rmx,&rmy,&rmz);

    rax -= ax_off; ray -= ay_off; raz -= az_off;
    rgx -= gx_off; rgy -= gy_off; rgz -= gz_off;
    rmx -= mx_off; rmy -= my_off; rmz -= mz_off;

    ax_f = alpha_acc  * rax + (1 - alpha_acc)  * ax_f;
    ay_f = alpha_acc  * ray + (1 - alpha_acc)  * ay_f;
    az_f = alpha_acc  * raz + (1 - alpha_acc)  * az_f;

    gx_f = alpha_gyro * rgx + (1 - alpha_gyro) * gx_f;
    gy_f = alpha_gyro * rgy + (1 - alpha_gyro) * gy_f;
    gz_f = alpha_gyro * rgz + (1 - alpha_gyro) * gz_f;

    mx_f = alpha_mag  * rmx + (1 - alpha_mag)  * mx_f;
    my_f = alpha_mag  * rmy + (1 - alpha_mag)  * my_f;
    mz_f = alpha_mag  * rmz + (1 - alpha_mag)  * mz_f;

    *ax = ax_f; *ay = ay_f; *az = az_f;
    *gx = gx_f; *gy = gy_f; *gz = gz_f;
    *mx = mx_f; *my = my_f; *mz = mz_f;
}