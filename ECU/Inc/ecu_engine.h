/**
 * ecu_engine.h — Máquina de estados do motor e coordenação dos subsistemas.
 */
#ifndef ECU_ENGINE_H
#define ECU_ENGINE_H

#include "ecu_types.h"
#include "ecu_config.h"

/* Variáveis globais — definidas em ecu_engine.c */
extern volatile EngineState_t        g_engine;
extern volatile EngineStateMachine_t g_engine_state;
extern EcuProfile_t                  g_profile;

void ECU_Engine_Init(void);
void ECU_Engine_SetState(EngineStateMachine_t new_state);
void ECU_Engine_EmergencyStop(void);
void ECU_Engine_StartPriming(void);

/* Tarefas periódicas — chamadas do main loop */
void ECU_Engine_Task_10ms(void);   /* Leitura de sensores */
void ECU_Engine_Task_50ms(void);   /* PID de marcha lenta */
void ECU_Engine_Task_100ms(void);  /* Diagnósticos + BLE */

/* Callbacks de IRQ — CRÍTICO: executam em contexto de interrupção */
void OnCrankTooth(uint32_t period_us, uint8_t tooth_num);
void OnCamTooth(void);

#endif /* ECU_ENGINE_H */
