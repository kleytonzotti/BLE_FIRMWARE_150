/**
 * ecu_engine.c — Máquina de estados do motor e coordenação dos subsistemas.
 *
 * FLUXO TEMPORAL:
 *  IRQ TIM2 (prioridade 0): OnCrankTooth() — calcula RPM, agenda injeção e ignição
 *  IRQ TIM3 (prioridade 1): Scheduler 100µs — fecha injetores, dispara CDI
 *  IRQ DMA  (prioridade 2): Atualiza buffer ADC
 *  Main loop: leitura de sensores (10ms), idle PID (50ms), BLE/CAN (100ms)
 *
 * REGRA: nenhuma função de ISR pode bloquear, alocar memória ou chamar HAL.
 */

#include "ecu_engine.h"
#include "ecu_fuel.h"
#include "ecu_ignition.h"
#include "ecu_cdi.h"
#include "ecu_sensors.h"
#include "ecu_idle.h"
#include "ecu_diagnostics.h"
#include "ecu_tables.h"
#include "ecu_vehicles.h"
#include "drv_timer.h"
#include "drv_adc.h"
#include "drv_flash.h"
#include "drv_gpio.h"
#include "drv_can_mcp2515.h"
#include "ble_ecu_service.h"
#include "pinout.h"

/* ================================================================
 * Variáveis globais (extern nos headers)
 * ================================================================ */
volatile EngineState_t        g_engine       = {0};
volatile EngineStateMachine_t g_engine_state = ENGINE_OFF;
EcuProfile_t                  g_profile      = {0};

/* ================================================================
 * Estado interno do sincronismo do virabrequim
 * ================================================================ */
static uint8_t  s_sync         = 0;     /* 1 = sincronizado com o gap */
static uint8_t  s_tooth        = 0;     /* Contador de dentes */
static uint32_t s_avg_period   = 0;     /* Período médio (filtro exp.) */
static uint32_t s_last_tick    = 0;     /* Tick do último dente */

/* Agenda dos injetores (acesso pelo scheduler TIM3) */
typedef struct {
    uint8_t  active;
    uint32_t open_tick;
    uint32_t close_tick;
} InjSched_t;
static volatile InjSched_t s_inj[4];

/* Temporizadores de tarefas periódicas */
static uint32_t s_t_10ms  = 0;
static uint32_t s_t_50ms  = 0;
static uint32_t s_t_100ms = 0;

/* Taxa de telemetria BLE (configurável via BLE_CMD_SET_TELEM_RATE) */
uint32_t g_ble_telem_rate_ms = ECU_BLE_TELEM_PERIOD_MS;

/* ================================================================
 * Inicialização
 * ================================================================ */
void ECU_Engine_Init(void)
{
    /* Estado seguro antes de qualquer outra coisa */
    g_engine_state = ENGINE_OFF;
    g_engine.rpm   = 0;

    /* Fecha todos os injetores e bobinas via GPIO direto */
    INJ1_CLOSE(); INJ2_CLOSE(); INJ3_CLOSE(); INJ4_CLOSE();
    IGN1_FIRE();  IGN2_FIRE();  IGN3_FIRE();  IGN4_FIRE();
    CDI1_CLEAR(); CDI2_CLEAR();
    FUEL_PUMP_OFF();
    FAN_OFF();
    CHECK_ENG_OFF();

    /* Carrega perfil da flash ou padrão */
    if (DRV_Flash_LoadProfile(&g_profile) != ECU_OK) {
        /* Flash corrompida ou primeira vez — carrega genérico 4 cil turbo */
        ECU_LoadDefaultProfile(PROFILE_GENERICO_CARRO_4CIL_TURBO);
    }

    /* Inicializa subsistemas */
    ECU_Fuel_Init();
    ECU_Ignition_Init();

    if (g_profile.ignition_type == IGN_CDI) {
        ECU_CDI_Init();
        ECU_CDI_SetMode(1);
    }

    ECU_Idle_Init((IacType_t)g_profile.iac_type);
    ECU_Diag_Init();

    /* Pré-posiciona IAC pela temperatura atual */
    ECU_Idle_PrePosition(g_engine.coolant_c);

    /* Inicia timers */
    DRV_Timer_CKP_Init();
    DRV_Timer_INJ_Init();
    DRV_Timer_IGN_Init();
    DRV_Timer_Scheduler_Init();
    DRV_Timer_WG_Init(1000);

    /* ADC em DMA circular */
    DRV_ADC_Init();

#ifdef FEATURE_CAN_BUS
    if (DRV_CAN_IsPresent()) {
        DRV_CAN_Init(CAN_BAUD_500K);
    }
#endif

    /* Priming da bomba (2 segundos antes de aceitar partida) */
    ECU_Engine_StartPriming();
}

