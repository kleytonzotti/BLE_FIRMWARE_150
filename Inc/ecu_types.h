/**
 * ecu_types.h — Tipos centrais da ECU Programável Universal.
 * Todos os módulos incluem este arquivo.
 */
#ifndef ECU_TYPES_H
#define ECU_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ecu_config.h"
#include "ecu_fault.h"

/* ================================================================
 * ENUMS DE CONFIGURAÇÃO
 * ================================================================ */
typedef enum { VEHICLE_MOTO = 0, VEHICLE_CARRO = 1 } VehicleType_t;
typedef enum { ENGINE_1CYL=1, ENGINE_2CYL=2, ENGINE_3CYL=3, ENGINE_4CYL=4 } CylinderCount_t;
typedef enum { INJ_SEQUENTIAL=0, INJ_BATCH=1, INJ_SEMISEQUENTIAL=2, INJ_NONE=3 } InjectionType_t;
typedef enum { IGN_INDUCTIVE=0, IGN_CDI=1, IGN_DIS=2, IGN_COIL=0 } IgnitionType_t;
typedef enum { FUEL_GAS=0, FUEL_ETHANOL=1, FUEL_FLEX=2 } FuelType_t;
typedef enum { LOAD_MAP=0, LOAD_TPS=1, LOAD_HYBRID=2 } LoadType_t;
typedef enum { IAC_STEPPER=0, IAC_DUTY=1, IAC_NONE=2 } IacType_t;

/* IDs de perfil de veículo para CMD_RESET_TO_DEFAULT */
typedef enum {
    PROFILE_GENERICO_MOTO_1CIL      = 0x00,
    PROFILE_GENERICO_MOTO_2CIL      = 0x01,
    PROFILE_GENERICO_CARRO_4CIL     = 0x02,
    PROFILE_GENERICO_CARRO_4CIL_TURBO = 0x03,
    PROFILE_HONDA_CG150             = 0x10,
    PROFILE_HONDA_CG160             = 0x11,
    PROFILE_HONDA_CB160             = 0x12,
    PROFILE_HONDA_TITAN160          = 0x13,
    PROFILE_HONDA_PCX150            = 0x14,
    PROFILE_YAMAHA_YBR125           = 0x20,
    PROFILE_YAMAHA_FACTOR150        = 0x21,
    PROFILE_CDI_HONDA_CG150         = 0x31,
    PROFILE_CDI_GENERICO_1CIL       = 0x30,
} VehicleProfileId_t;

/* ================================================================
 * PERFIL DO VEÍCULO — salvo em flash, configurável via BLE
 * ================================================================ */
typedef struct __attribute__((packed)) {
    /* Identificação */
    uint32_t magic;
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
    uint8_t  profile_version;
    char     vehicle_name[16];

    /* Tipo */
    uint8_t  vehicle_type;          /* VehicleType_t */
    uint8_t  cylinders;             /* 1-4 */
    uint16_t displacement_cc;
    uint8_t  injection_type;        /* InjectionType_t */
    uint8_t  ignition_type;         /* IgnitionType_t */
    uint8_t  fuel_type;             /* FuelType_t */
    uint8_t  load_type;             /* LoadType_t */

    /* Injetores */
    uint16_t injector_flow_cc_min;
    uint16_t injector_dead_time_us;
    uint16_t fuel_pressure_kpa;
    uint8_t  injector_angle[4];     /* Ângulo BTDC de abertura por cilindro */

    /* Ignição */
    uint8_t  coil_dwell_ms;
    int8_t   ign_base_advance_deg;
    uint8_t  crank_teeth;
    uint8_t  crank_missing_teeth;
    uint8_t  cam_teeth;
    uint8_t  ign_sequence[4];       /* Ordem de ignição */

    /* Limites */
    uint16_t rev_limit_rpm;
    uint16_t launch_rpm;
    uint16_t idle_target_rpm;
    uint8_t  max_coolant_temp_c;
    uint16_t boost_target_kpa;

    /* Calibração de sensores */
    uint16_t tps_min_mv;
    uint16_t tps_max_mv;
    uint16_t map_min_mv;
    uint16_t map_max_mv;
    uint16_t map_max_kpa;
    uint16_t o2_min_mv;
    uint16_t o2_max_mv;
    uint16_t o2_min_afr_x10;        /* ex: 100 = 10.0 */
    uint16_t o2_max_afr_x10;        /* ex: 200 = 20.0 */

    /* Trims globais */
    int8_t   global_fuel_trim;      /* -50 a +50 % */
    int8_t   global_ign_trim;       /* -20 a +20 graus */
    int8_t   global_boost_trim;     /* kPa */

    /* IAC */
    uint8_t  iac_type;              /* IacType_t */
    uint8_t  iac_steps_max;
    int16_t  idle_p_gain;
    int16_t  idle_i_gain;
    int16_t  idle_d_gain;

    /* Features ativas (bitmask) */
    uint32_t features;

    /* Tabelas 16×16 */
    uint8_t  fuel_ve_table[FUEL_TABLE_ROWS][FUEL_TABLE_COLS];     /* VE 0-200 */
    int8_t   ign_advance_table[IGN_TABLE_ROWS][IGN_TABLE_COLS];   /* °BTDC */
    uint8_t  afr_target_table[AFR_TABLE_ROWS][AFR_TABLE_COLS];    /* AFR×10 */

    /* Curva CDI */
    uint16_t cdi_rpm[CDI_TABLE_POINTS];
    uint16_t cdi_advance_x10[CDI_TABLE_POINTS];                   /* graus×10 */

    /* Correções por temperatura */
    int8_t   clt_fuel_corr[TEMP_CORR_POINTS];
    int8_t   iat_fuel_corr[TEMP_CORR_POINTS];
    int8_t   clt_ign_corr[TEMP_CORR_POINTS];
    int8_t   flex_fuel_corr[TEMP_CORR_POINTS];
    int8_t   flex_ign_corr[TEMP_CORR_POINTS];

    /* Eixos das tabelas */
    uint16_t rpm_axis[FUEL_TABLE_ROWS];
    uint8_t  load_axis[FUEL_TABLE_COLS];

    /* Integridade */
    uint32_t crc32;
} EcuProfile_t;

