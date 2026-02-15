#include "battery/battery.h"

extern "C" {
#include "driver/adc.h"
#include "esp_adc_cal.h"
}

#ifndef BAT_ADC_CHANNEL
#define BAT_ADC_CHANNEL      ADC1_CHANNEL_0
#endif
#ifndef BAT_ADC_ATTEN
#define BAT_ADC_ATTEN        ADC_ATTEN_DB_11
#endif
#ifndef BAT_ADC_WIDTH
#define BAT_ADC_WIDTH        ADC_WIDTH_BIT_12
#endif
#ifndef BAT_DIVIDER_RATIO
#define BAT_DIVIDER_RATIO    2.0f
#endif
#ifndef VBAT_MIN
#define VBAT_MIN             3.30f
#endif
#ifndef VBAT_MAX
#define VBAT_MAX             4.20f
#endif

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
