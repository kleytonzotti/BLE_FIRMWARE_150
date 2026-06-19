/**
 * main.c — ECU Programável STM32WB55 — Loop principal.
 *
 * Sequência de boot:
 *   1. SystemInit: clocks, DWT, SysTick
 *   2. GPIO: todos os pinos
 *   3. BLE stack (CPU2) via SHCI
 *   4. ADC + DMA circular
 *   5. Timers: CKP, Scheduler, INJ, IGN, WG
 *   6. ECU: carrega perfil, inicia state machine
 *   7. Loop: tasks periódicas + BLE events
 *
 * ISR de alta prioridade (configuradas em drv_timer.c):
 *   TIM2_IRQHandler (prio 0) → OnCrankTooth
 *   TIM3_IRQHandler (prio 1) → ECU_Engine_Scheduler_100us
 */

#include <stdint.h>
#include "ecu_config.h"
#include "ecu_engine.h"
#include "ecu_sensors.h"
#include "drv_gpio.h"
#include "drv_adc.h"
#include "drv_timer.h"
#include "app_ble.h"
#include "pinout.h"
#include "ecu_sim.h"

/* ----------------------------------------------------------------
 * DWT para delay em µs (habilitado antes dos timers)
 * ---------------------------------------------------------------- */
volatile uint32_t *DWT_CYCCNT = (volatile uint32_t *)0xE0001004UL;

static void _dwt_init(void)
{
    /* CoreDebug->DEMCR |= TRCENA */
    volatile uint32_t *CoreDebug_DEMCR = (volatile uint32_t *)0xE000EDFC;
    *CoreDebug_DEMCR |= (1U << 24);

    /* DWT->CYCCNT = 0 */
    *DWT_CYCCNT = 0;

    /* DWT->CTRL |= CYCCNTENA */
    volatile uint32_t *DWT_CTRL = (volatile uint32_t *)0xE0001000;
    *DWT_CTRL |= (1U << 0);
}

/* ----------------------------------------------------------------
 * SysTick: 1ms
 * ---------------------------------------------------------------- */
static void _systick_init(void)
{
    /* SysTick: 64MHz / 64000 = 1kHz = 1ms */
    volatile uint32_t *SYST_RVR  = (volatile uint32_t *)0xE000E014;
    volatile uint32_t *SYST_CVR  = (volatile uint32_t *)0xE000E018;
    volatile uint32_t *SYST_CSR  = (volatile uint32_t *)0xE000E010;

    *SYST_RVR = (64000U - 1U);
    *SYST_CVR = 0;
    *SYST_CSR = 7U;  /* CLKSOURCE=1, TICKINT=1, ENABLE=1 */
}

/* ----------------------------------------------------------------
 * Vetor de interrupção global (handler)
 * ---------------------------------------------------------------- */
void SysTick_Handler(void)
{
    SysTick_Handler_Inc();  /* incrementa ms_tick em drv_timer.c */
}

void TIM2_IRQHandler(void)
{
    /* Captura CKP: lê CCR1, calcula período */
    extern void _tim2_ckp_handler(void);
    _tim2_ckp_handler();
}

void TIM3_IRQHandler(void)
{
    /* Limpa flag UIE */
    /* TIM3->SR = 0 — feito dentro do handler */
    volatile uint32_t *TIM3_SR = (volatile uint32_t *)0x40000410UL;
    *TIM3_SR = 0;
    ECU_Engine_Scheduler_100us();
}

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void)
{
    /* 1. Infraestrutura base */
    _dwt_init();
    _systick_init();

    /* 2. Todos os GPIOs da ECU */
    DRV_GPIO_Init();

    /* 3. BLE stack (CPU2 deve estar rodando antes de aci_gap_init) */
    APP_BLE_Init();

    /* 4. ECU: carrega perfil, inicializa subsistemas e timers */
    ECU_Engine_Init();

    /* 5. Simulação: OLED + botões da STM32WB5MM-DK */
    ECU_Sim_Init();

    /* ----------------------------------------------------------------
     * Loop principal
     * Nenhuma lógica de tempo crítico aqui — tudo via ISR + tasks.
     * ---------------------------------------------------------------- */
    uint32_t last_led_blink = 0;

    for (;;) {
        /* BLE: processa eventos do stack */
        APP_BLE_Process();

        /* Tasks periódicas da ECU */
        ECU_Engine_Task_10ms();
        ECU_Engine_Task_50ms();
        ECU_Engine_Task_100ms();

        /* Simulação: lê botões, injeta valores em g_engine, atualiza OLED */
        ECU_Sim_Task_10ms();

        /* Blink LED de heartbeat: 500ms */
        uint32_t now = DRV_Timer_GetMs();
        if ((now - last_led_blink) >= 500) {
            last_led_blink = now;
            /* Toggle LED PB0 via BSRR */
            extern volatile uint32_t *s_led_state;
            static uint8_t led_st = 0;
            led_st ^= 1;
            volatile uint32_t *GPIOB_BSRR = (volatile uint32_t *)0x48000418UL;
            if (led_st) {
                *GPIOB_BSRR = (1U << 0);       /* Set PB0 */
            } else {
                *GPIOB_BSRR = (1U << (0 + 16)); /* Reset PB0 */
            }
        }

        /* Modo limp: se CKP ausente por 3 segundos, desliga tudo */
        static uint32_t last_rpm_nonzero = 0;
        if (g_engine.rpm > 0) last_rpm_nonzero = DRV_Timer_GetMs();
        if (g_engine_state >= ENGINE_CRANKING &&
            (DRV_Timer_GetMs() - last_rpm_nonzero) > 3000) {
            ECU_Engine_SetState(ENGINE_OFF);
        }

        /* Priming timeout: desliga bomba se não ligar em 5s */
        static uint32_t priming_start = 0;
        if (g_engine_state == ENGINE_PRIMING) {
            if (priming_start == 0) priming_start = DRV_Timer_GetMs();
            if ((DRV_Timer_GetMs() - priming_start) > 5000) {
                FUEL_PUMP_OFF();
            }
        } else {
            priming_start = 0;
        }
    }
}
