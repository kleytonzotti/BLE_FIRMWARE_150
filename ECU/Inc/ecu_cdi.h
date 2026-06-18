/**
 * ecu_cdi.h — CDI (Capacitor Discharge Ignition) para motor carburado.
 *
 * No modo CDI:
 *  - Conversor DC-DC boost (TIM17 PWM) carrega capacitor a ~350V
 *  - SCR (tiristor) dispara ao sinal do STM32 via optocoupler PC817
 *  - A energia do capacitor descarrega no primário da bobina CDI
 *  - Centelha muito rápida (~1µs) — ideal para altas rotações
 *  - Não usa injeção — o carburador cuida do combustível
 *  - Curva de avanço: tabela RPM → graus BTDC (32 pontos)
 */
#ifndef ECU_CDI_H
#define ECU_CDI_H

#include "ecu_types.h"

void    ECU_CDI_Init(void);
void    ECU_CDI_SetMode(uint8_t enable);
void    ECU_CDI_BoostStart(void);
void    ECU_CDI_BoostStop(void);
uint8_t ECU_CDI_IsCapacitorReady(void);
void    ECU_CDI_ScheduleFire(uint8_t cdi_ch, uint32_t delay_us);
void    ECU_CDI_Calculate(volatile EngineState_t *eng, const EcuProfile_t *prof);
void    ECU_CDI_Fire(uint8_t cdi_ch);

#endif /* ECU_CDI_H */
