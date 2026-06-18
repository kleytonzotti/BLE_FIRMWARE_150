/**
 * drv_timer.h — Drivers de timer via registradores diretos STM32WB55.
 *
 * TIM2  (32-bit, 1MHz): CKP/CMP Input Capture
 * TIM1  (16-bit, 1MHz): Injetores Output Compare
 * TIM8  (16-bit, 1MHz): Ignição Output Compare
 * TIM3  (16-bit, 1MHz): Scheduler 100µs
 * TIM16 (16-bit, 1kHz): Wastegate PWM
 * TIM17 (16-bit, 100kHz): CDI Boost PWM
 */
#ifndef DRV_TIMER_H
#define DRV_TIMER_H

#include <stdint.h>

/* TIM2 — CKP/CMP Input Capture */
void     DRV_Timer_CKP_Init(void);
uint32_t DRV_Timer_GetCrankTick(void);          /* Lê TIM2->CNT diretamente */
uint16_t DRV_Timer_PeriodToRPM(uint32_t period_us, uint8_t teeth_per_rev);

/* TIM1 — Injetores Output Compare */
void DRV_Timer_INJ_Init(void);
void DRV_Timer_INJ_Schedule(uint8_t ch, uint32_t open_tick, uint32_t close_tick);

/* TIM8 — Ignição Output Compare */
void DRV_Timer_IGN_Init(void);
void DRV_Timer_IGN_ScheduleDwell(uint8_t ch, uint32_t dwell_start_tick);
void DRV_Timer_IGN_ScheduleFire(uint8_t ch, uint32_t fire_tick);

/* TIM3 — Scheduler 100µs */
void     DRV_Timer_Scheduler_Init(void);
uint32_t DRV_Timer_Scheduler_GetTick(void);

/* TIM16 — Wastegate PWM (1kHz padrão) */
void DRV_Timer_WG_Init(uint32_t freq_hz);
void DRV_Timer_WG_SetDuty(uint8_t duty_pct);

/* TIM17 — CDI Boost PWM (100kHz) */
void DRV_Timer_CDI_Boost_Init(uint32_t freq_hz);
void DRV_Timer_CDI_Boost_SetDuty(uint8_t duty_pct);
void DRV_Timer_CDI_Boost_Stop(void);

/* Utilitários de tempo */
uint32_t DRV_Timer_GetMs(void);           /* SysTick em ms desde boot */
void     DRV_Timer_DelayUs(uint32_t us);  /* Delay bloqueante curto */

#endif /* DRV_TIMER_H */
