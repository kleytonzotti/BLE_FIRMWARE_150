/**
 * ecu_sim.c — Simulador de cenários ECU para STM32WB5MM-DK.
 *
 * Fluxo:
 *   1. ECU_Sim_Init() — configura botões, inicializa OLED, exibe cenário 0
 *   2. ECU_Sim_Task_10ms() — chamado no main loop:
 *        a) Lê botões com debounce (3 × 10ms = 30ms)
 *        b) Injeta valores simulados em g_engine
 *        c) Atualiza OLED a cada 200ms (ou imediatamente após troca de cenário)
 *
 * Os valores de g_engine são sobrescritos neste módulo.
 * A ECU pode usar esses valores normalmente (BLE telemetria, etc.).
 */

#include "ecu_sim.h"
#include "ecu_engine.h"
#include "ecu_fault.h"
#include "drv_oled.h"
#include "pinout.h"
#include "ecu_config.h"

#include <string.h>  /* memset */

/* ================================================================
 * Formatação de inteiros sem stdio (evita dependência de libc printf)
 * ================================================================ */

/* Escreve val decimal (sem sinal) em buf[0..width-1], alinhado à direita,
 * preenchido com spaces. Não adiciona '\0' — retorna ponteiro após o campo. */
static char *_puti(char *p, uint32_t val, uint8_t width)
{
    char tmp[6]; uint8_t n = 0;
    if (val == 0U) { tmp[n++] = '0'; }
    else { while (val > 0U) { tmp[n++] = (char)('0' + val % 10U); val /= 10U; } }
    while (n < width) { *p++ = ' '; width--; }  /* padding esquerdo */
    while (n > 0U)    { *p++ = tmp[--n]; }
    return p;
}

/* Escreve val decimal com sinal */
static char *_puti_signed(char *p, int32_t val, uint8_t width)
{
    uint8_t neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    /* reserva 1 char para sinal */
    char tmp[6]; uint8_t n = 0;
    if (val == 0) { tmp[n++] = '0'; }
    else { while (val > 0) { tmp[n++] = (char)('0' + val % 10); val /= 10; } }
    if (neg) tmp[n++] = '-';
    while ((n + (neg ? 0 : 0)) < width) { *p++ = ' '; width--; }
    while (n > 0U) { *p++ = tmp[--n]; }
    return p;
}

/* Escreve string src em p, truncando/preenchendo até width caracteres */
static char *_puts_w(char *p, const char *src, uint8_t width)
{
    uint8_t n = 0;
    while (*src && n < width) { *p++ = *src++; n++; }
    while (n < width)         { *p++ = ' '; n++; }
    return p;
}

/* Escreve literal string */
static char *_puts(char *p, const char *src)
{
    while (*src) *p++ = *src++;
    return p;
}

/* ================================================================
 * Debounce de botão
 * ================================================================ */
typedef struct {
    uint8_t count;   /* contador de leituras consecutivas pressionado */
    uint8_t active;  /* 1 = evento de pressão disponível para consumo */
} BtnCtx_t;

static BtnCtx_t s_btn_next = {0, 0};
static BtnCtx_t s_btn_prev = {0, 0};

/* Chame a cada 10ms. Gera evento no soltar após ≥3 leituras pressionado. */
static void _btn_update(BtnCtx_t *b, uint8_t raw_pressed)
{
    if (raw_pressed) {
        if (b->count < 255U) b->count++;
    } else {
        if (b->count >= 3U && !b->active) {
            b->active = 1U;
        }
        b->count = 0U;
    }
}

/* Retorna 1 e limpa o evento se houver pressão disponível */
static uint8_t _btn_consume(BtnCtx_t *b)
{
    if (b->active) { b->active = 0U; return 1U; }
    return 0U;
}

/* ================================================================
 * Estrutura dos cenários de simulação
 * ================================================================ */
typedef struct {
    const char  *name;          /* Nome curto ≤12 chars */
    uint16_t     rpm;
    uint16_t     tps_pct_x10;  /* 0-1000 (0.0-100.0%) */
    uint16_t     map_kpa_x10;  /* kPa × 10 */
    int16_t      coolant_c_x10;/* °C × 10 */
    int16_t      iat_c_x10;    /* °C × 10 */
    uint16_t     batt_mv;      /* mV */
    uint16_t     afr_x10;      /* AFR × 10 (ex: 147 = 14.7) */
    uint16_t     knock_level;  /* 0-1000 */
    int16_t      ign_adv_x10;  /* graus BTDC × 10 */
    uint32_t     faults;       /* bitmask EcuFaultCode_t */
    EngineStateMachine_t state;
    uint8_t      cdi_mode;     /* 1 = CDI carburado */
    const char  *info;         /* Texto linha 5 ≤20 chars */
} SimScenario_t;

