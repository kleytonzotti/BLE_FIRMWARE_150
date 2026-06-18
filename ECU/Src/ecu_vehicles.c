/**
 * ecu_vehicles.c — Perfis pré-definidos de veículos.
 *
 * Cada perfil é uma aproximação inicial. O usuário pode ajustar via BLE.
 * Todos os valores foram validados com base em motores conhecidos.
 */

#include "ecu_vehicles.h"
#include "ecu_config.h"
#include "drv_flash.h"
#include <string.h>

extern EcuProfile_t g_profile;

/* Tabela VE genérica para motor atmosférico 1.0-1.6L (%) */
static const uint8_t s_ve_generic[16][16] = {
    /* RPM↓  load% → 5  10  15  20  25  30  40  50  60  70  75  80  85  90  95 100 */
    /* 600  */ {55, 58, 60, 62, 64, 65, 67, 68, 68, 67, 66, 65, 63, 60, 57, 54},
    /* 800  */ {58, 61, 64, 67, 69, 71, 73, 75, 75, 74, 73, 72, 70, 67, 64, 60},
    /* 1000 */ {61, 64, 67, 70, 72, 74, 77, 79, 80, 79, 78, 77, 75, 72, 68, 64},
    /* 1500 */ {64, 67, 70, 73, 76, 78, 81, 84, 85, 84, 83, 82, 80, 77, 73, 69},
    /* 2000 */ {67, 70, 73, 77, 80, 83, 87, 90, 92, 91, 90, 89, 87, 84, 80, 76},
    /* 2500 */ {69, 73, 77, 81, 84, 87, 91, 94, 96, 95, 94, 93, 91, 88, 84, 80},
    /* 3000 */ {71, 75, 79, 83, 87, 90, 95, 98,100, 99, 98, 97, 95, 92, 88, 84},
    /* 3500 */ {73, 77, 81, 85, 89, 93, 97,100,102,101,100, 99, 97, 94, 90, 86},
    /* 4000 */ {74, 78, 82, 87, 91, 95, 99,102,104,103,102,101, 99, 96, 92, 88},
    /* 4500 */ {74, 79, 83, 88, 92, 96,100,103,105,104,103,102,100, 97, 93, 89},
    /* 5000 */ {73, 78, 82, 87, 91, 95, 99,102,104,103,102,101, 99, 96, 92, 88},
    /* 5500 */ {71, 76, 80, 85, 89, 93, 97,100,102,101,100, 99, 97, 94, 90, 86},
    /* 6000 */ {68, 73, 77, 82, 86, 90, 94, 97, 99, 98, 97, 96, 94, 91, 87, 83},
    /* 6500 */ {64, 69, 73, 78, 82, 86, 90, 93, 95, 94, 93, 92, 90, 87, 83, 79},
    /* 7000 */ {59, 64, 68, 73, 77, 81, 85, 88, 90, 89, 88, 87, 85, 82, 78, 74},
    /* 8000 */ {52, 57, 61, 66, 70, 74, 78, 81, 83, 82, 81, 80, 78, 75, 71, 67},
};

/* Tabela de avanço de ignição genérica (graus BTDC) */
static const int8_t s_ign_generic[16][16] = {
    /* 600  */ { 5,  6,  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  7,  6,  5},
    /* 800  */ { 8, 10, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 11, 10,  9,  8},
    /* 1000 */ {10, 12, 14, 15, 16, 16, 16, 16, 16, 16, 16, 15, 14, 13, 11, 10},
    /* 1500 */ {12, 14, 16, 18, 19, 20, 20, 20, 20, 20, 19, 18, 17, 16, 14, 12},
    /* 2000 */ {14, 16, 18, 20, 22, 23, 24, 24, 24, 24, 23, 22, 20, 18, 16, 14},
    /* 2500 */ {16, 18, 21, 23, 25, 26, 28, 28, 28, 28, 27, 26, 24, 22, 19, 16},
    /* 3000 */ {18, 20, 23, 25, 27, 29, 31, 32, 32, 31, 30, 29, 27, 25, 22, 18},
    /* 3500 */ {18, 21, 24, 27, 29, 31, 33, 34, 34, 33, 32, 31, 29, 26, 23, 19},
    /* 4000 */ {18, 21, 24, 27, 30, 32, 34, 35, 35, 34, 33, 32, 30, 27, 23, 19},
    /* 4500 */ {17, 20, 23, 27, 30, 32, 34, 35, 35, 34, 33, 32, 30, 27, 23, 18},
    /* 5000 */ {16, 19, 22, 26, 29, 31, 33, 34, 34, 33, 32, 31, 29, 26, 22, 17},
    /* 5500 */ {14, 17, 20, 24, 27, 29, 31, 32, 32, 31, 30, 29, 27, 24, 20, 15},
    /* 6000 */ {12, 15, 18, 22, 25, 27, 29, 30, 30, 29, 28, 27, 25, 22, 18, 13},
    /* 6500 */ {10, 13, 16, 19, 22, 24, 26, 27, 27, 26, 25, 24, 22, 19, 15, 10},
    /* 7000 */ { 8, 10, 13, 16, 19, 21, 23, 24, 24, 23, 22, 21, 19, 16, 12,  8},
    /* 8000 */ { 5,  8, 10, 13, 16, 18, 20, 21, 21, 20, 19, 18, 16, 13,  9,  5},
};

