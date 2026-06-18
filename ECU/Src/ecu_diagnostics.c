/**
 * ecu_diagnostics.c — Diagnóstico de falhas, log e modo limp.
 */

#include "ecu_diagnostics.h"
#include "ecu_fault.h"
#include "drv_timer.h"
#include "ecu_config.h"

static FaultLogEntry_t s_log[FAULT_LOG_MAX_ENTRIES];
static uint8_t s_log_head = 0;
static uint8_t s_log_count = 0;

void ECU_Diag_Init(void)
{
    s_log_head  = 0;
    s_log_count = 0;
    for (uint8_t i = 0; i < FAULT_LOG_MAX_ENTRIES; i++) {
        s_log[i].fault_code = 0;
    }
}

void ECU_Diag_Update(volatile EngineState_t *eng, const EcuProfile_t *prof)
{
    ECU_Diag_CheckOvertemp(eng, prof);
    ECU_Diag_CheckBattery(eng);

    /* Pressão de óleo baixa em marcha */
    if ((prof->features & FEAT_OIL_PRESSURE) &&
        eng->rpm > 500 &&
        eng->oil_pressure_kpa < 100) {  /* < 1 bar */
        FAULT_SET(eng->active_faults, FAULT_OIL_PRESSURE);
        ECU_Diag_LogFault(FAULT_OIL_PRESSURE, eng);
    } else {
        FAULT_CLEAR(eng->active_faults, FAULT_OIL_PRESSURE);
    }

    /* CKP ausente acima de um período mínimo de tempo */
    if (eng->rpm == 0 && eng->engine_runtime_s > 5) {
        FAULT_SET(eng->active_faults, FAULT_CKP_MISSING);
    }
}

void ECU_Diag_SetFault(volatile EngineState_t *eng, EcuFaultCode_t code)
{
    FAULT_SET(eng->active_faults, code);
    ECU_Diag_LogFault(code, eng);
}

void ECU_Diag_ClearFault(volatile EngineState_t *eng, EcuFaultCode_t code)
{
    FAULT_CLEAR(eng->active_faults, code);
}

void ECU_Diag_ClearAll(volatile EngineState_t *eng)
{
    eng->active_faults = 0;
    eng->knock_events  = 0;
    s_log_head  = 0;
    s_log_count = 0;
}

void ECU_Diag_LogFault(EcuFaultCode_t code, const volatile EngineState_t *eng)
{
    /* Verifica se já existe entrada para este código */
    for (uint8_t i = 0; i < s_log_count; i++) {
        uint8_t idx = (uint8_t)((s_log_head - s_log_count + i) % FAULT_LOG_MAX_ENTRIES);
        if (s_log[idx].fault_code == code) {
            s_log[idx].count++;
            return;
        }
    }

    /* Nova entrada */
    s_log[s_log_head].fault_code    = code;
    s_log[s_log_head].timestamp_s   = DRV_Timer_GetMs() / 1000UL;
    s_log[s_log_head].rpm_at_fault  = eng->rpm;
    s_log[s_log_head].engine_state  = (uint8_t)eng->engine_runtime_s;
    s_log[s_log_head].count         = 1;

    s_log_head = (uint8_t)((s_log_head + 1) % FAULT_LOG_MAX_ENTRIES);
    if (s_log_count < FAULT_LOG_MAX_ENTRIES) s_log_count++;
}

uint8_t ECU_Diag_IsLimpMode(const volatile EngineState_t *eng)
{
    /* Modo limp quando há falhas críticas de sensor */
    uint32_t limp_faults = FAULT_CKP_MISSING | FAULT_MAP_OPEN | FAULT_TPS_OPEN;
    return (uint8_t)((eng->active_faults & limp_faults) != 0);
}

void ECU_Diag_CheckOvertemp(volatile EngineState_t *eng, const EcuProfile_t *prof)
{
    if (eng->coolant_c > (int16_t)(prof->max_coolant_temp_c * 10)) {
        FAULT_SET(eng->active_faults, FAULT_OVERTEMP);
        ECU_Diag_LogFault(FAULT_OVERTEMP, eng);
    } else {
        FAULT_CLEAR(eng->active_faults, FAULT_OVERTEMP);
    }
}

void ECU_Diag_CheckBattery(volatile EngineState_t *eng)
{
    if (eng->batt_mv < ECU_MIN_BATT_MV) {
        FAULT_SET(eng->active_faults, FAULT_BATT_LOW);
    } else if (eng->batt_mv > ECU_MAX_BATT_MV) {
        FAULT_SET(eng->active_faults, FAULT_BATT_HIGH);
    } else {
        FAULT_CLEAR(eng->active_faults, FAULT_BATT_LOW);
        FAULT_CLEAR(eng->active_faults, FAULT_BATT_HIGH);
    }
}

void ECU_Diag_GetFirmwareInfo(FirmwareInfo_t *info, const EcuProfile_t *prof)
{
    info->major          = ECU_FW_MAJOR;
    info->minor          = ECU_FW_MINOR;
    info->patch          = ECU_FW_PATCH;
    info->vehicle_type   = (uint8_t)(prof ? prof->cylinders : 4);
    info->cylinders      = (uint8_t)(prof ? prof->cylinders : 4);
    info->displacement_cc = 0;
    info->features_hi    = (uint8_t)(prof ? (uint8_t)((prof->features >> 16) & 0xFF) : 0);
    info->features_lo    = (uint8_t)(prof ? (uint8_t)(prof->features & 0xFF) : 0);
    const char *bd = ECU_FW_BUILD_DATE;
    for (uint8_t i = 0; i < 6 && bd[i]; i++) info->build_date[i] = bd[i];
}

const FaultLogEntry_t *ECU_Diag_GetLog(uint8_t *count)
{
    *count = s_log_count;
    return s_log;
}