void ECU_Engine_StartPriming(void)
{
    ECU_Engine_SetState(ENGINE_PRIMING);
    FUEL_PUMP_ON();
    /* O main loop desliga se o motor não ligar em 5 segundos */
}

/* ================================================================
 * Máquina de estados
 * ================================================================ */
void ECU_Engine_SetState(EngineStateMachine_t new_state)
{
    if (new_state == g_engine_state) return;
    g_engine_state = new_state;

    switch (new_state) {
        case ENGINE_OFF:
        case ENGINE_FAULT:
            FUEL_PUMP_OFF();
            ECU_Fuel_Cut();
            ECU_Ignition_CutAll();
            break;

        case ENGINE_PRIMING:
            FUEL_PUMP_ON();
            break;

        case ENGINE_CRANKING:
        case ENGINE_CDI_CRANK:
            FUEL_PUMP_ON();
            ECU_Fuel_Resume();
            break;

        case ENGINE_RUNNING:
        case ENGINE_IDLE:
            FUEL_PUMP_ON();
            ECU_Fuel_Resume();
            break;

        case ENGINE_DECEL:
            /* Corte de combustível no decel acima de 1800 RPM */
            if (g_engine.rpm > 1800) {
                ECU_Fuel_Cut();
            }
            break;

        case ENGINE_REV_LIMIT:
            ECU_Fuel_Cut();
            FAULT_SET(g_engine.active_faults, FAULT_REV_LIMIT);
            break;

        case ENGINE_LIMP:
            /* Proteção: enriquece levemente */
            g_profile.global_fuel_trim = 10;
            break;

        default:
            break;
    }
}

void ECU_Engine_EmergencyStop(void)
{
    /* Desligamento de emergência — tudo de uma vez, sem transição de estado */
    ECU_Fuel_Cut();
    ECU_Ignition_CutAll();
    CDI1_CLEAR();
    CDI2_CLEAR();
    ECU_CDI_BoostStop();
    FUEL_PUMP_OFF();
    g_engine_state = ENGINE_FAULT;
}

/* ================================================================
 * _tim2_ckp_handler — ponto de entrada do TIM2_IRQHandler em main.c
 * Lê CCR1 (Input Capture 1MHz), calcula período em µs, chama OnCrankTooth.
 * ================================================================ */
#define _TIM2_SR    (*(volatile uint32_t *)0x40000010UL)
#define _TIM2_CCR1  (*(volatile uint32_t *)0x40000034UL)

static uint32_t s_ckp_last_ccr = 0;

void _tim2_ckp_handler(void)
{
    if (!(_TIM2_SR & (1U << 1))) return;  /* CC1IF não setado */
    _TIM2_SR &= ~(1U << 1);               /* Limpa CC1IF */

    uint32_t ccr    = _TIM2_CCR1;
    uint32_t period = ccr - s_ckp_last_ccr;  /* Wraparound 32-bit safe */
    s_ckp_last_ccr  = ccr;

    OnCrankTooth(period, 0);
}

/* ================================================================
 * Callback do virabrequim — IRQ TIM2, PRIORIDADE 0
 * Máxima prioridade, mínima latência.
 * NUNCA chamar funções bloqueantes aqui.
 * ================================================================ */