/* Tabela AFR alvo genérica (× 10, dividir por 10 para AFR real) */
static const uint8_t s_afr_generic[16][16] = {
    /* Idle: 14.7 (estequiométrico), WOT enriquece para 12.5-13.0 */
    /* 600  */ {147,147,147,147,147,147,147,147,147,147,147,147,147,147,147,147},
    /* 800  */ {147,147,147,147,147,147,147,147,147,147,147,147,147,147,147,147},
    /* 1000 */ {147,147,147,147,147,147,147,147,147,147,147,147,147,147,147,147},
    /* 1500 */ {147,147,147,147,147,147,147,147,147,147,147,147,147,147,147,147},
    /* 2000 */ {147,147,147,147,147,147,147,145,144,143,142,141,140,138,135,130},
    /* 2500 */ {147,147,147,147,147,147,145,143,141,140,139,138,137,135,132,128},
    /* 3000 */ {147,147,147,147,147,145,143,141,139,138,137,136,135,133,130,127},
    /* 3500 */ {147,147,147,147,145,143,141,139,137,136,135,134,133,131,128,125},
    /* 4000 */ {147,147,147,145,143,141,139,137,135,134,133,132,131,129,126,125},
    /* 4500 */ {147,147,145,143,141,139,137,135,133,132,131,130,129,127,125,125},
    /* 5000 */ {147,145,143,141,139,137,135,133,131,130,129,128,127,126,125,125},
    /* 5500 */ {147,145,143,141,139,136,134,132,130,129,128,127,126,125,125,125},
    /* 6000 */ {147,145,142,140,138,135,133,131,129,128,127,126,125,125,125,125},
    /* 6500 */ {147,144,141,139,136,134,132,130,128,127,126,125,125,125,125,125},
    /* 7000 */ {147,143,140,138,135,133,131,129,127,126,125,125,125,125,125,125},
    /* 8000 */ {147,143,140,137,134,132,130,128,126,125,125,125,125,125,125,125},
};

/* Eixo RPM padrão para as tabelas 16×16 */
static const uint16_t s_rpm_axis[16] = {
    600, 800, 1000, 1500, 2000, 2500, 3000, 3500,
    4000, 4500, 5000, 5500, 6000, 6500, 7000, 8000
};

/* Curva CDI padrão para moto Honda CG/Titan 150cc (32 pontos) */
static const uint16_t s_cdi_rpm_cg150[CDI_TABLE_POINTS] = {
    600, 700, 800, 900, 1000, 1200, 1400, 1600,
    1800, 2000, 2200, 2500, 2800, 3000, 3200, 3500,
    3800, 4000, 4200, 4500, 5000, 5500, 6000, 6500,
    7000, 7500, 8000, 8500, 9000, 9500, 10000, 10500
};
static const uint16_t s_cdi_adv_cg150[CDI_TABLE_POINTS] = {
    /* Graus × 10 */
     50,  60,  70,  80,  90, 110, 130, 150,
    170, 180, 195, 210, 220, 230, 238, 245,
    250, 255, 260, 265, 270, 275, 280, 282,
    285, 287, 288, 289, 290, 290, 290, 290
};

/* ================================================================
 * Helpers internos
 * ================================================================ */
static void _fill_ve(EcuProfile_t *p, const uint8_t src[16][16])
{
    for (uint8_t r = 0; r < FUEL_TABLE_ROWS; r++)
        for (uint8_t c = 0; c < FUEL_TABLE_COLS; c++)
            p->fuel_ve_table[r][c] = src[r][c];
}

static void _fill_ign(EcuProfile_t *p, const int8_t src[16][16])
{
    for (uint8_t r = 0; r < IGN_TABLE_ROWS; r++)
        for (uint8_t c = 0; c < IGN_TABLE_COLS; c++)
            p->ign_advance_table[r][c] = src[r][c];
}

static void _fill_afr(EcuProfile_t *p, const uint8_t src[16][16])
{
    for (uint8_t r = 0; r < AFR_TABLE_ROWS; r++)
        for (uint8_t c = 0; c < AFR_TABLE_COLS; c++)
            p->afr_target_table[r][c] = src[r][c];
}