static const SimScenario_t s_scenarios[SIM_COUNT] = {
    /* 0 — Marcha lenta quente */
    { "MARCHA LENTA",
      800, 0, 300, 900, 350, 13800, 147, 0, 120,
      0U, ENGINE_IDLE, 0, "Operacao normal" },

    /* 1 — Aceleração parcial */
    { "ACELERACAO",
      3500, 550, 750, 900, 400, 14000, 135, 0, 280,
      0U, ENGINE_RUNNING, 0, "50% throttle" },

    /* 2 — Carga plena */
    { "CARGA PLENA",
      6000, 1000, 1000, 950, 450, 13600, 125, 50, 320,
      0U, ENGINE_RUNNING, 0, "WOT 6000rpm" },

    /* 3 — Partida a frio */
    { "PARTIDA FRIA",
      300, 50, 250, 150, 180, 11800, 110, 0, 50,
      0U, ENGINE_CRANKING, 0, "Cold crank 15C" },

    /* 4 — Superaquecimento */
    { "SUPERAQUEC.",
      900, 0, 300, 1180, 500, 13500, 147, 0, 120,
      (uint32_t)FAULT_OVERTEMP, ENGINE_LIMP, 0, "P0217 TEMP ALTA!" },

    /* 5 — CDI moto carburada */
    { "CDI MOTO",
      4000, 0, 0, 800, 380, 13800, 0, 0, 280,
      0U, ENGINE_CDI_RUN, 1, "CDI 28 BTDC" },

    /* 6 — Detonação */
    { "DETONACAO",
      5000, 850, 950, 930, 420, 13700, 140, 850, 180,
      (uint32_t)FAULT_KNOCK_SEVERE, ENGINE_RUNNING, 0, "P0324 IGN-8deg" },

    /* 7 — Corte de combustível */
    { "CORTE RPM",
      11000, 1000, 1000, 960, 450, 13800, 130, 0, 300,
      (uint32_t)FAULT_REV_LIMIT, ENGINE_REV_LIMIT, 0, "REV LIMIT 11000!" },
};

/* ================================================================
 * Estado da simulação
 * ================================================================ */
static SimMode_t s_mode          = SIM_IDLE;
static uint8_t   s_display_tick  = 0U;
static uint8_t   s_dirty         = 1U;  /* força update inicial */

/* ================================================================
 * Nomes de estados do motor (indexados por EngineStateMachine_t)
 * ================================================================ */
static const char *const s_state_names[] = {
    "DESLIGADO",   /* 0  ENGINE_OFF       */
    "PRIMING",     /* 1  ENGINE_PRIMING   */
    "PARTIDA",     /* 2  ENGINE_CRANKING  */
    "RODANDO",     /* 3  ENGINE_RUNNING   */
    "MARCHA LENTA",/* 4  ENGINE_IDLE      */
    "DECEL",       /* 5  ENGINE_DECEL     */
    "LIM RPM",     /* 6  ENGINE_REV_LIMIT */
    "LIMP MODE",   /* 7  ENGINE_LIMP      */
    "FALHA GRAVE", /* 8  ENGINE_FAULT     */
    "CDI PART.",   /* 9  ENGINE_CDI_CRANK */
    "CDI RODANDO", /* 10 ENGINE_CDI_RUN   */
};
#define STATE_NAMES_COUNT  ((uint8_t)(sizeof(s_state_names)/sizeof(s_state_names[0])))

/* ================================================================
 * Renderização do display (atualiza framebuffer e faz flush)
 *
 * Layout 128×64 com fonte 6×8 (21 chars × 8 linhas):
 *
 *   Row 0  "SIM X/8: NOME_______"  cenário
 *   Row 1  "RPM:XXXXX  TPS: XX%"   rotação e acelerador
 *   Row 2  "MAP: XXkPa CLT:XXXC"   pressão e temperatura
 *   Row 3  "AFR:XX.X  BAT:XX.XV"   lambda e bateria
 *   Row 4  "ESTADO: XXXXXXXXXX "   estado da ECU
 *   Row 5  "INFO: XXXXXXXXXXXXX"   info / alerta
 *   Row 6  "[SW1] PROXIMO     "   dica de botão
 *   Row 7  "[SW2] ANTERIOR    "   dica de botão
 * ================================================================ */
