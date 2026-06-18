/**
 * ecu_tables.h — Interpolação bilinear nas tabelas 16×16.
 */
#ifndef ECU_TABLES_H
#define ECU_TABLES_H

#include "ecu_types.h"

/* Interpolação bilinear — retorna valor interpolado nas tabelas 16×16 */
uint8_t  ECU_Table_LookupVE(const EcuProfile_t *prof, uint16_t rpm, uint16_t load_pct);
int16_t  ECU_Table_LookupIgn(const EcuProfile_t *prof, uint16_t rpm, uint16_t load_pct);
uint8_t  ECU_Table_LookupAFR(const EcuProfile_t *prof, uint16_t rpm, uint16_t load_pct);

/* Interpolação linear 1D para correções por temperatura */
int16_t  ECU_Table_Interp1D(const int8_t *table, const uint16_t *x_axis, uint8_t n, uint16_t x);

/* Encontra os dois índices ao redor de 'value' no eixo.
 * Retorna 1 se encontrou, preenche idx_lo e idx_hi. */
uint8_t  ECU_Table_FindAxis(const uint16_t *axis, uint8_t n, uint16_t value,
                             uint8_t *idx_lo, uint8_t *idx_hi);

/* Curva CDI — interpolação 1D em tabela de 32 pontos */
uint16_t ECU_Table_LookupCDI(const EcuProfile_t *prof, uint16_t rpm);

#endif /* ECU_TABLES_H */
