/**
 * ecu_sensors.c — Leitura, conversão e validação de todos os sensores.
 */

#include "ecu_sensors.h"
#include "drv_adc.h"
#include "ecu_config.h"

volatile AdcData_t g_adc = {0};

/* Tabela NTC automotiva Bosch (10 pontos, tensão em mV com pull-up 2.2kΩ @ 3.3V)
 * Índice: [-40,-20,0,20,40,60,80,90,100,120] °C × 10 */
static const uint16_t s_ntc_temp_x10[10] = {
    (uint16_t)((-400) + 4000),  /* Offset +4000 para evitar negativo */
    (uint16_t)((-200) + 4000),
    (uint16_t)(   0  + 4000),
    (uint16_t)( 200  + 4000),
    (uint16_t)( 400  + 4000),
    (uint16_t)( 600  + 4000),
    (uint16_t)( 800  + 4000),
    (uint16_t)( 900  + 4000),
    (uint16_t)(1000  + 4000),
    (uint16_t)(1200  + 4000),
};
/* Tensão correspondente com pull-up 2.2kΩ, Vref 3.3V (em mV) */
static const uint16_t s_ntc_mv[10] = { 3250, 3150, 2900, 2500, 1900,
                                         1350,  850,  630,  480,  270 };

void ECU_Sensors_Init(void)
{
    /* ADC já iniciado em drv_adc.c via DMA circular */
}

uint16_t ECU_Sensors_AdcToMv(uint16_t raw)
{
    return (uint16_t)((uint32_t)raw * ADC_VREF_MV / 4095U);
}

int16_t ECU_Sensors_NtcToTemp(uint16_t mv, uint16_t pullup_ohm)
{
    /* Busca na tabela NTC por interpolação */
    (void)pullup_ohm;

    /* Tensão decresce com temperatura — busca do maior para o menor */
    if (mv >= s_ntc_mv[0]) return (int16_t)(s_ntc_temp_x10[0] - 4000);
    if (mv <= s_ntc_mv[9]) return (int16_t)(s_ntc_temp_x10[9] - 4000);

    for (uint8_t i = 0; i < 9; i++) {
        if (mv <= s_ntc_mv[i] && mv >= s_ntc_mv[i + 1]) {
            /* Interpolação linear entre os pontos */
            int32_t t0 = (int32_t)s_ntc_temp_x10[i]     - 4000;
            int32_t t1 = (int32_t)s_ntc_temp_x10[i + 1] - 4000;
            int32_t v0 = (int32_t)s_ntc_mv[i];
            int32_t v1 = (int32_t)s_ntc_mv[i + 1];
            if (v0 == v1) return (int16_t)t0;
            int32_t t = t0 + (t1 - t0) * ((int32_t)mv - v0) / (v1 - v0);
            return (int16_t)t;
        }
    }
    return 200;  /* 20.0°C como fallback */
}

uint16_t ECU_Sensors_MapMvToKpa(uint16_t mv, const EcuProfile_t *prof)
{
    /* Mapeamento linear: mv_min → 0 kPa, mv_max → max_kpa */
    if (mv <= prof->map_min_mv) return 0;
    if (mv >= prof->map_max_mv) return prof->map_max_kpa;

    uint32_t kpa_x10 = (uint32_t)(mv - prof->map_min_mv)
                       * (uint32_t)prof->map_max_kpa
                       / (uint32_t)(prof->map_max_mv - prof->map_min_mv);
    return (uint16_t)kpa_x10;
}

uint16_t ECU_Sensors_TpsMvToPct(uint16_t mv, const EcuProfile_t *prof)
{
    if (mv <= prof->tps_min_mv) return 0;
    if (mv >= prof->tps_max_mv) return 1000;

    return (uint16_t)((uint32_t)(mv - prof->tps_min_mv) * 1000U
                      / (uint32_t)(prof->tps_max_mv - prof->tps_min_mv));
}

void ECU_Sensors_UpdateFlexFreq(uint32_t freq_hz, volatile EngineState_t *eng)
{
    /* Continental CGAS: 50Hz = E0, 150Hz = E100 */
    if (freq_hz < 50)  freq_hz = 50;
    if (freq_hz > 150) freq_hz = 150;
    eng->flex_pct = (uint16_t)((freq_hz - 50) * 1000U / 100U);  /* 0-1000 */
}