void OnCrankTooth(uint32_t period_us, uint8_t tooth_num)
{
    (void)tooth_num;

    s_tooth++;
    s_last_tick = DRV_Timer_GetCrankTick();

    /* Filtra períodos inválidos */
    if (period_us < 80 || period_us > 500000) return;

    /* Atualiza RPM imediatamente */
    {
        uint8_t teeth = (uint8_t)(g_profile.crank_teeth
                                  - g_profile.crank_missing_teeth);
        if (teeth == 0) teeth = 10;
        g_engine.rpm = DRV_Timer_PeriodToRPM(period_us, teeth);
        g_engine.crank_period_us = period_us;
    }

    /* Detecta gap do dente faltante para sincronização
     * Gap = período ~2.5× maior que o período médio */
    if (s_avg_period > 0 && period_us > (s_avg_period * 2U + s_avg_period / 2U)) {
        s_tooth     = 0;
        s_sync      = 1;
    }
    /* Filtro exponencial do período médio: (3×old + new) / 4 */
    s_avg_period = (s_avg_period * 3U + period_us) >> 2U;

    if (!s_sync) return;
    if (g_engine_state < ENGINE_CRANKING) return;

    /* Calcula ângulo disponível por dente */
    uint8_t  teeth_eff      = (uint8_t)(g_profile.crank_teeth - g_profile.crank_missing_teeth);
    uint8_t  cyls           = g_profile.cylinders;
    uint16_t teeth_per_cyl  = (uint16_t)(teeth_eff * 2U / cyls);

    for (uint8_t c = 0; c < cyls && c < 4; c++) {
        uint16_t inj_tooth = (uint16_t)(c * teeth_per_cyl + teeth_per_cyl / 2U);
        uint16_t ign_tooth = (uint16_t)(c * teeth_per_cyl + teeth_per_cyl - 2U);

        /* ---- Agenda injetor ---- */
        if (s_tooth == (inj_tooth % teeth_eff)) {
            if (!ECU_Fuel_IsCut()) {
                s_inj[c].open_tick  = s_last_tick + (period_us >> 1U);
                s_inj[c].close_tick = s_inj[c].open_tick + g_engine.pulse_width_us[c];
                s_inj[c].active     = 1;
            }
        }

        /* ---- Agenda ignição ou CDI ---- */
        if (s_tooth == (ign_tooth % teeth_eff)) {
            if (g_profile.ignition_type == IGN_CDI) {
                /* CDI: calcula delay e agenda disparo SCR */
                uint32_t adv_deg   = (uint32_t)(g_engine.cdi_advance_deg / 10U);
                uint32_t fire_delay = adv_deg * (period_us / 360U);
                ECU_CDI_ScheduleFire(c < 2 ? c : 0,
                                     (uint32_t)(s_last_tick + period_us - fire_delay));
            } else {
                /* Ignição indutiva: dwell e disparo */
                uint32_t fire_delay = (uint32_t)(g_engine.ign_advance_deg / 10)
                                      * (period_us / 360U);
                uint32_t fire_tick  = s_last_tick + period_us - fire_delay;
                uint32_t dwell_tick = fire_tick - g_engine.dwell_us;
                ECU_Ignition_ScheduleDwell(c, dwell_tick);
                ECU_Ignition_ScheduleFire(c, fire_tick);
            }
        }
    }

    g_engine.engine_runtime_s++;
}

void OnCamTooth(void)
{
    /* CMP: fase do motor — necessário para injeção sequencial */
    /* Por ora, apenas registra que o CMP está presente */
    FAULT_CLEAR(g_engine.active_faults, FAULT_CMP_MISSING);
}

/* ================================================================
 * Scheduler 100µs — IRQ TIM3, PRIORIDADE 1
 * Chamada do stm32wbxx_it.c: TIM3_IRQHandler() → ECU_Engine_Scheduler_100us()
 * ================================================================ */
void ECU_Engine_Scheduler_100us(void)
{
    static volatile uint32_t tick = 0;
    tick++;

    uint32_t now = tick; /* cópia local */

    /* Abre e fecha injetores no tempo exato */
    for (uint8_t c = 0; c < 4; c++) {
        if (!s_inj[c].active) continue;

        if (now >= (s_inj[c].open_tick / 100U)) {
            ECU_Fuel_OpenInjector(c);
            s_inj[c].active = 2; /* marcado como aberto */
        }
        if (s_inj[c].active == 2 && now >= (s_inj[c].close_tick / 100U)) {
            ECU_Fuel_CloseInjector(c);
            s_inj[c].active = 0;
            g_engine.total_injections++;
        }
    }
}

/* ================================================================
 * Tarefas periódicas do main loop
 * ================================================================ */
