/**
 * drv_adc.c — ADC1 com DMA circular, oversample 16×.
 *
 * Sequência de 12 canais em modo contínuo + DMA.
 * Cada amostra é a média de 16 leituras (oversample por software).
 *
 * Registradores-chave:
 *   ADC1->SQR1-SQR4 — ordem dos canais
 *   ADC1->CFGR      — DMA, oversample, modo contínuo
 *   DMA1->CCR1      — circular, mem incrementa, half-word
 */

#include "drv_adc.h"
#include "ecu_config.h"

/* Buffer DMA: 12 canais × 16 amostras (sobresampling manual) */
#define OVERSAMPLE_N  16U
volatile uint16_t g_adc_dma_buf[ADC_CHANNELS * OVERSAMPLE_N];

/* Buffer público de médias */
volatile uint16_t g_adc_avg[ADC_CHANNELS];

/* ----------------------------------------------------------------
 * Endereços de registrador direto
 * ---------------------------------------------------------------- */
#define RCC_AHB2ENR   (*((volatile uint32_t *)0x4002104CUL))
#define RCC_AHB1ENR   (*((volatile uint32_t *)0x40021048UL))

#define ADC1_BASE     0x50040000UL
#define DMA1_BASE     0x40020000UL
#define DMAMUX1_BASE  0x40020800UL

typedef struct {
    volatile uint32_t ISR, IER, CR, CFGR, CFGR2, SMPR1, SMPR2, _r1;
    volatile uint32_t TR1, TR2, TR3, _r2;
    volatile uint32_t SQR1, SQR2, SQR3, SQR4;
    volatile uint32_t DR;
    volatile uint32_t _r3[2];
    volatile uint32_t JSQR;
    volatile uint32_t _r4[4];
    volatile uint32_t OFR1, OFR2, OFR3, OFR4;
    volatile uint32_t _r5[4];
    volatile uint32_t JDR1, JDR2, JDR3, JDR4;
    volatile uint32_t _r6[4];
    volatile uint32_t AWD2CR, AWD3CR;
    volatile uint32_t _r7[2];
    volatile uint32_t DIFSEL, CALFACT;
} ADC_Regs_t;

typedef struct {
    volatile uint32_t CCR, CMAR, CPAR, CNDTR;
} DMA_Ch_t;

typedef struct {
    volatile uint32_t ISR, IFCR;
    DMA_Ch_t CH[8];
} DMA_Regs_t;

#define ADC1    ((ADC_Regs_t *)ADC1_BASE)
#define DMA1    ((DMA_Regs_t *)DMA1_BASE)

/* Tabela de mapeamento canal ADC → PA/PB/PC (STM32WB55) */
/* Canais: TPS=PA0(5), MAP=PA1(6), CLT=PA2(7), IAT=PA3(8),
           BATT=PA4(9), O2=PA5(10), BOOST=PA6(11), OIL=PA7(12),
           FUEL_PRESS=PB0(15), FLEX=PC0(1), KNOCK2=PC1(2), SPARE=PC2(3) */
static const uint8_t s_adc_ch[ADC_CHANNELS] = {
    5, 6, 7, 8, 9, 10, 11, 12, 15, 1, 2, 3
};

