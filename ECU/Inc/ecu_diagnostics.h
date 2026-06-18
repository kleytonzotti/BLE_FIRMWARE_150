/**
 * ecu_diagnostics.h — Diagnósticos do motor e log de falhas.
 */
#ifndef ECU_DIAGNOSTICS_H
#define ECU_DIAGNOSTICS_H

#include "ecu_types.h"

typedef struct {
    uint32_t fault_code;
    uint32_t timestamp_s;
    uint16_t rpm_at_fault;
    uint8_t  engine_state;
    uint8_t  count;
} FaultLogEntry_t;

#define FAULT_LOG_MAX_ENTRIES   32

void    ECU_Diag_Init(void);
void    ECU_Diag_Update(volatile EngineState_t *eng, const EcuProfile_t *prof);
void    ECU_Diag_SetFault(volatile EngineState_t *eng, EcuFaultCode_t code);
void    ECU_Diag_ClearFault(volatile EngineState_t *eng, EcuFaultCode_t code);
void    ECU_Diag_ClearAll(volatile EngineState_t *eng);
void    ECU_Diag_GetFirmwareInfo(FirmwareInfo_t *info, const EcuProfile_t *prof);
uint8_t ECU_Diag_IsLimpMode(const volatile EngineState_t *eng);
void    ECU_Diag_CheckOvertemp(volatile EngineState_t *eng, const EcuProfile_t *prof);
void    ECU_Diag_CheckBattery(volatile EngineState_t *eng);
void    ECU_Diag_LogFault(EcuFaultCode_t code, const volatile EngineState_t *eng);

#endif /* ECU_DIAGNOSTICS_H */
