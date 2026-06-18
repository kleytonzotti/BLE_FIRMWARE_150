/**
 * ecu_fuel.c — Cálculo de pulso de injeção e controle dos injetores.
 *
 * Fórmula base (Speed-Density):
 *   PW = (VE/100 × MAP_kPa × 3.483 / IAT_K / AFR_stoich) × (1/inj_flow) × 60000000
 *        + dead_time_corrected
 *
 * Simplificado para inteiros:
 *   PW_us = base × VE% × corrections / 1000
 */

#include "ecu_fuel.h"
#include "ecu_tables.h"
#include "pinout.h"
#include "ecu_config.h"
#include <string.h>

static uint8_t s_fuel_cut = 0;

/* Closed-loop: integrador de correção O2 (× 100 para precisão inteira) */
static int32_t s_o2_integrator = 0;
static int32_t s_o2_trim       = 0;   /* Trim atual: -50% a +50% × 100 */

void ECU_Fuel_Init(void)
{
    s_fuel_cut      = 0;
    s_o2_integrator = 0;
    s_o2_trim       = 0;
}

void ECU_Fuel_Calculate(volatile EngineState_t *eng, const EcuProfile_t *prof)
{
    if (eng->rpm < 50) {
        /* Motor parado — zera pulse width */
        for (uint8_t c = 0; c < 4; c++) eng->pulse_width_us[c] = 0;
        return;
    }

    /* ---- 1. Eficiência volumétrica da tabela ---- */
    uint8_t ve = ECU_Table_LookupVE(prof, eng->rpm, eng->tps_pct / 10U);
    /* ve: 0-200 (100 = 100%) */

    /* ---- 2. Massa de ar estimada (MAP + IAT, Speed-Density) ---- */
    /* MAP em kPa × 10, IAT em °C × 10 → converte para Kelvin × 10 */
    int32_t iat_k_x10 = eng->iat_c + 2731;  /* °C×10 + 2731 = K×10 */
    if (iat_k_x10 < 2231) iat_k_x10 = 2231; /* mínimo -50°C */

    /* base_pw = MAP × ve / AFR_stoich / IAT_K × constante
     * Evita float: usa inteiros com escala × 1000 */
    uint32_t map_kpa = eng->map_kpa;  /* já é kPa × 10 */

    /* AFR estequiométrico: gasolina=147, etanol=90, flex interpolado */
    uint16_t afr_stoich = 147;
    if (prof->fuel_type == FUEL_ETHANOL) {
        afr_stoich = 90;
    } else if (prof->fuel_type == FUEL_FLEX) {
        /* Interpolação linear 0%→147, 100%→90 */
        afr_stoich = (uint16_t)(147 - (uint32_t)(147 - 90) * eng->flex_pct / 1000);
    }

    /* AFR alvo da tabela */
    uint8_t afr_target = ECU_Table_LookupAFR(prof, eng->rpm, eng->tps_pct / 10U);
    if (afr_target == 0) afr_target = afr_stoich;

    /* Pulso base em µs (fórmula simplificada sem float):
     * PW = (inj_const × MAP × VE) / (IAT_K × AFR_target × inj_flow)
     * inj_const = 3483000  (constante derivada de R=287, Vd, fator de escala) */
    uint32_t inj_flow = prof->injector_flow_cc_min;
    if (inj_flow == 0) inj_flow = 200;

    uint32_t numerator   = (uint32_t)map_kpa * ve * 3483UL;
    uint32_t denominator = (uint32_t)iat_k_x10 * afr_target * inj_flow / 1000UL;
    if (denominator == 0) denominator = 1;

    uint32_t base_pw_us = numerator / denominator;

    /* ---- 3. Tempo morto do injetor (dead time) corrigido pela tensão ---- */
    uint32_t dead_time = prof->injector_dead_time_us;
    /* Bateria baixa → mais tempo morto (injetor demora mais para abrir) */
    if (eng->batt_mv < 14000) {
        dead_time += (uint32_t)(14000 - eng->batt_mv) / 200;
    }

    /* ---- 4. Correções aditivas ---- */
    int32_t pw = (int32_t)base_pw_us;

    /* Correção por temperatura do refrigerante (CLT) */
    {
        static const uint16_t clt_axis[10] = {
            (uint16_t)((-30 + 2731)), (uint16_t)((-20 + 2731)),
            (uint16_t)(0 + 2731),    (uint16_t)(20 + 2731),
            (uint16_t)(40 + 2731),   (uint16_t)(60 + 2731),
            (uint16_t)(80 + 2731),   (uint16_t)(90 + 2731),
            (uint16_t)(100 + 2731),  (uint16_t)(110 + 2731)
        };
        int16_t clt_corr = ECU_Table_Interp1D(prof->clt_fuel_corr, clt_axis, 10,
                                               (uint16_t)(eng->coolant_c + 2731));
        pw = pw + (pw * clt_corr / 100);
    }

    /* Correção por temperatura do ar (IAT) */
    {
        static const uint16_t iat_axis[10] = {
            (uint16_t)((-30+2731)), (uint16_t)((-20+2731)), (uint16_t)(0+2731),
            (uint16_t)(10+2731), (uint16_t)(20+2731), (uint16_t)(30+2731),
            (uint16_t)(40+2731), (uint16_t)(50+2731), (uint16_t)(60+2731),
            (uint16_t)(70+2731)
        };
        int16_t iat_corr = ECU_Table_Interp1D(prof->iat_fuel_corr, iat_axis, 10,
                                               (uint16_t)iat_k_x10);
        pw = pw + (pw * iat_corr / 100);
    }

    /* Trim global de combustível */
    pw = pw + (pw * prof->global_fuel_trim / 100);

    /* Closed-loop trim O2 */
    pw = pw + (pw * s_o2_trim / 10000);

    /* Adiciona tempo morto */
    pw += (int32_t)dead_time;

    /* Clamp de segurança */
    if (pw < (int32_t)ECU_MIN_INJ_PW_US) pw = ECU_MIN_INJ_PW_US;
    if (pw > (int32_t)ECU_MAX_INJ_PW_US) pw = ECU_MAX_INJ_PW_US;

    /* Aplica para todos os cilindros */
    for (uint8_t c = 0; c < prof->cylinders && c < 4; c++) {
        eng->pulse_width_us[c] = (uint32_t)pw;
        eng->injector_duty[c]  = (uint8_t)ECU_MIN(pw * eng->rpm / 1200000UL, 99);
    }
}

