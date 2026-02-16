#include "battery/battery.h"
#include "common/strings_constants.h"
extern "C" {
#include "driver/adc.h"
#include "esp_adc_cal.h"
}

static esp_adc_cal_characteristics_t adc_chars;

static float clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

void battery_init(void)
{
    adc1_config_width(BAT_ADC_WIDTH);
    adc1_config_channel_atten((adc1_channel_t)BAT_ADC_CHANNEL, BAT_ADC_ATTEN);

    // 1100mV: valeur “classique” vref. Si tu veux plus précis -> eFuse / calibration dédiée.
    esp_adc_cal_characterize(ADC_UNIT_1, BAT_ADC_ATTEN, BAT_ADC_WIDTH, 1100, &adc_chars);
}

float battery_read_voltage(void)
{
    int raw = adc1_get_raw((adc1_channel_t)BAT_ADC_CHANNEL);
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

    float v_adc = (float)mv / 1000.0f;
    float v_bat = v_adc * BAT_DIVIDER_RATIO;
    return v_bat;
}

uint8_t battery_read_percent(void)
{
    float v = battery_read_voltage();
    float pct = (v - VBAT_MIN) * 100.0f / (VBAT_MAX - VBAT_MIN);
    pct = clampf(pct, 0.0f, 100.0f);
    return (uint8_t)(pct + 0.5f);
}

bool is_on_sector(float vbat)
{
    static float prev_vbat = 0.0f;
    static bool initialized = false;
    static bool last_state = false;

    const float threshold = 0.01f; // 10 mV

    if (!initialized)
    {
        prev_vbat = vbat;
        initialized = true;
        return false;
    }

    float delta = vbat - prev_vbat;
    prev_vbat = vbat;

    if (delta > threshold)
    {
        last_state = true;
    }
    else if (delta < -threshold)
    {
        last_state = false;
    }

    return last_state;
}
