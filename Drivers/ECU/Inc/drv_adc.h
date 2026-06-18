/**
 * drv_adc.h — ADC via DMA circular, acesso direto a registradores STM32WB55.
 *
 * ADC1 + DMA1 em modo circular contínuo.
 * Oversample de ADC_OVERSAMPLE leituras por canal (média em software).
 * ADC2 usado exclusivamente para knock (alta velocidade).
 */
#ifndef DRV_ADC_H
#define DRV_ADC_H

#include <stdint.h>
#include "ecu_config.h"

/* Buffer preenchido automaticamente pelo DMA — não escrever diretamente */
extern volatile uint16_t g_adc_dma_buf[ADC_CHANNELS * ADC_OVERSAMPLE];

void     DRV_ADC_Init(void);
uint16_t DRV_ADC_GetRaw(uint8_t channel);
uint16_t DRV_ADC_RawToMv(uint16_t raw);
uint16_t DRV_ADC_ReadSingle(uint8_t channel);  /* Bloqueante, para diagnóstico */

void     DRV_ADC_Knock_Init(void);
uint16_t DRV_ADC_Knock_GetPeak(void);

#endif /* DRV_ADC_H */
