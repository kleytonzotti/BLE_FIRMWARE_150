/**
 * ecu_fuel.h — Cálculo e controle de injeção de combustível.
 */
#ifndef ECU_FUEL_H
#define ECU_FUEL_H

#include "ecu_types.h"

void    ECU_Fuel_Init(void);
void    ECU_Fuel_Calculate(volatile EngineState_t *eng, const EcuProfile_t *prof);
void    ECU_Fuel_OpenInjector(uint8_t cyl);
void    ECU_Fuel_CloseInjector(uint8_t cyl);
void    ECU_Fuel_Cut(void);
void    ECU_Fuel_Resume(void);
uint8_t ECU_Fuel_IsCut(void);
void    ECU_Fuel_UpdateClosedLoop(volatile EngineState_t *eng, const EcuProfile_t *prof);

#endif /* ECU_FUEL_H */