void ECU_Engine_Task_10ms(void)
{
    uint32_t now = DRV_Timer_GetMs();
    if ((now - s_t_10ms) < 10) return;
    s_t_10ms = now;

    /* Lê sensores e atualiza g_engine */
    ECU_Sensors_Update(&g_engine, &g_profile);

    /* Calcula combustível e ignição */
    ECU_Fuel_Calculate(&g_engine, &g_profile);
    ECU_Ignition_Calculate(&g_engine, &g_profile);

    if (g_profile.ignition_type == IGN_CDI) {
        ECU_CDI_Calculate(&g_engine, &g_profile);
    }

    /* Closed-loop O2 */
    if ((g_profile.features & FEAT_WIDEBAND_O2) &&
        g_engine_state == ENGINE_RUNNING) {
        ECU_Fuel_UpdateClosedLoop(&g_engine, &g_profile);
    }

    /* Controle do ventilador */
    if (g_engine.coolant_c > (int16_t)(g_profile.max_coolant_temp_c * 10 - 50)) {
        FAN_ON();
    } else if (g_engine.coolant_c < (int16_t)(g_profile.max_coolant_temp_c * 10 - 100)) {
        FAN_OFF();
    }

    /* Limita RPM */
    if ((g_profile.features & FEAT_REV_LIMITER) &&
        g_engine.rpm > g_profile.rev_limit_rpm &&
        g_engine_state != ENGINE_REV_LIMIT) {
        ECU_Engine_SetState(ENGINE_REV_LIMIT);
    } else if (g_engine_state == ENGINE_REV_LIMIT &&
               g_engine.rpm < (g_profile.rev_limit_rpm - 200)) {
        ECU_Engine_SetState(ENGINE_RUNNING);
    }

    /* Atualiza estado: motor partindo → rodando */
    if (g_engine_state == ENGINE_CRANKING && g_engine.rpm > 400) {
        ECU_Engine_SetState(ENGINE_RUNNING);
    }
    if (g_engine_state == ENGINE_CDI_CRANK && g_engine.rpm > 400) {
        ECU_Engine_SetState(ENGINE_CDI_RUN);
    }

    /* Motor morreu */
    if (g_engine_state >= ENGINE_CRANKING &&
        g_engine.rpm == 0 && s_avg_period == 0) {
        ECU_Engine_SetState(ENGINE_OFF);
    }
}

void ECU_Engine_Task_50ms(void)
{
    uint32_t now = DRV_Timer_GetMs();
    if ((now - s_t_50ms) < 50) return;
    s_t_50ms = now;

    /* PID de marcha lenta */
    if ((g_profile.features & FEAT_IAC) &&
        (g_engine_state == ENGINE_IDLE || g_engine_state == ENGINE_RUNNING)) {
        ECU_Idle_Update(&g_engine, &g_profile);
    }

    /* Controle de boost */
    if ((g_profile.features & FEAT_BOOST_CTRL) && g_engine.boost_kpa > 0) {
        /* Wastegate: se boost acima do alvo, aumenta duty (abre wastegate) */
        uint16_t target = (uint16_t)(g_profile.boost_target_kpa
                                    + (int16_t)g_profile.global_boost_trim);
        if (g_engine.boost_kpa > target + 50) {
            g_engine.wastegate_duty = (uint16_t)ECU_MIN(g_engine.wastegate_duty + 5, 90);
        } else if (g_engine.boost_kpa < target - 50) {
            g_engine.wastegate_duty = (uint16_t)ECU_MAX((int32_t)g_engine.wastegate_duty - 5, 0);
        }
        DRV_Timer_WG_SetDuty((uint8_t)g_engine.wastegate_duty);
    }
}

void ECU_Engine_Task_100ms(void)
{
    uint32_t now = DRV_Timer_GetMs();
    if ((now - s_t_100ms) < 100) return;
    s_t_100ms = now;

    /* Diagnósticos */
    ECU_Diag_Update(&g_engine, &g_profile);

    /* LED de check engine */
    if (g_engine.active_faults & ~FAULT_REV_LIMIT) {
        CHECK_ENG_ON();
    } else {
        CHECK_ENG_OFF();
    }

    /* BLE telemetria (respeita taxa configurada) */
    static uint32_t s_last_telem = 0;
    if (BLE_ECU_IsConnected() && (now - s_last_telem) >= g_ble_telem_rate_ms) {
        s_last_telem = now;
        BLE_ECU_SendTelemetry(&g_engine, g_engine_state);
        BLE_ECU_SendFaults(&g_engine);
    }

#ifdef FEATURE_CAN_BUS
    DRV_CAN_SendTelemetry(&g_engine);
#endif
}
