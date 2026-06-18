/**
 * ecu_idle.h — Controle de marcha lenta via IAC (stepper ou solenóide duty).
 */
#ifndef ECU_IDLE_H
#define ECU_IDLE_H

#include "ecu_types.h"

void    ECU_Idle_Init(IacType_t type);
void    ECU_Idle_PrePosition(int16_t coolant_c_x10);
void    ECU_Idle_Update(volatile EngineState_t *eng, const EcuProfile_t *prof);
void    ECU_Idle_SetTarget(uint16_t target_rpm);
void    ECU_Idle_StepOpen(uint8_t steps);
void    ECU_Idle_StepClose(uint8_t steps);
int16_t ECU_Idle_GetPosition(void);
void    ECU_Idle_Reset(void);

#endif /* ECU_IDLE_H */