/* ================================================================
 * ESTADO DO MOTOR — RAM, atualizado a cada ciclo
 * ================================================================ */
typedef struct {
    /* Rotação */
    uint16_t  rpm;
    uint16_t  rpm_filtered;
    uint32_t  crank_period_us;
    uint8_t   tooth_count;
    uint8_t   engine_cycle_pos;     /* posição 0-720° */

    /* Sensores calibrados */
    uint16_t  tps_pct;              /* 0-1000 = 0.0-100.0% */
    uint16_t  map_kpa;              /* ×10 → ex: 1013 = 101.3kPa */
    int16_t   coolant_c;            /* ×10 → ex: 850 = 85.0°C */
    int16_t   iat_c;                /* ×10 */
    uint16_t  batt_mv;
    uint16_t  o2_afr_x10;           /* ex: 147 = 14.7 */
    uint16_t  knock_level;          /* 0-1000 */
    uint16_t  vss_kmh;
    uint16_t  flex_pct;             /* % etanol ×10 */
    uint16_t  boost_kpa;            /* ×10 */
    uint16_t  oil_pressure_kpa;
    uint16_t  fuel_pressure_kpa;
    uint16_t  egt_c;

    /* Injeção */
    uint32_t  pulse_width_us[4];
    uint8_t   injector_duty[4];

    /* Ignição */
    int16_t   ign_advance_deg;      /* ×10 */
    uint16_t  dwell_us;
    uint8_t   coil_state[4];

    /* CDI */
    uint16_t  cdi_advance_deg;      /* ×10, para modo CDI */
    uint8_t   cdi_capacitor_ok;

    /* Marcha lenta */
    int16_t   iac_position;
    int16_t   idle_error;
    int32_t   idle_integral;

    /* Turbo */
    uint16_t  wastegate_duty;

    /* Diagnóstico */
    uint32_t  active_faults;
    uint32_t  stored_faults;
    uint8_t   limp_mode;

    /* Estatísticas */
    uint32_t  engine_runtime_s;
    uint32_t  total_injections;
    uint32_t  knock_events;
    uint32_t  rev_limit_events;
} EngineState_t;

/* ================================================================
 * MÁQUINA DE ESTADOS DO MOTOR
 * ================================================================ */
typedef enum {
    ENGINE_OFF        = 0,
    ENGINE_PRIMING    = 1,   /* Priming da bomba (2s) */
    ENGINE_CRANKING   = 2,   /* Partida */
    ENGINE_RUNNING    = 3,   /* Funcionando */
    ENGINE_IDLE       = 4,   /* Marcha lenta */
    ENGINE_DECEL      = 5,   /* Desaceleração — corte */
    ENGINE_REV_LIMIT  = 6,   /* Limitador ativo */
    ENGINE_LIMP       = 7,   /* Modo proteção */
    ENGINE_FAULT      = 8,   /* Falha grave */
    ENGINE_CDI_CRANK  = 9,   /* Partida CDI (carburado) */
    ENGINE_CDI_RUN    = 10,  /* Rodando CDI */
} EngineStateMachine_t;

/* ================================================================
 * PACOTE DE TELEMETRIA BLE (40 bytes — little-endian)
 * ================================================================ */
typedef struct __attribute__((packed)) {
    uint16_t rpm;
    uint16_t tps_pct_x10;
    uint16_t map_kpa_x10;
    int16_t  coolant_c_x10;
    int16_t  iat_c_x10;
    uint16_t batt_mv;
    uint16_t o2_afr_x10;
    uint16_t knock_level;
    uint16_t vss_kmh;
    uint16_t boost_kpa_x10;
    int16_t  ign_adv_x10;
    uint16_t inj_pw1_us;
    uint16_t inj_pw2_us;
    uint16_t flex_pct_x10;
    uint16_t oil_kpa;
    uint16_t fuel_kpa;
    uint8_t  engine_state;
    uint8_t  limp_mode;
    uint16_t egt_c;
    uint32_t active_faults;
} BleTelemPacket_t;  /* sizeof = 40 */

