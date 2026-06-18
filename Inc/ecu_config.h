/**
 * ecu_config.h — Configuração mestre da ECU Programável Universal.
 * Altere aqui o tipo de veículo e os features ativos.
 * Pode ser sobrescrito via BLE e salvo em flash.
 */
#ifndef ECU_CONFIG_H
#define ECU_CONFIG_H

/* ================================================================
 * VERSÃO DO FIRMWARE
 * ================================================================ */
#define ECU_FW_MAJOR        3
#define ECU_FW_MINOR        0
#define ECU_FW_PATCH        1
#define ECU_FW_BUILD_DATE   "260617"   /* AAMMDD */

/* ================================================================
 * SELEÇÃO DE VEÍCULO PADRÃO (pode ser alterado via BLE)
 * Descomente apenas UM.
 * ================================================================ */
// #define VEHICLE_HONDA_CG150
// #define VEHICLE_HONDA_CG160
// #define VEHICLE_HONDA_CB160
// #define VEHICLE_HONDA_TITAN160
// #define VEHICLE_HONDA_PCX150
// #define VEHICLE_YAMAHA_YBR125
// #define VEHICLE_YAMAHA_FACTOR150
// #define VEHICLE_GENERICO_MOTO_1CIL
// #define VEHICLE_GENERICO_MOTO_2CIL
// #define VEHICLE_GENERICO_CARRO_4CIL
#define VEHICLE_GENERICO_CARRO_4CIL_TURBO
// #define VEHICLE_CDI_HONDA_CG150
// #define VEHICLE_CDI_GENERICO_1CIL

/* ================================================================
 * MODO DE IGNIÇÃO (pode ser alterado via BLE)
 * ================================================================ */
#define IGN_MODE_INDUCTIVE          /* Bobina indutiva padrão */
// #define IGN_MODE_CDI             /* CDI — capacitor discharge */
// #define IGN_MODE_DIS             /* Direct Ignition System (COP) */

/* ================================================================
 * MODO DE INJEÇÃO (pode ser alterado via BLE)
 * ================================================================ */
#define INJECTION_SEQUENTIAL        /* Por cilindro */
// #define INJECTION_BATCH          /* Em par */
// #define INJECTION_SEMISEQUENTIAL

/* ================================================================
 * SENSOR DE CARGA (pode ser alterado via BLE)
 * ================================================================ */
#define LOAD_SPEED_DENSITY          /* MAP sensor */
// #define LOAD_ALPHA_N             /* TPS */
// #define LOAD_HYBRID              /* MAP + TPS */

/* ================================================================
 * COMBUSTÍVEL (pode ser alterado via BLE)
 * ================================================================ */
#define FUEL_GASOLINE
// #define FUEL_ETHANOL
// #define FUEL_FLEX

/* ================================================================
 * FEATURES — Comente para desabilitar
 * ================================================================ */
#define FEATURE_WIDEBAND_O2         /* Sonda lambda wideband LSU 4.9 */
#define FEATURE_KNOCK_SENSOR        /* Sensor de detonação */
#define FEATURE_IDLE_CONTROL        /* Controle IAC (stepper ou duty) */
#define FEATURE_BOOST_CONTROL       /* Controle de wastegate */
#define FEATURE_FLEX_FUEL           /* Sensor de composição E0-E100 */
#define FEATURE_VSS                 /* Sensor de velocidade */
#define FEATURE_LAUNCH_CONTROL      /* Launch control */
#define FEATURE_REV_LIMITER         /* Limitador de RPM */
#define FEATURE_TRACTION_CONTROL    /* Controle de tração */
#define FEATURE_CAN_BUS             /* CAN 2.0B via MCP2515 */
#define FEATURE_DATALOG_BLE         /* Datalogging via BLE */
#define FEATURE_AUTOTUNE            /* Autotune pelo lambda */
#define FEATURE_EGT                 /* Temperatura do escapamento */
#define FEATURE_OIL_PRESSURE        /* Pressão do óleo */
#define FEATURE_FUEL_PRESSURE       /* Pressão do combustível */
#define FEATURE_FLAT_SHIFT          /* Flat shift / antilag */
// #define FEATURE_CDI_MODE         /* Modo CDI carburado — desativa injeção */

/* ================================================================
 * PARÂMETROS DO MCU — STM32WB55 @ 64 MHz
 * ================================================================ */
#define ECU_SYSCLOCK_HZ             64000000UL
#define ECU_APB1_HZ                 64000000UL
#define ECU_APB2_HZ                 64000000UL

/* Timer do virabrequim: resolução 1µs */
#define ECU_CRANK_TIMER_FREQ_HZ     1000000UL
#define ECU_SCHEDULER_TICK_US       100        /* Scheduler de injetores: 100µs */

