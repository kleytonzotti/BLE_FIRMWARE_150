/**
 * ecu_tables.c — Interpolação bilinear nas tabelas 16×16.
 *
 * Todos os cálculos em inteiros (sem float).
 * Interpolação bilinear: interpola nos dois eixos sequencialmente.
 */

#include "ecu_tables.h"
#include "ecu_config.h"

/* Encontra índices vizinhos no eixo */
uint8_t ECU_Table_FindAxis(const uint16_t *axis, uint8_t n, uint16_t value,
                            uint8_t *idx_lo, uint8_t *idx_hi)
{
    if (value <= axis[0]) {
        *idx_lo = 0; *idx_hi = 0; return 1;
    }
    if (value >= axis[n - 1]) {
        *idx_lo = n - 1; *idx_hi = n - 1; return 1;
    }
    for (uint8_t i = 0; i < (uint8_t)(n - 1); i++) {
        if (value >= axis[i] && value <= axis[i + 1]) {
            *idx_lo = i; *idx_hi = i + 1; return 1;
        }
    }
    *idx_lo = 0; *idx_hi = 0; return 0;
}

/* Interpola entre dois valores inteiros pelo fator t (0-256 = 0-100%) */
static int32_t _lerp(int32_t a, int32_t b, uint16_t lo, uint16_t hi, uint16_t x)
{
    if (hi == lo) return a;
    return a + (int32_t)(b - a) * (int32_t)(x - lo) / (int32_t)(hi - lo);
}

uint8_t ECU_Table_LookupVE(const EcuProfile_t *prof, uint16_t rpm, uint16_t load_pct)
{
    uint8_t r0, r1, c0, c1;
    ECU_Table_FindAxis(prof->rpm_axis, FUEL_TABLE_ROWS, rpm, &r0, &r1);
    /* load_axis em % (0-100), load_pct em 0-100 */
    static const uint16_t load_axis16[16] = {
        5,10,15,20,25,30,40,50,60,70,75,80,85,90,95,100
    };
    ECU_Table_FindAxis(load_axis16, FUEL_TABLE_COLS, load_pct, &c0, &c1);

    /* Interpola nos dois eixos */
    int32_t v00 = prof->fuel_ve_table[r0][c0];
    int32_t v01 = prof->fuel_ve_table[r0][c1];
    int32_t v10 = prof->fuel_ve_table[r1][c0];
    int32_t v11 = prof->fuel_ve_table[r1][c1];

    int32_t v0 = _lerp(v00, v01, load_axis16[c0], load_axis16[c1], load_pct);
    int32_t v1 = _lerp(v10, v11, load_axis16[c0], load_axis16[c1], load_pct);
    int32_t ve = _lerp(v0,  v1,  prof->rpm_axis[r0], prof->rpm_axis[r1], rpm);

    return (uint8_t)ECU_CLAMP(ve, 0, 200);
}

int16_t ECU_Table_LookupIgn(const EcuProfile_t *prof, uint16_t rpm, uint16_t load_pct)
{
    uint8_t r0, r1, c0, c1;
    static const uint16_t load_axis16[16] = {
        5,10,15,20,25,30,40,50,60,70,75,80,85,90,95,100
    };
    ECU_Table_FindAxis(prof->rpm_axis, IGN_TABLE_ROWS, rpm, &r0, &r1);
    ECU_Table_FindAxis(load_axis16, IGN_TABLE_COLS, load_pct, &c0, &c1);

    int32_t v00 = prof->ign_advance_table[r0][c0];
    int32_t v01 = prof->ign_advance_table[r0][c1];
    int32_t v10 = prof->ign_advance_table[r1][c0];
    int32_t v11 = prof->ign_advance_table[r1][c1];

    int32_t v0 = _lerp(v00, v01, load_axis16[c0], load_axis16[c1], load_pct);
    int32_t v1 = _lerp(v10, v11, load_axis16[c0], load_axis16[c1], load_pct);
    int32_t adv = _lerp(v0, v1, prof->rpm_axis[r0], prof->rpm_axis[r1], rpm);

    return (int16_t)ECU_CLAMP(adv, -20, 60);
}

uint8_t ECU_Table_LookupAFR(const EcuProfile_t *prof, uint16_t rpm, uint16_t load_pct)
{
    uint8_t r0, r1, c0, c1;
    static const uint16_t load_axis16[16] = {
        5,10,15,20,25,30,40,50,60,70,75,80,85,90,95,100
    };
    ECU_Table_FindAxis(prof->rpm_axis, AFR_TABLE_ROWS, rpm, &r0, &r1);
    ECU_Table_FindAxis(load_axis16, AFR_TABLE_COLS, load_pct, &c0, &c1);

    int32_t v00 = prof->afr_target_table[r0][c0];
    int32_t v01 = prof->afr_target_table[r0][c1];
    int32_t v10 = prof->afr_target_table[r1][c0];
    int32_t v11 = prof->afr_target_table[r1][c1];

    int32_t v0  = _lerp(v00, v01, load_axis16[c0], load_axis16[c1], load_pct);
    int32_t v1  = _lerp(v10, v11, load_axis16[c0], load_axis16[c1], load_pct);
    int32_t afr = _lerp(v0, v1, prof->rpm_axis[r0], prof->rpm_axis[r1], rpm);

    return (uint8_t)ECU_CLAMP(afr, 90, 200);
}

int16_t ECU_Table_Interp1D(const int8_t *table, const uint16_t *x_axis,
                             uint8_t n, uint16_t x)
{
    if (x <= x_axis[0]) return (int16_t)table[0];
    if (x >= x_axis[n - 1]) return (int16_t)table[n - 1];

    for (uint8_t i = 0; i < (uint8_t)(n - 1); i++) {
        if (x >= x_axis[i] && x <= x_axis[i + 1]) {
            int32_t y = _lerp((int32_t)table[i], (int32_t)table[i + 1],
                               x_axis[i], x_axis[i + 1], x);
            return (int16_t)y;
        }
    }
    return (int16_t)table[0];
}

uint16_t ECU_Table_LookupCDI(const EcuProfile_t *prof, uint16_t rpm)
{
    if (rpm <= prof->cdi_rpm[0]) return prof->cdi_advance_x10[0];
    if (rpm >= prof->cdi_rpm[CDI_TABLE_POINTS - 1])
        return prof->cdi_advance_x10[CDI_TABLE_POINTS - 1];

    for (uint8_t i = 0; i < CDI_TABLE_POINTS - 1; i++) {
        if (rpm >= prof->cdi_rpm[i] && rpm <= prof->cdi_rpm[i + 1]) {
            int32_t adv = _lerp((int32_t)prof->cdi_advance_x10[i],
                                 (int32_t)prof->cdi_advance_x10[i + 1],
                                 prof->cdi_rpm[i], prof->cdi_rpm[i + 1], rpm);
            return (uint16_t)ECU_CLAMP(adv, 0, 500);
        }
    }
    return prof->cdi_advance_x10[0];
}