static void _render_display(void)
{
    const SimScenario_t *sc = &s_scenarios[s_mode];
    char line[22];  /* 21 chars + '\0' */
    char *p;

    DRV_OLED_Clear();

    /* ---- Row 0: cenário ---------------------------------------- */
    p = line;
    p = _puts(p, "SIM ");
    p = _puti(p, (uint32_t)(s_mode + 1U), 1U);
    p = _puts(p, "/8: ");
    p = _puts_w(p, sc->name, 8U);
    *p = '\0';
    DRV_OLED_Print(0, 0, line);

    /* ---- Row 1: RPM e TPS -------------------------------------- */
    p = line;
    p = _puts(p, "RPM:");
    p = _puti(p, sc->rpm, 5U);
    p = _puts(p, " TPS:");
    p = _puti(p, (uint32_t)(sc->tps_pct_x10 / 10U), 3U);
    p = _puts(p, "%");
    *p = '\0';
    DRV_OLED_Print(0, 1, line);

    /* ---- Row 2: MAP e CLT -------------------------------------- */
    p = line;
    p = _puts(p, "MAP:");
    p = _puti(p, (uint32_t)(sc->map_kpa_x10 / 10U), 3U);
    p = _puts(p, "kPa CLT:");
    p = _puti_signed(p, (int32_t)(sc->coolant_c_x10 / 10), 3);
    p = _puts(p, "C");
    *p = '\0';
    DRV_OLED_Print(0, 2, line);

    /* ---- Row 3: AFR / CDI e bateria ---------------------------- */
    p = line;
    if (sc->cdi_mode) {
        p = _puts(p, "CDI:");
        p = _puti(p, (uint32_t)(sc->ign_adv_x10 / 10), 2U);
        p = _puts(p, "BTDC");
        p = _puts_w(p, "", 8U);
    } else {
        uint32_t afr_i = (uint32_t)(sc->afr_x10 / 10U);
        uint32_t afr_d = (uint32_t)(sc->afr_x10 % 10U);
        p = _puts(p, "AFR:");
        p = _puti(p, afr_i, 2U);
        *p++ = '.'; p = _puti(p, afr_d, 1U);
    }
    {
        uint32_t bat_i = sc->batt_mv / 1000U;
        uint32_t bat_d = (sc->batt_mv % 1000U) / 100U;
        p = _puts(p, " BAT:");
        p = _puti(p, bat_i, 2U);
        *p++ = '.'; p = _puti(p, bat_d, 1U);
        *p++ = 'V';
    }
    *p = '\0';
    DRV_OLED_Print(0, 3, line);

    /* ---- Row 4: estado ----------------------------------------- */
    p = line;
    p = _puts(p, "ESTADO:");
    uint8_t st = (uint8_t)sc->state;
    const char *sname = (st < STATE_NAMES_COUNT) ? s_state_names[st] : "???";
    p = _puts_w(p, sname, 13U);
    *p = '\0';
    DRV_OLED_Print(0, 4, line);

    /* ---- Row 5: alerta ou info --------------------------------- */
    p = line;
    if (sc->faults != 0U) {
        p = _puts_w(p, sc->info, 20U);
    } else {
        p = _puts(p, "INFO: ");
        p = _puts_w(p, sc->info, 14U);
    }
    *p = '\0';
    DRV_OLED_Print(0, 5, line);
    /* Inverte a linha de info quando há falha para destacar */
    if (sc->faults != 0U) {
        DRV_OLED_InvertRow(5);
    }

    /* ---- Row 6 e 7: dicas de botão ----------------------------- */
    DRV_OLED_Print(0, 6, "[SW1] PROXIMO       ");
    DRV_OLED_Print(0, 7, "[SW2] ANTERIOR      ");

    DRV_OLED_Flush();
}

/* ================================================================
 * Aplica valores do cenário em g_engine
 * ================================================================ */