/* ================================================================
 * PERÍODOS DE TAREFAS
 * ================================================================ */
#define ECU_SENSOR_PERIOD_MS        10         /* Leitura de sensores */
#define ECU_IDLE_PERIOD_MS          50         /* PID de marcha lenta */
#define ECU_DIAG_PERIOD_MS          100        /* Diagnósticos */
#define ECU_BLE_TELEM_PERIOD_MS     100        /* Telemetria BLE padrão */
#define ECU_BLE_FAULT_PERIOD_MS     500        /* Envio de falhas BLE */
#define ECU_CAN_PERIOD_MS           10         /* Mensagens CAN */
#define ECU_DATALOG_PERIOD_MS       50         /* Datalog rápido */

/* ================================================================
 * WATCHDOG
 * ================================================================ */
#define ECU_IWDG_TIMEOUT_MS         500

/* ================================================================
 * LIMITES DE SEGURANÇA ABSOLUTOS
 * ================================================================ */
#define ECU_MAX_RPM                 15000
#define ECU_DEFAULT_REV_LIMIT       11000
#define ECU_MAX_COOLANT_C           115
#define ECU_MIN_BATT_MV             9000
#define ECU_MAX_BATT_MV             16500
#define ECU_MAX_INJ_PW_US           25000
#define ECU_MIN_INJ_PW_US           200
#define ECU_MAX_DWELL_US            6000
#define ECU_MIN_DWELL_US            500

/* ================================================================
 * ADC
 * ================================================================ */
#define ADC_CHANNELS                12
#define ADC_OVERSAMPLE              16          /* Média de 16 amostras */
#define ADC_VREF_MV                 3300

/* Índices no buffer DMA do ADC1 */
#define ADC_CH_TPS                  0
#define ADC_CH_MAP                  1
#define ADC_CH_CLT                  2
#define ADC_CH_IAT                  3
#define ADC_CH_BATT                 4
#define ADC_CH_O2                   5
#define ADC_CH_KNOCK                6
#define ADC_CH_BOOST                7
#define ADC_CH_OIL                  8
#define ADC_CH_FUEL_PRESS           9
#define ADC_CH_EGT                  10
#define ADC_CH_SPARE                11

/* ================================================================
 * TABELAS — dimensões fixas 16×16
 * ================================================================ */
#define FUEL_TABLE_ROWS             16
#define FUEL_TABLE_COLS             16
#define IGN_TABLE_ROWS              16
#define IGN_TABLE_COLS              16
#define AFR_TABLE_ROWS              16
#define AFR_TABLE_COLS              16
#define CDI_TABLE_POINTS            32          /* Pontos da curva CDI */
#define TEMP_CORR_POINTS            10          /* Pontos de correção por temperatura */

/* ================================================================
 * FLASH — STM32WB55 (1MB)
 * Página = 4KB. Últimas 2 páginas para config.
 * ================================================================ */
#define ECU_FLASH_BASE              0x08000000UL
#define ECU_FLASH_SIZE              0x00100000UL  /* 1MB */
#define ECU_CONFIG_PAGE_A           0x080FE000UL  /* Penúltima página */
#define ECU_CONFIG_PAGE_B           0x080FF000UL  /* Última página */
#define ECU_CONFIG_PAGE_SIZE        0x1000        /* 4KB */
#define ECU_CONFIG_MAGIC            0xEC04321UL
#define ECU_CONFIG_MAGIC_END        0xABCD1234UL

/* ================================================================
 * BLE
 * ================================================================ */
#define BLE_TELEM_PACKET_SIZE       40
#define BLE_CONFIG_PACKET_SIZE      64
#define BLE_CHUNK_PAYLOAD_SIZE      60
#define BLE_FAULTMAP_SIZE           8

/* ================================================================
 * CAN (MCP2515 via SPI)
 * ================================================================ */
#define CAN_BAUD_500K               500000UL
#define CAN_ID_ECU_TELEM            0x700       /* ID padrão telemetria */
#define CAN_ID_ECU_FAULTS           0x701
#define CAN_ID_ECU_CONFIG           0x702
#define CAN_ID_PAINEL               0x710       /* Painel digital */

/* ================================================================
 * CDI
 * ================================================================ */
#define CDI_CAPACITOR_VOLTAGE_V     350         /* Tensão de carga do capacitor */
#define CDI_BOOST_PWM_FREQ_HZ       100000      /* Frequência do conversor boost */
#define CDI_BOOST_DUTY_MAX          75          /* Duty máximo do boost */

/* ================================================================
 * HARDWARE
 * ================================================================ */
#define ECU_HW_REV                  1

#endif /* ECU_CONFIG_H */
