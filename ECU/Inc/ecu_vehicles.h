/**
 * ecu_vehicles.h — Perfis padrão de veículos com tabelas VE e ignição.
 */
#ifndef ECU_VEHICLES_H
#define ECU_VEHICLES_H

#include "ecu_types.h"

void        ECU_LoadDefaultProfile(VehicleProfileId_t id);
void        ECU_Profile_SetDefaults(EcuProfile_t *prof);
void        ECU_Profile_CalcCRC(EcuProfile_t *p);
uint8_t     ECU_Profile_IsValid(const EcuProfile_t *prof);
void        ECU_Profile_Copy(EcuProfile_t *dst, const EcuProfile_t *src);

#endif /* ECU_VEHICLES_H */