static void _apply_scenario(void)
{
    const SimScenario_t *sc = &s_scenarios[s_mode];

    g_engine.rpm              = sc->rpm;
    g_engine.rpm_filtered     = sc->rpm;
    g_engine.tps_pct          = sc->tps_pct_x10;
    g_engine.map_kpa          = sc->map_kpa_x10;
    g_engine.coolant_c        = sc->coolant_c_x10;
    g_engine.iat_c            = sc->iat_c_x10;
    g_engine.batt_mv          = sc->batt_mv;
    g_engine.o2_afr_x10       = sc->afr_x10;
    g_engine.knock_level      = sc->knock_level;
    g_engine.ign_advance_deg  = sc->ign_adv_x10;
    g_engine.active_faults    = sc->faults;
    g_engine.limp_mode        = (sc->faults != 0U) ? 1U : 0U;
    g_engine.cdi_advance_deg  = sc->cdi_mode ? (uint16_t)(sc->ign_adv_x10) : 0U;
    g_engine.cdi_capacitor_ok = sc->cdi_mode ? 1U : 0U;

    ECU_Engine_SetState(sc->state);
}

/* ================================================================
 * API pública
 * ================================================================ */
void ECU_Sim_Init(void)
{
    /* Configura GPIOE para o botão SW2 (PE4) */
    volatile uint32_t *RCC_AHB2ENR = (volatile uint32_t *)0x4002104CUL;
    *RCC_AHB2ENR |= (1U << 2U) | (1U << 4U);  /* GPIOC + GPIOE */

    /* PC4 = BTN_NEXT: entrada com pull-up */
    BTN_NEXT_PORT->MODER  &= ~(3U << (BTN_NEXT_PIN * 2U));  /* Input */
    BTN_NEXT_PORT->PUPDR  &= ~(3U << (BTN_NEXT_PIN * 2U));
    BTN_NEXT_PORT->PUPDR  |=  (1U << (BTN_NEXT_PIN * 2U));  /* Pull-up */

    /* PE4 = BTN_PREV: entrada com pull-up */
    BTN_PREV_PORT->MODER  &= ~(3U << (BTN_PREV_PIN * 2U));  /* Input */
    BTN_PREV_PORT->PUPDR  &= ~(3U << (BTN_PREV_PIN * 2U));
    BTN_PREV_PORT->PUPDR  |=  (1U << (BTN_PREV_PIN * 2U));  /* Pull-up */

    /* Inicializa OLED */
    DRV_OLED_Init();

    s_mode         = SIM_IDLE;
    s_display_tick = 0U;
    s_dirty        = 1U;
}

void ECU_Sim_NextMode(void)
{
    s_mode  = (SimMode_t)((uint8_t)(s_mode + 1U) % (uint8_t)SIM_COUNT);
    s_dirty = 1U;
}

void ECU_Sim_PrevMode(void)
{
    s_mode  = (s_mode == SIM_IDLE) ? (SimMode_t)(SIM_COUNT - 1U)
                                    : (SimMode_t)(s_mode - 1U);
    s_dirty = 1U;
}

SimMode_t ECU_Sim_GetMode(void)
{
    return s_mode;
}

void ECU_Sim_Task_10ms(void)
{
    /* 1. Atualiza debounce dos botões */
    _btn_update(&s_btn_next, BTN_NEXT_PRESSED() ? 1U : 0U);
    _btn_update(&s_btn_prev, BTN_PREV_PRESSED() ? 1U : 0U);

    /* 2. Processa eventos */
    if (_btn_consume(&s_btn_next)) {
        ECU_Sim_NextMode();
        /* LED verde pisca para feedback visual */
        LED_GREEN_ON();
    } else {
        LED_GREEN_OFF();
    }

    if (_btn_consume(&s_btn_prev)) {
        ECU_Sim_PrevMode();
        /* LED azul pisca para feedback visual */
        LED_BLUE_ON();
    } else {
        LED_BLUE_OFF();
    }

    /* LED vermelho acende quando há falha ativa */
    if (s_scenarios[s_mode].faults != 0U) {
        LED_RED_ON();
    } else {
        LED_RED_OFF();
    }

    /* 3. Injeta valores simulados em g_engine */
    _apply_scenario();

    /* 4. Atualiza display a cada 200ms ou imediatamente se houve troca */
    if (++s_display_tick >= 20U || s_dirty) {
        s_display_tick = 0U;
        s_dirty        = 0U;
        _render_display();
    }
}
