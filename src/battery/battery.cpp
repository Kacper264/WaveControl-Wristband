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
    const int samples = 32;
    int values[samples];

    // 1. Acquisition
    for (int i = 0; i < samples; i++)
    {
        values[i] = adc1_get_raw((adc1_channel_t)BAT_ADC_CHANNEL);
        esp_rom_delay_us(200); // petit délai pour stabiliser
    }

    // 2. Tri simple (bubble sort léger vu faible taille)
    for (int i = 0; i < samples - 1; i++)
    {
        for (int j = i + 1; j < samples; j++)
        {
            if (values[j] < values[i])
            {
                int tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }

    // 3. Moyenne sans extrêmes (trimmed mean)
    int sum = 0;
    int count = 0;
    for (int i = 2; i < samples - 2; i++) // enlève 2 plus petits et 2 plus grands
    {
        sum += values[i];
        count++;
    }

    int raw_avg = sum / count;

    // 4. Conversion en tension
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw_avg, &adc_chars);
    float v_adc = (float)mv / 1000.0f;
    float v_bat = v_adc * BAT_DIVIDER_RATIO;

    // 5. Filtre exponentiel (EMA)
    static float filtered_vbat = 0.0f;
    const float alpha = 0.2f; // 0.1 = très lisse, 0.3 = plus réactif

    if (filtered_vbat == 0.0f)
        filtered_vbat = v_bat; // init
    else
        filtered_vbat = alpha * v_bat + (1.0f - alpha) * filtered_vbat;

    ESP_LOGI("BATTERY", "Raw avg: %d, Voltage: %.3f V (filtered: %.3f V)", raw_avg, v_bat, filtered_vbat);

    return filtered_vbat;
}

uint8_t battery_read_percent(void)
{
    float v = battery_read_voltage();
    float pct = (v - VBAT_MIN) * 100.0f / (VBAT_MAX - VBAT_MIN);
    pct = clampf(pct, 0.0f, 100.0f);
    ESP_LOGI("BATTERY", "Battery level: %.1f%%", pct);
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
    else if (delta < threshold)
    {
        last_state = false;
    }

    return last_state;
}
