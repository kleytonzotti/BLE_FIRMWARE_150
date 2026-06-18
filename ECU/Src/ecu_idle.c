/**
 * ecu_idle.c — Controle PID de marcha lenta via IAC stepper.
 */

#include "ecu_idle.h"
#include "pinout.h"
#include "ecu_config.h"

static IacType_t s_iac_type     = IAC_STEPPER;
static int16_t   s_position     = 0;   /* Posição atual em passos */
static uint16_t  s_target_rpm   = 1200;
static int32_t   s_integral     = 0;
static int16_t   s_prev_error   = 0;

void ECU_Idle_Init(IacType_t type)
{
    s_iac_type = type;
    s_integral = 0;
    s_prev_error = 0;

    if (type == IAC_STEPPER) {
        IAC_ENABLE();
    }
}

void ECU_Idle_PrePosition(int16_t coolant_c_x10)
{
    /* Motor frio → mais passos abertos (mais ar para manter idle)
     * Curva simples: < 20°C → 60 passos, > 80°C → 10 passos */
    int16_t temp = coolant_c_x10 / 10;
    int16_t steps;

    if      (temp < -20) steps = 80;
    else if (temp <   0) steps = 70;
    else if (temp <  20) steps = 60;
    else if (temp <  40) steps = 45;
    else if (temp <  60) steps = 30;
    else if (temp <  80) steps = 18;
    else                 steps = 10;

    /* Move para a posição pré-calculada */
    ECU_Idle_Reset();
    ECU_Idle_StepOpen((uint8_t)steps);
}

void ECU_Idle_SetTarget(uint16_t target_rpm)
{
    s_target_rpm = target_rpm;
}

void ECU_Idle_Update(volatile EngineState_t *eng, const EcuProfile_t *prof)
{
    if (s_iac_type == IAC_NONE) return;

    uint16_t target = prof->idle_target_rpm;
    if (s_target_rpm > 0) target = s_target_rpm;

    /* Erro = alvo - atual */
    int16_t error = (int16_t)target - (int16_t)eng->rpm;

    /* Ganhos do PID (divididos por 100 para escalar) */
    int32_t Kp = prof->idle_p_gain;
    int32_t Ki = prof->idle_i_gain;
    int32_t Kd = prof->idle_d_gain;

    if (Kp == 0) { Kp = 50;  }  /* Padrão seguro */
    if (Ki == 0) { Ki = 10;  }
    if (Kd == 0) { Kd = 5;   }

    /* Integrador com anti-windup */
    s_integral += (int32_t)error * Ki / 100;
    s_integral = ECU_CLAMP(s_integral, -5000, 5000);

    int32_t derivative = ((int32_t)error - s_prev_error) * Kd / 100;
    int32_t output = (int32_t)error * Kp / 100 + s_integral + derivative;
    s_prev_error = error;

    /* Converte output em passos do stepper */
    int16_t steps = (int16_t)(output / 100);

    if (steps > 0 && s_position < prof->iac_steps_max) {
        steps = ECU_MIN(steps, (int32_t)(prof->iac_steps_max - s_position));
        ECU_Idle_StepOpen((uint8_t)steps);
    } else if (steps < 0 && s_position > 0) {
        steps = (int16_t)ECU_MAX(steps, -(int32_t)s_position);
        ECU_Idle_StepClose((uint8_t)(-steps));
    }

    eng->iac_position  = s_position;
    eng->idle_error    = error;
    eng->idle_integral = s_integral;
}

void ECU_Idle_StepOpen(uint8_t steps)
{
    IAC_DIR_OPEN();
    for (uint8_t i = 0; i < steps; i++) {
        IAC_STEP();
        s_position++;
    }
}

void ECU_Idle_StepClose(uint8_t steps)
{
    IAC_DIR_CLOSE();
    for (uint8_t i = 0; i < steps; i++) {
        if (s_position == 0) break;
        IAC_STEP();
        s_position--;
    }
}

int16_t ECU_Idle_GetPosition(void)
{
    return s_position;
}

void ECU_Idle_Reset(void)
{
    /* Vai para zero fechando o máximo de passos possível */
    IAC_DIR_CLOSE();
    for (uint16_t i = 0; i < 200; i++) {
        IAC_STEP();
    }
    s_position = 0;
}
