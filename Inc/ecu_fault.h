/**
 * ecu_fault.h — Códigos de falha (estilo OBD-II P-codes).
 * 32 falhas ativas em bitmask uint32_t.
 */
#ifndef ECU_FAULT_H
#define ECU_FAULT_H

#include <stdint.h>

typedef enum {
    /* Sensores */
    FAULT_TPS_OPEN       = (1U <<  0),  /* P0120 */
    FAULT_TPS_RANGE      = (1U <<  1),  /* P0121 */
    FAULT_MAP_OPEN       = (1U <<  2),  /* P0105 */
    FAULT_MAP_RANGE      = (1U <<  3),  /* P0106 */
    FAULT_CLT_OPEN       = (1U <<  4),  /* P0115 */
    FAULT_CLT_RANGE      = (1U <<  5),  /* P0116 */
    FAULT_IAT_OPEN       = (1U <<  6),  /* P0110 */
    FAULT_IAT_RANGE      = (1U <<  7),  /* P0111 */
    FAULT_O2_OPEN        = (1U <<  8),  /* P0130 */
    FAULT_O2_SLOW        = (1U <<  9),  /* P0133 */
    FAULT_KNOCK_OPEN     = (1U << 10),  /* P0325 */
    FAULT_BATT_LOW       = (1U << 11),  /* P0562 */
    FAULT_BATT_HIGH      = (1U << 12),  /* P0563 */
    /* Injetores */
    FAULT_INJ1_OPEN      = (1U << 13),  /* P0201 */
    FAULT_INJ2_OPEN      = (1U << 14),  /* P0202 */
    FAULT_INJ3_OPEN      = (1U << 15),  /* P0203 */
    FAULT_INJ4_OPEN      = (1U << 16),  /* P0204 */
    /* Ignição */
    FAULT_IGN1_FAIL      = (1U << 17),  /* P0351 */
    FAULT_IGN2_FAIL      = (1U << 18),  /* P0352 */
    FAULT_IGN3_FAIL      = (1U << 19),  /* P0353 */
    FAULT_IGN4_FAIL      = (1U << 20),  /* P0354 */
    /* Sistema */
    FAULT_CKP_MISSING    = (1U << 21),  /* P0335 — para motor */
    FAULT_CMP_MISSING    = (1U << 22),  /* P0340 — modo batch */
    FAULT_SYNC_LOST      = (1U << 23),  /* P0336 */
    FAULT_OVERTEMP       = (1U << 24),  /* P0217 */
    FAULT_KNOCK_SEVERE   = (1U << 25),  /* P0324 */
    FAULT_FUEL_PRESS     = (1U << 26),  /* P0087 */
    FAULT_REV_LIMIT      = (1U << 27),  /* Informativo */
    FAULT_CAN_BUS_OFF    = (1U << 28),  /* P0600 */
    FAULT_FLASH_ERR      = (1U << 29),  /* Config corrompida */
    FAULT_WATCHDOG       = (1U << 30),  /* Reset por WDG */
    FAULT_ECU_INTERNAL   = (int32_t)(1UL << 31),  /* Falha interna */
} EcuFaultCode_t;

/* Alias para pressão de óleo (compartilha bit 26 com pressão de combustível) */
#define FAULT_OIL_PRESSURE      FAULT_FUEL_PRESS

/* Operações de bitmask */
#define FAULT_IS_ACTIVE(faults, code)   (((faults) & (uint32_t)(code)) != 0U)
#define FAULT_SET(faults, code)         ((faults) |= (uint32_t)(code))
#define FAULT_CLEAR(faults, code)       ((faults) &= ~(uint32_t)(code))

/* Limites de validação de sensores (em mV) */
#define SENSOR_TPS_MIN_MV       100U
#define SENSOR_TPS_MAX_MV       4900U
#define SENSOR_MAP_MIN_MV       150U
#define SENSOR_MAP_MAX_MV       4850U
#define SENSOR_CLT_MIN_MV       100U
#define SENSOR_CLT_MAX_MV       4900U
#define SENSOR_IAT_MIN_MV       100U
#define SENSOR_IAT_MAX_MV       4900U
#define SENSOR_O2_MIN_MV        50U
#define SENSOR_O2_MAX_MV        4950U
#define SENSOR_KNOCK_MIN_MV     0U
#define SENSOR_KNOCK_MAX_MV     3300U

/* Texto das falhas (para envio via BLE / painel) */
static inline const char *ECU_Fault_GetText(EcuFaultCode_t code)
{
    switch (code) {
        case FAULT_TPS_OPEN:    return "P0120 TPS circuito aberto";
        case FAULT_TPS_RANGE:   return "P0121 TPS fora de faixa";
        case FAULT_MAP_OPEN:    return "P0105 MAP circuito aberto";
        case FAULT_MAP_RANGE:   return "P0106 MAP fora de faixa";
        case FAULT_CLT_OPEN:    return "P0115 CLT circuito aberto";
        case FAULT_CLT_RANGE:   return "P0116 CLT fora de faixa";
        case FAULT_IAT_OPEN:    return "P0110 IAT circuito aberto";
        case FAULT_IAT_RANGE:   return "P0111 IAT fora de faixa";
        case FAULT_O2_OPEN:     return "P0130 Lambda circuito aberto";
        case FAULT_O2_SLOW:     return "P0133 Lambda resposta lenta";
        case FAULT_KNOCK_OPEN:  return "P0325 Knock circuito aberto";
        case FAULT_BATT_LOW:    return "P0562 Tensao bateria baixa";
        case FAULT_BATT_HIGH:   return "P0563 Tensao bateria alta";
        case FAULT_INJ1_OPEN:   return "P0201 Injetor 1 aberto";
        case FAULT_INJ2_OPEN:   return "P0202 Injetor 2 aberto";
        case FAULT_INJ3_OPEN:   return "P0203 Injetor 3 aberto";
        case FAULT_INJ4_OPEN:   return "P0204 Injetor 4 aberto";
        case FAULT_IGN1_FAIL:   return "P0351 Bobina 1 falha";
        case FAULT_IGN2_FAIL:   return "P0352 Bobina 2 falha";
        case FAULT_IGN3_FAIL:   return "P0353 Bobina 3 falha";
        case FAULT_IGN4_FAIL:   return "P0354 Bobina 4 falha";
        case FAULT_CKP_MISSING: return "P0335 CKP ausente";
        case FAULT_CMP_MISSING: return "P0340 CMP ausente";
        case FAULT_SYNC_LOST:   return "P0336 Sincronismo perdido";
        case FAULT_OVERTEMP:    return "P0217 Motor superaquecido";
        case FAULT_KNOCK_SEVERE:return "P0324 Detonacao severa";
        case FAULT_FUEL_PRESS:  return "P0087 Pressao combustivel baixa";
        case FAULT_REV_LIMIT:   return "Limitador RPM ativo";
        case FAULT_CAN_BUS_OFF: return "P0600 CAN bus off";
        case FAULT_FLASH_ERR:   return "Config flash corrompida";
        case FAULT_WATCHDOG:    return "Reset watchdog";
        case FAULT_ECU_INTERNAL:return "Falha interna ECU";
        default:                return "Falha desconhecida";
    }
}

#endif /* ECU_FAULT_H */
