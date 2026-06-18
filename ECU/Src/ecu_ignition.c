/**
 * ecu_ignition.c — Cálculo de avanço e controle de bobinas indutivas.
 */

#include "ecu_ignition.h"
#include "ecu_tables.h"
#include "drv_timer.h"
#include "pinout.h"
#include "ecu_config.h"

static int16_t s_knock_retard = 0;   /* Recuo de avanço por knock (× 10°) */
static uint8_t s_cut = 0;

void ECU_Ignition_Init(void)
{
    s_knock_retard = 0;
    s_cut = 0;
    /* Garante bobinas desligadas */
    IGN1_FIRE(); IGN2_FIRE(); IGN3_FIRE(); IGN4_FIRE();
}

void ECU_Ignition_Calculate(volatile EngineState_t *eng, const EcuProfile_t *prof)
{
    if (eng->rpm < 50) return;

    /* Avanço base da tabela (× 10°) */
    int16_t adv = ECU_Table_LookupIgn(prof, eng->rpm, eng->tps_pct / 10U);
    adv *= 10;  /* converte para × 10° */

    /* Trim global */
    adv += (int16_t)(prof->global_ign_trim * 10);

    /* Correção por CLT (motor frio → menos avanço) */
    {
        static const uint16_t clt_axis[10] = {
            300, 500, 730, 930, 1130, 1330, 1530, 1630, 1730, 1830
        };
        int16_t corr = ECU_Table_Interp1D(prof->clt_ign_corr, clt_axis, 10,
                                           (uint16_t)(eng->coolant_c + 2731));
        adv += corr * 10;
    }

    /* Recuo por knock */
    adv -= s_knock_retard;

    /* Recuperação gradual: +0.5°/ciclo até zero */
    if (s_knock_retard > 0) {
        s_knock_retard -= 5;
        if (s_knock_retard < 0) s_knock_retard = 0;
    }

    /* Limite de segurança */
    if (adv < -200) adv = -200;   /* -20° */
    if (adv > 600)  adv = 600;    /* +60° */

    eng->ign_advance_deg = adv;

    /* Dwell: compensa tensão da bateria
     * Tensão nominal 14V = dwell padrão. -1V = +0.5ms */
    int32_t dwell = (int32_t)prof->coil_dwell_ms * 1000;  /* µs */
    dwell += (int32_t)(14000 - (int32_t)eng->batt_mv) * 35 / 100;
    dwell = ECU_CLAMP(dwell, (int32_t)ECU_MIN_DWELL_US, (int32_t)ECU_MAX_DWELL_US);
    eng->dwell_us = (uint16_t)dwell;
}

void ECU_Ignition_ScheduleDwell(uint8_t coil, uint32_t tick_start)
{
    if (s_cut) return;
    (void)tick_start;
    /* Hardware usa Output Compare do TIM8 — stub para integração */
    switch (coil) {
        case 0: IGN1_DWELL(); break;
        case 1: IGN2_DWELL(); break;
        case 2: IGN3_DWELL(); break;
        case 3: IGN4_DWELL(); break;
        default: break;
    }
}

void ECU_Ignition_ScheduleFire(uint8_t coil, uint32_t tick_fire)
{
    if (s_cut) return;
    (void)tick_fire;
    switch (coil) {
        case 0: IGN1_FIRE(); break;
        case 1: IGN2_FIRE(); break;
        case 2: IGN3_FIRE(); break;
        case 3: IGN4_FIRE(); break;
        default: break;
    }
}

void ECU_Ignition_CutAll(void)
{
    s_cut = 1;
    IGN1_FIRE(); IGN2_FIRE(); IGN3_FIRE(); IGN4_FIRE();
}

void ECU_Ignition_KnockRetard(volatile EngineState_t *eng, uint8_t severity)
{
    (void)eng;
    /* severity: 1=leve(2°), 2=moderado(5°), 3=severo(10°) */
    static const int16_t retard_table[4] = { 0, 20, 50, 100 }; /* × 10° */
    if (severity > 3) severity = 3;
    s_knock_retard += retard_table[severity];
    if (s_knock_retard > 150) s_knock_retard = 150; /* máx -15° */
    eng->knock_events++;
}