void ECU_Fuel_OpenInjector(uint8_t cyl)
{
    switch (cyl) {
        case 0: INJ1_OPEN(); break;
        case 1: INJ2_OPEN(); break;
        case 2: INJ3_OPEN(); break;
        case 3: INJ4_OPEN(); break;
        default: break;
    }
}

void ECU_Fuel_CloseInjector(uint8_t cyl)
{
    switch (cyl) {
        case 0: INJ1_CLOSE(); break;
        case 1: INJ2_CLOSE(); break;
        case 2: INJ3_CLOSE(); break;
        case 3: INJ4_CLOSE(); break;
        default: break;
    }
}

void ECU_Fuel_Cut(void)
{
    s_fuel_cut = 1;
    INJ1_CLOSE(); INJ2_CLOSE(); INJ3_CLOSE(); INJ4_CLOSE();
}

void ECU_Fuel_Resume(void)
{
    s_fuel_cut = 0;
}

uint8_t ECU_Fuel_IsCut(void)
{
    return s_fuel_cut;
}

void ECU_Fuel_UpdateClosedLoop(volatile EngineState_t *eng, const EcuProfile_t *prof)
{
    /* Closed-loop apenas em condições estáveis */
    if (eng->rpm < 800 || eng->rpm > 5000) return;
    if (eng->tps_pct > 800) return;  /* Evita full-throttle */
    if (eng->o2_afr_x10 == 0) return;

    /* AFR alvo da tabela */
    uint16_t target_afr = ECU_Table_LookupAFR(prof, eng->rpm, eng->tps_pct / 10U);
    if (target_afr == 0) target_afr = 147;

    /* Erro = alvo - medido (× 10) */
    int32_t error = (int32_t)target_afr - (int32_t)eng->o2_afr_x10;

    /* Integrador simples (proporcional ao erro) */
    s_o2_integrator += error;
    s_o2_integrator = ECU_CLAMP(s_o2_integrator, -500000, 500000);

    /* Trim = integrador / ganho → faixa -50% a +50% × 100 */
    s_o2_trim = s_o2_integrator / 5000;
    s_o2_trim = ECU_CLAMP(s_o2_trim, -5000, 5000);
}