void ECU_Sensors_Update(volatile EngineState_t *eng, const EcuProfile_t *prof)
{
    /* Atualiza buffer de mV a partir do DMA */
    for (uint8_t ch = 0; ch < ADC_CHANNELS; ch++) {
        g_adc.raw[ch] = DRV_ADC_GetRaw(ch);
        g_adc.mv[ch]  = ECU_Sensors_AdcToMv(g_adc.raw[ch]);
    }

    /* TPS → 0-1000 */
    eng->tps_pct = ECU_Sensors_TpsMvToPct(g_adc.mv[ADC_CH_TPS], prof);

    /* MAP → kPa × 10 */
    eng->map_kpa = ECU_Sensors_MapMvToKpa(g_adc.mv[ADC_CH_MAP], prof);

    /* CLT → °C × 10 */
    eng->coolant_c = ECU_Sensors_NtcToTemp(g_adc.mv[ADC_CH_CLT], 2200);

    /* IAT → °C × 10 */
    eng->iat_c = ECU_Sensors_NtcToTemp(g_adc.mv[ADC_CH_IAT], 2200);

    /* Tensão da bateria: divisor 39k/10k → × 4.9 */
    eng->batt_mv = (uint16_t)((uint32_t)g_adc.mv[ADC_CH_BATT] * 49U / 10U);

    /* O2 analógico (AEM 30-2853 ou similar: 0V=AFR10, 5V=AFR20)
     * Ajuste para 0-3.3V: AFR = 10.0 + 10.0 × mv / 3300 × 10 */
    if (prof->features & FEAT_WIDEBAND_O2) {
        uint16_t o2mv = g_adc.mv[ADC_CH_O2];
        /* Interpolação linear entre min_afr e max_afr */
        if (o2mv <= prof->o2_min_mv) {
            eng->o2_afr_x10 = prof->o2_min_afr_x10;
        } else if (o2mv >= prof->o2_max_mv) {
            eng->o2_afr_x10 = prof->o2_max_afr_x10;
        } else {
            uint32_t range = prof->o2_max_mv - prof->o2_min_mv;
            eng->o2_afr_x10 = (uint16_t)(
                prof->o2_min_afr_x10 +
                (uint32_t)(prof->o2_max_afr_x10 - prof->o2_min_afr_x10)
                * (o2mv - prof->o2_min_mv) / range
            );
        }
    }

    /* Boost → kPa × 10 (reutiliza MapMvToKpa com escala maior) */
    if (prof->features & FEAT_BOOST_CTRL) {
        eng->boost_kpa = ECU_Sensors_MapMvToKpa(g_adc.mv[ADC_CH_BOOST], prof);
    }

    /* Pressão do óleo (sensor 0-10 bar linear) */
    if (prof->features & FEAT_OIL_PRESSURE) {
        eng->oil_pressure_kpa = (uint16_t)((uint32_t)g_adc.mv[ADC_CH_OIL] * 1000U / 3300U);
    }

    /* Pressão do combustível */
    if (prof->features & FEAT_FUEL_PRESSURE) {
        eng->fuel_pressure_kpa = (uint16_t)((uint32_t)g_adc.mv[ADC_CH_FUEL_PRESS] * 1000U / 3300U);
    }

    ECU_Sensors_ValidateAll(eng);
}

void ECU_Sensors_ValidateAll(volatile EngineState_t *eng)
{
    /* TPS */
    if (g_adc.mv[ADC_CH_TPS] < SENSOR_TPS_MIN_MV || g_adc.mv[ADC_CH_TPS] > SENSOR_TPS_MAX_MV) {
        FAULT_SET(eng->active_faults, FAULT_TPS_OPEN);
    } else {
        FAULT_CLEAR(eng->active_faults, FAULT_TPS_OPEN);
        FAULT_CLEAR(eng->active_faults, FAULT_TPS_RANGE);
    }

    /* MAP */
    if (g_adc.mv[ADC_CH_MAP] < SENSOR_MAP_MIN_MV || g_adc.mv[ADC_CH_MAP] > SENSOR_MAP_MAX_MV) {
        FAULT_SET(eng->active_faults, FAULT_MAP_OPEN);
    } else {
        FAULT_CLEAR(eng->active_faults, FAULT_MAP_OPEN);
    }

    /* CLT */
    if (g_adc.mv[ADC_CH_CLT] < SENSOR_CLT_MIN_MV || g_adc.mv[ADC_CH_CLT] > SENSOR_CLT_MAX_MV) {
        FAULT_SET(eng->active_faults, FAULT_CLT_OPEN);
    } else {
        FAULT_CLEAR(eng->active_faults, FAULT_CLT_OPEN);
    }

    /* IAT */
    if (g_adc.mv[ADC_CH_IAT] < SENSOR_IAT_MIN_MV || g_adc.mv[ADC_CH_IAT] > SENSOR_IAT_MAX_MV) {
        FAULT_SET(eng->active_faults, FAULT_IAT_OPEN);
    } else {
        FAULT_CLEAR(eng->active_faults, FAULT_IAT_OPEN);
    }

    /* Bateria */
    if (eng->batt_mv < ECU_MIN_BATT_MV) {
        FAULT_SET(eng->active_faults, FAULT_BATT_LOW);
    } else if (eng->batt_mv > ECU_MAX_BATT_MV) {
        FAULT_SET(eng->active_faults, FAULT_BATT_HIGH);
    } else {
        FAULT_CLEAR(eng->active_faults, FAULT_BATT_LOW);
        FAULT_CLEAR(eng->active_faults, FAULT_BATT_HIGH);
    }
}
