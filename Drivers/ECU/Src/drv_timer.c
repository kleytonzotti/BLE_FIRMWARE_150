/**
 * drv_timer.c — Timers em nível de registrador (sem HAL).
 *
 * TIM2  — CKP/CMP capture (CH1=CKP, CH2=CMP), clock: 64MHz
 * TIM1  — Injetores 1-4 Output Compare (CH1-CH4), IRQ OC
 * TIM8  — Ignição 1-4 Output Compare (CH1-CH4)
 * TIM3  — Scheduler geral 100µs (UP counter IRQ)
 * TIM16 — Wastegate PWM (CH1)
 * TIM17 — CDI boost PWM 100kHz (CH1)
 *
 * Relógio do sistema: SYSCLK = 64MHz (HCLK=64MHz, APB1=32MHz, APB2=64MHz)
 * TIM2/3/4/6/7 ficam no APB1 (×2 → 64MHz efetivo)
 * TIM1/8/16/17  ficam no APB2 (64MHz direto)
 */

#include "drv_timer.h"
#include "ecu_config.h"
#include "pinout.h"

/* ----------------------------------------------------------------
 * Helpers de registrador (evita HAL)
 * ---------------------------------------------------------------- */
#define RCC_APB1ENR1   (*((volatile uint32_t *)0x40021058UL))
#define RCC_APB2ENR    (*((volatile uint32_t *)0x40021060UL))

#define TIM2_BASE   0x40000000UL
#define TIM3_BASE   0x40000400UL
#define TIM1_BASE   0x40012C00UL
#define TIM8_BASE   0x40013400UL
#define TIM16_BASE  0x40014400UL
#define TIM17_BASE  0x40014800UL

typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2;
    volatile uint32_t CCER, CNT, PSC, ARR, RCR, CCR1, CCR2, CCR3, CCR4;
    volatile uint32_t BDTR, DCR, DMAR, OR1, CCMR3, CCR5, CCR6, AF1, AF2;
} TIM_Regs_t;

#define TIM2  ((TIM_Regs_t *)TIM2_BASE)
#define TIM3  ((TIM_Regs_t *)TIM3_BASE)
#define TIM1  ((TIM_Regs_t *)TIM1_BASE)
#define TIM8  ((TIM_Regs_t *)TIM8_BASE)
#define TIM16 ((TIM_Regs_t *)TIM16_BASE)
#define TIM17 ((TIM_Regs_t *)TIM17_BASE)

/* Tick global para GetMs (incrementado no SysTick_Handler) */
static volatile uint32_t s_ms_tick = 0;

void SysTick_Handler_Inc(void) { s_ms_tick++; }

uint32_t DRV_Timer_GetMs(void) { return s_ms_tick; }

/* Delay bloqueante em µs (usa DWT ou loop calibrado) */
void DRV_Timer_DelayUs(uint32_t us)
{
    /* DWT: habilitado no SystemInit / startup */
    extern volatile uint32_t *DWT_CYCCNT;
    uint32_t start = *DWT_CYCCNT;
    uint32_t ticks = us * (ECU_SYSCLOCK_HZ / 1000000UL);
    while ((*DWT_CYCCNT - start) < ticks);
}

/* ----------------------------------------------------------------
 * CKP — TIM2 CH1 Input Capture
 * Período: tempo entre bordas de subida do sensor Hall/Relutância
 * ---------------------------------------------------------------- */
void DRV_Timer_CKP_Init(void)
{
    /* Enable TIM2 clock */
    RCC_APB1ENR1 |= (1U << 0);  /* TIM2EN */

    TIM2->CR1  = 0;
    TIM2->PSC  = 63;    /* 64MHz / 64 = 1MHz → resolução 1µs */
    TIM2->ARR  = 0xFFFFFFFFUL;
    TIM2->CCMR1 = (0x1U << 0)  /* CC1S=01: CH1 como input mapeado em TI1 */
               | (0x3U << 2)   /* IC1PSC=11: sem pré-escala */
               | (0x0U << 4);  /* IC1F=0000: sem filtro */
    TIM2->CCER = (1U << 0);    /* CC1E=1: captura ativa, borda de subida */
    TIM2->DIER = (1U << 1);    /* CC1IE: IRQ na captura */
    TIM2->CR1  = (1U << 0);    /* CEN */

    /* NVIC: prioridade 0 para CKP */
    /* TIM2_IRQn = 28 */
    volatile uint32_t *NVIC_IPR = (volatile uint32_t *)0xE000E400UL;
    NVIC_IPR[7] = (NVIC_IPR[7] & ~(0xFFU << 0)) | (0x00U << 0); /* prio 0 */
    volatile uint32_t *NVIC_ISER = (volatile uint32_t *)0xE000E100UL;
    NVIC_ISER[0] |= (1U << 28);
}

uint32_t DRV_Timer_GetCrankTick(void)
{
    return TIM2->CNT;
}

uint16_t DRV_Timer_PeriodToRPM(uint32_t period_us, uint8_t teeth)
{
    if (period_us == 0 || teeth == 0) return 0;
    /* RPM = 60 × 1000000 / (period_us × teeth_per_rev/2) */
    /* teeth_per_rev/2 porque 1 volta do virabrequim = 2 voltas do árvo de cames,
     * mas o anel tem teeth dentes por volta do virabrequim */
    uint32_t rpm = 60000000UL / (period_us * (uint32_t)teeth);
    if (rpm > 15000) rpm = 15000;
    return (uint16_t)rpm;
}

/* ----------------------------------------------------------------
 * Scheduler geral — TIM3 100µs, prioridade 1
 * ---------------------------------------------------------------- */