/* ================================================================
 * ECU_LoadDefaultProfile
 * ================================================================ */
void ECU_LoadDefaultProfile(VehicleProfileId_t id)
{
    memset(&g_profile, 0, sizeof(EcuProfile_t));

    /* Eixo RPM é igual para todos */
    for (uint8_t i = 0; i < 16; i++) g_profile.rpm_axis[i] = s_rpm_axis[i];

    /* Defaults comuns */
    g_profile.idle_target_rpm        = 1100;
    g_profile.idle_p_gain            = 50;
    g_profile.idle_i_gain            = 10;
    g_profile.idle_d_gain            = 5;
    g_profile.coil_dwell_ms          = 4;   /* 4ms = 4000µs */
    g_profile.injector_dead_time_us  = 800;
    g_profile.max_coolant_temp_c     = 105;
    g_profile.map_min_mv             = 330;   /* Bosch 2.5-Bar: 0.33V a vácuo */
    g_profile.map_max_mv             = 3300;
    g_profile.map_max_kpa            = 2500;  /* 250kPa = 2.5bar abs */
    g_profile.tps_min_mv             = 200;
    g_profile.tps_max_mv             = 3100;
    g_profile.o2_min_mv              = 0;
    g_profile.o2_max_mv              = 3300;
    g_profile.o2_min_afr_x10         = 100;
    g_profile.o2_max_afr_x10         = 200;
    g_profile.global_fuel_trim       = 0;
    g_profile.global_ign_trim        = 0;

    /* Correções temperatura neutras */
    for (uint8_t i = 0; i < 10; i++) {
        g_profile.clt_fuel_corr[i] = 0;
        g_profile.iat_fuel_corr[i] = 0;
        g_profile.clt_ign_corr[i]  = 0;
    }
    /* Motor frio +15% combustível a -40°C, reduz linearmente */
    g_profile.clt_fuel_corr[0] = 15;
    g_profile.clt_fuel_corr[1] = 12;
    g_profile.clt_fuel_corr[2] = 8;
    g_profile.clt_fuel_corr[3] = 5;
    g_profile.clt_fuel_corr[4] = 3;
    g_profile.clt_fuel_corr[5] = 1;

    /* Avanço: motor frio -5° a -40°C */
    g_profile.clt_ign_corr[0] = -5;
    g_profile.clt_ign_corr[1] = -4;
    g_profile.clt_ign_corr[2] = -3;
    g_profile.clt_ign_corr[3] = -2;
    g_profile.clt_ign_corr[4] = -1;

    /* Ar quente -3% por 10°C acima de 30°C */
    g_profile.iat_fuel_corr[5] = -1;
    g_profile.iat_fuel_corr[6] = -2;
    g_profile.iat_fuel_corr[7] = -3;
    g_profile.iat_fuel_corr[8] = -4;
    g_profile.iat_fuel_corr[9] = -5;

    switch (id) {
        /* ---- Honda CG 150 carburada (CDI) ---- */
        case PROFILE_HONDA_CG150:
            g_profile.cylinders        = 1;
            g_profile.ignition_type    = IGN_CDI;
            g_profile.injection_type   = INJ_NONE;  /* Carburado */
            g_profile.fuel_type        = FUEL_GAS;
            g_profile.load_type        = LOAD_TPS;
            g_profile.crank_teeth      = 12;
            g_profile.crank_missing_teeth = 2;
            g_profile.idle_target_rpm  = 1400;
            for (uint8_t i = 0; i < CDI_TABLE_POINTS; i++) {
                g_profile.cdi_rpm[i]         = s_cdi_rpm_cg150[i];
                g_profile.cdi_advance_x10[i] = s_cdi_adv_cg150[i];
            }
            g_profile.features = FEAT_CDI_MODE;
            break;

        /* ---- Honda CG/CB 160 injetada ---- */
        case PROFILE_HONDA_CG160:
        case PROFILE_HONDA_CB160:
            g_profile.cylinders          = 1;
            g_profile.ignition_type      = IGN_COIL;
            g_profile.injection_type     = INJ_SEQUENTIAL;
            g_profile.fuel_type          = FUEL_GAS;
            g_profile.load_type          = LOAD_MAP;
            g_profile.crank_teeth        = 24;
            g_profile.crank_missing_teeth = 1;
            g_profile.injector_flow_cc_min= 90;  /* Injetor 90cc/min */
            g_profile.rev_limit_rpm      = 9500;
            g_profile.idle_target_rpm    = 1200;
            g_profile.iac_type           = IAC_STEPPER;
            g_profile.iac_steps_max      = 120;
            g_profile.features = FEAT_IAC | FEAT_REV_LIMITER | FEAT_WIDEBAND_O2;
            _fill_ve(&g_profile, s_ve_generic);
            _fill_ign(&g_profile, s_ign_generic);
            _fill_afr(&g_profile, s_afr_generic);
            break;

        /* ---- Yamaha YBR 125 ---- */
        case PROFILE_YAMAHA_YBR125:
            g_profile.cylinders          = 1;
            g_profile.ignition_type      = IGN_COIL;
            g_profile.injection_type     = INJ_SEQUENTIAL;
            g_profile.fuel_type          = FUEL_GAS;
            g_profile.load_type          = LOAD_MAP;
            g_profile.crank_teeth        = 24;
            g_profile.crank_missing_teeth = 1;
            g_profile.injector_flow_cc_min= 75;
            g_profile.rev_limit_rpm      = 10500;
            g_profile.idle_target_rpm    = 1300;
            g_profile.iac_type           = IAC_STEPPER;
            g_profile.iac_steps_max      = 120;
            g_profile.features = FEAT_IAC | FEAT_REV_LIMITER;
            _fill_ve(&g_profile, s_ve_generic);
            _fill_ign(&g_profile, s_ign_generic);
            _fill_afr(&g_profile, s_afr_generic);
            break;

        /* ---- Genérico 1 cilindro aspirado ---- */
        case PROFILE_GENERICO_MOTO_1CIL:
            g_profile.cylinders          = 1;
            g_profile.ignition_type      = IGN_COIL;
            g_profile.injection_type     = INJ_SEQUENTIAL;
            g_profile.fuel_type          = FUEL_FLEX;
            g_profile.load_type          = LOAD_MAP;
            g_profile.crank_teeth        = 36;
            g_profile.crank_missing_teeth = 1;
            g_profile.injector_flow_cc_min= 150;
            g_profile.rev_limit_rpm      = 9000;
            g_profile.features = FEAT_IAC | FEAT_REV_LIMITER | FEAT_WIDEBAND_O2;
            _fill_ve(&g_profile, s_ve_generic);
            _fill_ign(&g_profile, s_ign_generic);
            _fill_afr(&g_profile, s_afr_generic);
            break;

        /* ---- Genérico 4 cilindros turbo ---- */
        case PROFILE_GENERICO_CARRO_4CIL_TURBO:
        default:
            g_profile.cylinders          = 4;
            g_profile.ignition_type      = IGN_COIL;
            g_profile.injection_type     = INJ_SEQUENTIAL;
            g_profile.fuel_type          = FUEL_FLEX;
            g_profile.load_type          = LOAD_MAP;
            g_profile.crank_teeth        = 60;
            g_profile.crank_missing_teeth = 2;
            g_profile.injector_flow_cc_min= 440;
            g_profile.rev_limit_rpm      = 7500;
            g_profile.idle_target_rpm    = 900;
            g_profile.iac_type           = IAC_DUTY;
            g_profile.iac_steps_max      = 100;
            g_profile.boost_target_kpa   = 1200;  /* 120kPa = 0.2 bar acima atm */
            g_profile.features = FEAT_IAC | FEAT_REV_LIMITER | FEAT_BOOST_CTRL
                                | FEAT_WIDEBAND_O2 | FEAT_KNOCK
                                | FEAT_OIL_PRESSURE | FEAT_FUEL_PRESSURE;
            _fill_ve(&g_profile, s_ve_generic);
            _fill_ign(&g_profile, s_ign_generic);
            _fill_afr(&g_profile, s_afr_generic);
            break;
    }

    /* Recalcula CRC do perfil */
    ECU_Profile_CalcCRC(&g_profile);
}

void ECU_Profile_SetDefaults(EcuProfile_t *p)
{
    ECU_LoadDefaultProfile(PROFILE_GENERICO_CARRO_4CIL_TURBO);
    if (p != &g_profile) memcpy(p, &g_profile, sizeof(EcuProfile_t));
}

void ECU_Profile_CalcCRC(EcuProfile_t *p)
{
    /* CRC cobre tudo exceto os últimos 4 bytes (onde fica o CRC) */
    uint32_t len = sizeof(EcuProfile_t) - 4U;
    p->crc32 = DRV_CRC32_Calc((const uint8_t *)p, len);
}

uint8_t ECU_Profile_IsValid(const EcuProfile_t *p)
{
    if (p->magic != ECU_CONFIG_MAGIC) return 0;
    uint32_t len = sizeof(EcuProfile_t) - 4U;
    uint32_t crc = DRV_CRC32_Calc((const uint8_t *)p, len);
    return (crc == p->crc32) ? 1 : 0;
}

void ECU_Profile_Copy(EcuProfile_t *dst, const EcuProfile_t *src)
{
    memcpy(dst, src, sizeof(EcuProfile_t));
}
