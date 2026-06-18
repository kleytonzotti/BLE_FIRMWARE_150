/**
 * ecu_sensors.h — Leitura e calibração de sensores via ADC.
 */
#ifndef ECU_SENSORS_H
#define ECU_SENSORS_H

#include "ecu_types.h"

typedef struct {
    uint16_t mv[ADC_CHANNELS];   /* Tensão em mV por canal */
    uint16_t raw[ADC_CHANNELS];  /* Valor bruto ADC 0-4095 */
    uint32_t timestamp_ms;
} AdcData_t;

extern volatile AdcData_t g_adc;

void     ECU_Sensors_Init(void);
void     ECU_Sensors_Update(volatile EngineState_t *eng, const EcuProfile_t *prof);
uint16_t ECU_Sensors_AdcToMv(uint16_t raw);
int16_t  ECU_Sensors_NtcToTemp(uint16_t mv, uint16_t pullup_ohm);
uint16_t ECU_Sensors_MapMvToKpa(uint16_t mv, const EcuProfile_t *prof);
uint16_t ECU_Sensors_TpsMvToPct(uint16_t mv, const EcuProfile_t *prof);
void     ECU_Sensors_UpdateFlexFreq(uint32_t freq_hz, volatile EngineState_t *eng);
void     ECU_Sensors_ValidateAll(volatile EngineState_t *eng);

#endif /* ECU_SENSORS_H */
