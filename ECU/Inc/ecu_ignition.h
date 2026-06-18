/**
 * ecu_ignition.h — Cálculo e controle de ignição (bobina indutiva / DIS).
 */
#ifndef ECU_IGNITION_H
#define ECU_IGNITION_H

#include "ecu_types.h"

void ECU_Ignition_Init(void);
void ECU_Ignition_Calculate(volatile EngineState_t *eng, const EcuProfile_t *prof);
void ECU_Ignition_ScheduleDwell(uint8_t coil, uint32_t tick_start);
void ECU_Ignition_ScheduleFire(uint8_t coil, uint32_t tick_fire);
void ECU_Ignition_CutAll(void);
/* Recua avanço na detecção de knock. severity: 0=nenhum, 1=leve, 2=moderado, 3=severo */
void ECU_Ignition_KnockRetard(volatile EngineState_t *eng, uint8_t severity);

#endif /* ECU_IGNITION_H */