/* ================================================================
 * PACOTE DE FALHAS BLE (8 bytes)
 * ================================================================ */
typedef struct __attribute__((packed)) {
    uint32_t active_faults;
    uint32_t stored_faults;
} BleFaultPacket_t;

/* ================================================================
 * PACOTE DE COMANDO BLE (64 bytes)
 * ================================================================ */
typedef struct __attribute__((packed)) {
    uint8_t  cmd;
    uint8_t  seq;
    uint8_t  len;
    uint8_t  payload[61];
} BleCmdPacket_t;

/* ================================================================
 * ACK DE COMANDO BLE (8 bytes)
 * ================================================================ */
typedef struct __attribute__((packed)) {
    uint8_t  cmd;
    uint8_t  seq;
    uint8_t  result;
    uint8_t  len;
    uint8_t  data[4];
} BleCmdAck_t;

/* ================================================================
 * CHUNK DE TABELA BLE (64 bytes)
 * ================================================================ */
typedef struct __attribute__((packed)) {
    uint8_t  chunk_id;
    uint8_t  total_chunks;
    uint8_t  map_type;       /* 0=VE, 1=IGN, 2=AFR, 3=CDI, 4=PROFILE */
    uint8_t  reserved;
    uint8_t  data[BLE_CHUNK_PAYLOAD_SIZE];
} BleChunk_t;

/* ================================================================
 * INFO DE FIRMWARE (resposta ao CMD_GET_FIRMWARE_INFO)
 * ================================================================ */
typedef struct __attribute__((packed)) {
    uint8_t  major;
    uint8_t  minor;
    uint8_t  patch;
    uint8_t  vehicle_type;
    uint8_t  cylinders;
    uint16_t displacement_cc;
    uint8_t  features_hi;
    uint8_t  features_lo;
    char     build_date[6];
} FirmwareInfo_t;

/* ================================================================
 * SENSOR RAW (para diagnóstico via BLE)
 * ================================================================ */
typedef struct __attribute__((packed)) {
    uint16_t tps_mv;
    uint16_t map_mv;
    uint16_t clt_mv;
    uint16_t iat_mv;
    uint16_t batt_mv;
    uint16_t o2_mv;
    uint16_t knock_mv;
    uint16_t flex_hz;
    uint16_t boost_mv;
    uint16_t oil_mv;
    uint16_t fuel_mv;
    uint16_t egt_raw;
    uint32_t ckp_period_us;
    uint32_t reserved;
} BleSensorRaw_t;

/* ================================================================
 * RESULTADO DE OPERAÇÕES
 * ================================================================ */
typedef enum {
    ECU_OK          = 0,
    ECU_ERROR       = 1,
    ECU_TIMEOUT     = 2,
    ECU_INVALID     = 3,
    ECU_BUSY        = 4,
    ECU_ERR_FLASH   = 5,
    ECU_ERR_PARAM   = 6,
    ECU_ERR_CAN     = 7,
    ECU_ERR_BLE     = 8,
} EcuResult_t;

/* ================================================================
 * MACROS UTILITÁRIOS
 * ================================================================ */
#define ECU_CLAMP(x, mn, mx)     ((x) < (mn) ? (mn) : ((x) > (mx) ? (mx) : (x)))
#define ECU_ABS(x)               ((x) < 0 ? -(x) : (x))
#define ECU_MIN(a, b)            ((a) < (b) ? (a) : (b))
#define ECU_MAX(a, b)            ((a) > (b) ? (a) : (b))
#define ECU_ARRAY_SIZE(arr)      (sizeof(arr) / sizeof((arr)[0]))
#define ECU_LERP(y0,y1,x0,x1,x) \
    ((int32_t)(y0) + ((int32_t)((y1)-(y0)) * (int32_t)((x)-(x0))) / (int32_t)((x1)-(x0)))

/* ================================================================
 * FEATURE FLAGS (campo EcuProfile_t.features)
 * ================================================================ */
#define FEAT_WIDEBAND_O2        (1U <<  0)
#define FEAT_KNOCK              (1U <<  1)
#define FEAT_FLEX               (1U <<  2)
#define FEAT_BOOST_CTRL         (1U <<  3)
#define FEAT_IAC                (1U <<  4)
#define FEAT_VSS                (1U <<  5)
#define FEAT_EGT                (1U <<  6)
#define FEAT_REV_LIMITER        (1U <<  7)
#define FEAT_LAUNCH_CTRL        (1U <<  8)
#define FEAT_TRACTION_CTRL      (1U <<  9)
#define FEAT_FLAT_SHIFT         (1U << 10)
#define FEAT_AUTOTUNE           (1U << 11)
#define FEAT_DATALOG_BLE        (1U << 12)
#define FEAT_CAN_DIAG           (1U << 13)
#define FEAT_CDI_MODE           (1U << 14)
#define FEAT_ANTILAG            (1U << 15)
#define FEAT_OIL_PRESSURE       (1U << 16)
#define FEAT_FUEL_PRESSURE      (1U << 17)

#endif /* ECU_TYPES_H */
