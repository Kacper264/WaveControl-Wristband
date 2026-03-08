#include "include.h"

static esp_adc_cal_characteristics_t adc_chars;

/*
 * Fonction utilitaire : limite une valeur entre un minimum et un maximum.
 */
static float clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

/*
 * Initialisation de l'ADC pour la mesure de la batterie.
 * Configure la résolution, l'atténuation et la calibration de l'ADC.
 */
void battery_init(void)
{
    adc1_config_width(BAT_ADC_WIDTH);
    adc1_config_channel_atten((adc1_channel_t)BAT_ADC_CHANNEL, BAT_ADC_ATTEN);

    // Calibration ADC avec la tension de référence configurée
    esp_adc_cal_characterize(ADC_UNIT_1, BAT_ADC_ATTEN, BAT_ADC_WIDTH, BAT_ADC_VREF_MV, &adc_chars);
}

/*
 * Mesure la tension batterie en appliquant plusieurs filtres :
 * 1. multi-échantillonnage ADC
 * 2. suppression des valeurs extrêmes
 * 3. filtre exponentiel (EMA)
 */
float battery_read_voltage(void)
{
    int values[BAT_ADC_SAMPLES];

    // 1. Acquisition de plusieurs échantillons ADC
    for (int i = 0; i < BAT_ADC_SAMPLES; i++)
    {
        values[i] = adc1_get_raw((adc1_channel_t)BAT_ADC_CHANNEL);
        esp_rom_delay_us(BAT_ADC_STABILIZE_DELAY_US); // délai pour stabiliser la lecture
    }

    // 2. Tri simple (bubble sort) pour pouvoir supprimer les valeurs extrêmes
    for (int i = 0; i < BAT_ADC_SAMPLES - 1; i++)
    {
        for (int j = i + 1; j < BAT_ADC_SAMPLES; j++)
        {
            if (values[j] < values[i])
            {
                int tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }

    // 3. Moyenne sans les valeurs extrêmes (trimmed mean)
    int sum = 0;
    int count = 0;
    for (int i = 2; i < BAT_ADC_SAMPLES - 2; i++) // ignore les 2 plus faibles et 2 plus fortes
    {
        sum += values[i];
        count++;
    }

    int raw_avg = sum / count;

    // 4. Conversion valeur ADC -> tension
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw_avg, &adc_chars);
    float v_adc = (float)mv / 1000.0f;
    float v_bat = v_adc * BAT_DIVIDER_RATIO; // correction du pont diviseur

    // 5. Filtre exponentiel (EMA) pour lisser la tension
    static float filtered_vbat = 0.0f;
    const float alpha = BATTERY_FILTER_ALPHA;

    if (filtered_vbat == 0.0f)
        filtered_vbat = v_bat; // initialisation
    else
        filtered_vbat = alpha * v_bat + (1.0f - alpha) * filtered_vbat;

    ESP_LOGI("BATTERY", "Raw avg: %d, Voltage: %.3f V (filtered: %.3f V)", raw_avg, v_bat, filtered_vbat);

    return filtered_vbat;
}

/*
 * Convertit la tension batterie en pourcentage.
 * Utilise la plage LiPo définie par VBAT_MIN et VBAT_MAX.
 */
uint8_t battery_read_percent(void)
{
    float v = battery_read_voltage();
    float pct = (v - VBAT_MIN) * 100.0f / (VBAT_MAX - VBAT_MIN);
    pct = clampf(pct, 0.0f, 100.0f);

    ESP_LOGI("BATTERY", "Battery level: %.1f%%", pct);

    if (pct > 50.0f) {
        neopixel_set_idle_pixel(1, 0, 255, 0);       // vert
    }
    else if (pct > 10.0f) {
        neopixel_set_idle_pixel(1, 255, 165, 0);     // orange
    }
    else {
        neopixel_set_idle_pixel(1, 255, 0, 0);       // rouge
    }

    return (uint8_t)(pct + 0.5f);
}

/*
 * Détecte si l'appareil est alimenté par le secteur
 * en observant une augmentation de la tension batterie.
 */
bool is_on_sector(float vbat)
{
    static float prev_vbat = 0.0f;
    static bool initialized = false;
    static bool last_state = false;

    const float threshold = SECTOR_DETECTION_THRESHOLD;

    // Initialisation lors du premier appel
    if (!initialized)
    {
        prev_vbat = vbat;
        initialized = true;
        return false;
    }

    float delta = vbat - prev_vbat;
    prev_vbat = vbat;

    // Si la tension augmente au-delà du seuil -> probablement en charge
    if (delta >= threshold)
    {
        last_state = true;
    }
    else if (delta <= -threshold)
    {
        last_state = false;
    }
    ESP_LOGI("BATTERY", "Vbat: %.3f V, Delta: %.3f V, On sector: %s", vbat, delta, last_state ? "YES" : "NO");
    return last_state;
}