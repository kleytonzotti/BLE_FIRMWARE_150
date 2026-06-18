/**
 * ecu_cdi.c — Ignição CDI (Capacitor Discharge Ignition) para motor carburado.
 *
 * O conversor DC-DC boost (TIM17 PWM 100kHz) carrega o capacitor a 350V.
 * O disparo do SCR (BT151) é via GPIO → optocoupler PC817 → gate do SCR.
 * Pulso de gate: 100µs (o SCR trava e mantém condução até corrente zerar).
 *
 * Sem injeção: modo CDI gerencia apenas ignição.
 * A curva de avanço tem 32 pontos (RPM × graus BTDC × 10).
 */

#include "ecu_cdi.h"
#include "ecu_tables.h"
#include "drv_timer.h"
#include "pinout.h"
#include "ecu_config.h"

static uint8_t  s_cdi_enabled    = 0;
static uint8_t  s_capacitor_ready = 0;

void ECU_CDI_Init(void)
{
    s_cdi_enabled     = 0;
    s_capacitor_ready = 0;
    CDI1_CLEAR();
    CDI2_CLEAR();
}

void ECU_CDI_SetMode(uint8_t enable)
{
    s_cdi_enabled = enable;
    if (enable) {
        ECU_CDI_BoostStart();
    } else {
        ECU_CDI_BoostStop();
        CDI1_CLEAR();
        CDI2_CLEAR();
    }
}

void ECU_CDI_BoostStart(void)
{
    DRV_Timer_CDI_Boost_Init(CDI_BOOST_PWM_FREQ_HZ);
    DRV_Timer_CDI_Boost_SetDuty(60);  /* 60% duty — ajuste fino pelo feedback de tensão */
}

void ECU_CDI_BoostStop(void)
{
    DRV_Timer_CDI_Boost_Stop();
}

uint8_t ECU_CDI_IsCapacitorReady(void)
{
    /* Em hardware real: comparar tensão no capacitor via ADC.
     * Por ora, retorna pronto após inicializado. */
    return s_capacitor_ready;
}

void ECU_CDI_Calculate(volatile EngineState_t *eng, const EcuProfile_t *prof)
{
    if (eng->rpm < 50) {
        eng->cdi_advance_deg = 50;  /* 5° BTDC mínimo na partida */
        return;
    }

    /* Interpola na curva de 32 pontos */
    uint16_t adv = ECU_Table_LookupCDI(prof, eng->rpm);

    /* Corrige pelo trim global de ignição */
    int32_t adv_final = (int32_t)adv + (int32_t)prof->global_ign_trim * 10;
    adv_final = ECU_CLAMP(adv_final, 0, 450);  /* 0° a 45° */

    eng->cdi_advance_deg = (uint16_t)adv_final;
    s_capacitor_ready = 1;  /* Sinaliza para OnCrankTooth */
}

void ECU_CDI_ScheduleFire(uint8_t cdi_ch, uint32_t delay_us)
{
    /* Na prática: usar TIM Output Compare para disparar no momento exato.
     * Aqui, disparo imediato simplificado para protótipo. */
    (void)delay_us;
    ECU_CDI_Fire(cdi_ch);
}

void ECU_CDI_Fire(uint8_t cdi_ch)
{
    if (!s_cdi_enabled || !s_capacitor_ready) return;

    /* Pulso de 100µs no gate do SCR via optocoupler */
    if (cdi_ch == 0) {
        CDI1_TRIGGER();
        DRV_Timer_DelayUs(100);
        CDI1_CLEAR();
    } else {
        CDI2_TRIGGER();
        DRV_Timer_DelayUs(100);
        CDI2_CLEAR();
    }
    s_capacitor_ready = 0;  /* Aguarda recarregar */
}
