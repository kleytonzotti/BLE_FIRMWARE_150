/**
 * ecu_sim.h — Simulador de cenários ECU usando hardware da STM32WB5MM-DK.
 *
 * Usa o display OLED onboard para mostrar o estado de cada simulação
 * e as duas teclas (SW1/SW2) para navegar entre os 8 cenários.
 *
 * SW1 (PC4) → próximo cenário
 * SW2 (PE4) → cenário anterior
 *
 * O módulo injeta valores simulados diretamente em g_engine (ecu_engine.h),
 * sobrescrevendo os valores reais de ADC — útil durante desenvolvimento
 * sem hardware de sensor conectado.
 *
 * Ativação: defina FEATURE_SIMULATION em ecu_config.h
 */
#ifndef ECU_SIM_H
#define ECU_SIM_H

#include <stdint.h>
#include "ecu_types.h"

/* ----------------------------------------------------------------
 * Cenários disponíveis
 * ---------------------------------------------------------------- */
typedef enum {
    SIM_IDLE         = 0,  /* Marcha lenta quente                */
    SIM_ACCEL        = 1,  /* Aceleração parcial 3500 RPM        */
    SIM_FULL_LOAD    = 2,  /* Carga plena 6000 RPM               */
    SIM_COLD_START   = 3,  /* Partida a frio (CLT=15°C)          */
    SIM_OVERTEMP     = 4,  /* Superaquecimento (CLT=118°C)       */
    SIM_CDI_MOTO     = 5,  /* CDI moto carburada (sem injeção)   */
    SIM_KNOCK        = 6,  /* Detonação detectada                */
    SIM_REV_LIMIT    = 7,  /* Corte de combustível (RPM limite)  */
    SIM_COUNT        = 8,
} SimMode_t;

/* ----------------------------------------------------------------
 * API pública
 * ---------------------------------------------------------------- */
void      ECU_Sim_Init(void);
void      ECU_Sim_Task_10ms(void);  /* Chamar no main loop a cada 10ms */
SimMode_t ECU_Sim_GetMode(void);
void      ECU_Sim_NextMode(void);   /* Avança um cenário */
void      ECU_Sim_PrevMode(void);   /* Retrocede um cenário */

#endif /* ECU_SIM_H */