void DRV_Timer_Scheduler_Init(void)
{
    RCC_APB1ENR1 |= (1U << 1);  /* TIM3EN */

    TIM3->CR1  = 0;
    TIM3->PSC  = 63;            /* 64MHz / 64 = 1MHz */
    TIM3->ARR  = 99;            /* 100 ticks = 100µs */
    TIM3->DIER = (1U << 0);     /* UIE: IRQ no overflow */
    TIM3->SR   = 0;
    TIM3->CR1  = (1U << 0);     /* CEN */

    /* NVIC: prioridade 1, TIM3_IRQn = 29 */
    volatile uint32_t *NVIC_IPR = (volatile uint32_t *)0xE000E400UL;
    NVIC_IPR[7] = (NVIC_IPR[7] & ~(0xFFU << 8)) | (0x40U << 8); /* prio 1 */
    volatile uint32_t *NVIC_ISER = (volatile uint32_t *)0xE000E100UL;
    NVIC_ISER[0] |= (1U << 29);
}

uint32_t DRV_Timer_Scheduler_GetTick(void)
{
    return TIM3->CNT;
}

/* ----------------------------------------------------------------
 * INJ — TIM1 Output Compare (CH1-CH4)
 * ---------------------------------------------------------------- */
void DRV_Timer_INJ_Init(void)
{
    RCC_APB2ENR |= (1U << 11);  /* TIM1EN */

    TIM1->CR1  = 0;
    TIM1->PSC  = 63;            /* 1µs tick */
    TIM1->ARR  = 0xFFFFUL;
    /* CC1-CC4 como output compare, sem PWM */
    TIM1->CCMR1 = (0x0U << 0) | (0x0U << 8);  /* CC1S=00, CC2S=00: output */
    TIM1->CCMR2 = (0x0U << 0) | (0x0U << 8);
    TIM1->CCER  = 0;            /* Saídas desativadas (INJ via GPIO direto) */
    TIM1->DIER  = (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4); /* CC1-4IE */
    TIM1->BDTR  = (1U << 15);   /* MOE */
    TIM1->CR1   = (1U << 0);
}

void DRV_Timer_INJ_Schedule(uint8_t ch, uint32_t open_us, uint32_t close_us)
{
    switch (ch) {
        case 0: TIM1->CCR1 = open_us; break;
        case 1: TIM1->CCR2 = open_us; break;
        case 2: TIM1->CCR3 = open_us; break;
        case 3: TIM1->CCR4 = open_us; break;
        default: break;
    }
    (void)close_us;  /* Fechamento controlado pelo scheduler */
}

/* ----------------------------------------------------------------
 * IGN — TIM8 Output Compare (CH1-CH4)
 * ---------------------------------------------------------------- */
void DRV_Timer_IGN_Init(void)
{
    RCC_APB2ENR |= (1U << 13);  /* TIM8EN */

    TIM8->CR1  = 0;
    TIM8->PSC  = 63;
    TIM8->ARR  = 0xFFFFUL;
    TIM8->CCMR1 = 0;
    TIM8->CCMR2 = 0;
    TIM8->CCER  = 0;
    TIM8->DIER  = 0;
    TIM8->BDTR  = (1U << 15);
    TIM8->CR1   = (1U << 0);
}

void DRV_Timer_IGN_ScheduleDwell(uint8_t coil, uint32_t tick_start)
{
    (void)coil; (void)tick_start;
    /* Controlado via GPIO na ISR do TIM8 */
}

void DRV_Timer_IGN_ScheduleFire(uint8_t coil, uint32_t tick_fire)
{
    (void)coil; (void)tick_fire;
}

/* ----------------------------------------------------------------
 * Wastegate — TIM16 PWM (CH1)
 * ---------------------------------------------------------------- */
void DRV_Timer_WG_Init(uint32_t freq_hz)
{
    RCC_APB2ENR |= (1U << 17);  /* TIM16EN */

    TIM16->CR1  = 0;
    TIM16->PSC  = 63;           /* 1MHz */
    TIM16->ARR  = (uint16_t)(1000000UL / freq_hz - 1);
    TIM16->CCMR1 = (0x6U << 4); /* OC1M = PWM mode 1 */
    TIM16->CCR1  = 0;
    TIM16->CCER  = (1U << 0);   /* CC1E */
    TIM16->BDTR  = (1U << 15);
    TIM16->CR1   = (1U << 0);
}

void DRV_Timer_WG_SetDuty(uint8_t duty_pct)
{
    uint32_t arr = TIM16->ARR + 1;
    TIM16->CCR1 = (uint16_t)(arr * duty_pct / 100);
}

/* ----------------------------------------------------------------
 * CDI Boost — TIM17 PWM 100kHz (CH1)
 * ---------------------------------------------------------------- */
void DRV_Timer_CDI_Boost_Init(uint32_t freq_hz)
{
    RCC_APB2ENR |= (1U << 18);  /* TIM17EN */

    TIM17->CR1   = 0;
    TIM17->PSC   = 0;           /* 64MHz direto */
    TIM17->ARR   = (uint16_t)(ECU_SYSCLOCK_HZ / freq_hz - 1);
    TIM17->CCMR1 = (0x6U << 4);
    TIM17->CCR1  = 0;
    TIM17->CCER  = (1U << 0);
    TIM17->BDTR  = (1U << 15);
    TIM17->CR1   = (1U << 0);
}

void DRV_Timer_CDI_Boost_SetDuty(uint8_t duty_pct)
{
    uint32_t arr = TIM17->ARR + 1;
    TIM17->CCR1 = (uint16_t)(arr * duty_pct / 100);
}

void DRV_Timer_CDI_Boost_Stop(void)
{
    TIM17->CR1  = 0;
    TIM17->CCR1 = 0;
}