void DRV_ADC_Init(void)
{
    /* Clocks: ADC e DMA */
    RCC_AHB2ENR |= (1U << 13);  /* ADCEN */
    RCC_AHB1ENR |= (1U << 0);   /* DMA1EN */

    /* Calibração ADC */
    ADC1->CR = (1U << 28);  /* ADVREGEN = voltage regulator enable */
    /* Aguarda estabilização (~20µs) — loop simples */
    for (volatile uint32_t i = 0; i < 2000; i++) __asm volatile("nop");

    ADC1->CR |= (1U << 31);  /* ADCAL */
    while (ADC1->CR & (1U << 31));  /* Aguarda calibração */

    /* Habilita ADC */
    ADC1->ISR = (1U << 0);   /* Limpa ADRDY */
    ADC1->CR |= (1U << 0);   /* ADEN */
    while (!(ADC1->ISR & (1U << 0)));  /* ADRDY */

    /* Configura amostras: 47.5 ciclos por canal (SMPx=100) */
    uint32_t smp = 0;
    for (uint8_t i = 0; i < 8; i++) smp |= (4U << (i * 3));
    ADC1->SMPR1 = smp;
    ADC1->SMPR2 = smp;

    /* Sequência: 12 canais */
    ADC1->SQR1 = ((ADC_CHANNELS - 1) << 0)           /* L=11 (12 conv) */
               | ((uint32_t)s_adc_ch[0]  << 6)
               | ((uint32_t)s_adc_ch[1]  << 12)
               | ((uint32_t)s_adc_ch[2]  << 18)
               | ((uint32_t)s_adc_ch[3]  << 24);
    ADC1->SQR2 = ((uint32_t)s_adc_ch[4]  << 0)
               | ((uint32_t)s_adc_ch[5]  << 6)
               | ((uint32_t)s_adc_ch[6]  << 12)
               | ((uint32_t)s_adc_ch[7]  << 18)
               | ((uint32_t)s_adc_ch[8]  << 24);
    ADC1->SQR3 = ((uint32_t)s_adc_ch[9]  << 0)
               | ((uint32_t)s_adc_ch[10] << 6)
               | ((uint32_t)s_adc_ch[11] << 12);
    ADC1->SQR4 = 0;

    /* CFGR: DMA circular, modo contínuo, 12 bits */
    ADC1->CFGR = (1U << 0)   /* DMAEN */
               | (1U << 1)   /* DMACFG = circular */
               | (0U << 3)   /* RES = 12 bits */
               | (1U << 13); /* CONT = contínuo */

    /* DMA1 canal 1 para ADC1 */
    DMA1->CH[0].CCR  = 0;
    DMA1->CH[0].CPAR = ADC1_BASE + 0x40UL;  /* ADC1->DR */
    DMA1->CH[0].CMAR = (uint32_t)g_adc_dma_buf;
    DMA1->CH[0].CNDTR = ADC_CHANNELS * OVERSAMPLE_N;
    DMA1->CH[0].CCR = (1U << 5)    /* CIRC: circular */
                    | (1U << 7)    /* MINC: incrementa memória */
                    | (0U << 8)    /* PSIZE: 16 bits */
                    | (1U << 10)   /* MSIZE: 16 bits */
                    | (1U << 0);   /* EN */

    /* Inicia conversão */
    ADC1->CR |= (1U << 2);  /* ADSTART */
}

uint16_t DRV_ADC_GetRaw(uint8_t ch)
{
    if (ch >= ADC_CHANNELS) return 0;

    /* Calcula média das OVERSAMPLE_N amostras deste canal */
    uint32_t sum = 0;
    for (uint8_t i = 0; i < OVERSAMPLE_N; i++) {
        sum += g_adc_dma_buf[i * ADC_CHANNELS + ch];
    }
    return (uint16_t)(sum / OVERSAMPLE_N);
}

uint16_t DRV_ADC_RawToMv(uint16_t raw)
{
    return (uint16_t)((uint32_t)raw * ADC_VREF_MV / 4095U);
}

uint16_t DRV_ADC_ReadSingle(uint8_t ch)
{
    return DRV_ADC_GetRaw(ch);
}

/* ----------------------------------------------------------------
 * ADC2 para knock (alta velocidade, leitura por demanda)
 * ---------------------------------------------------------------- */
#define ADC2_BASE  0x50040100UL
#define ADC2       ((ADC_Regs_t *)ADC2_BASE)

static uint16_t s_knock_peak = 0;

void DRV_ADC_Knock_Init(void)
{
    /* ADC2 habilitado pelo mesmo clock AHB2 bit 13 */
    ADC2->CR = (1U << 28);
    for (volatile uint32_t i = 0; i < 2000; i++) __asm volatile("nop");
    ADC2->CR |= (1U << 31);
    while (ADC2->CR & (1U << 31));
    ADC2->ISR = (1U << 0);
    ADC2->CR |= (1U << 0);
    while (!(ADC2->ISR & (1U << 0)));

    /* Canal único do microfone piezo (ADC_CH_KNOCK definido em ecu_config.h) */
    ADC2->SMPR1 = (2U << (ADC_CH_KNOCK * 3));  /* 12.5 ciclos = amostragem rápida */
    ADC2->SQR1  = (uint32_t)ADC_CH_KNOCK << 6;
    ADC2->CFGR  = (0U << 13);  /* Single */
}

uint16_t DRV_ADC_Knock_GetPeak(void)
{
    /* Dispara 8 conversões e retorna o pico */
    s_knock_peak = 0;
    for (uint8_t i = 0; i < 8; i++) {
        ADC2->CR |= (1U << 2);   /* ADSTART */
        while (!(ADC2->ISR & (1U << 2)));  /* EOC */
        ADC2->ISR = (1U << 2);
        uint16_t val = (uint16_t)(ADC2->DR & 0xFFFU);
        if (val > s_knock_peak) s_knock_peak = val;
    }
    return s_knock_peak;
}
