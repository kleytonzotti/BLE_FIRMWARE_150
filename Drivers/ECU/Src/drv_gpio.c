/**
 * drv_gpio.c — Configuração de todos os GPIOs da ECU.
 *
 * Usa acesso direto aos registradores MODER/OTYPER/OSPEEDR/PUPDR/AFR.
 * Nenhuma chamada ao HAL.
 */

#include "drv_gpio.h"
#include "pinout.h"
#include "ecu_config.h"

/* ----------------------------------------------------------------
 * Endereços de registrador
 * ---------------------------------------------------------------- */
#define RCC_AHB2ENR  (*((volatile uint32_t *)0x4002104CUL))

/* GPIO_TypeDef e GPIOA-E definidos em pinout.h */

/* Habilita clock da porta se não estiver habilitado */
static void _gpio_clk_enable(GPIO_TypeDef *port)
{
    uint32_t bit = 0;
    if      (port == GPIOA) bit = 0;
    else if (port == GPIOB) bit = 1;
    else if (port == GPIOC) bit = 2;
    else if (port == GPIOD) bit = 3;
    else if (port == GPIOE) bit = 4;
    RCC_AHB2ENR |= (1U << bit);
}

/* Configura pino: mode 0=input, 1=output, 2=AF, 3=analog */
void DRV_GPIO_SetMode(GPIO_TypeDef *port, uint8_t pin, uint8_t mode)
{
    _gpio_clk_enable(port);
    port->MODER &= ~(3U << (pin * 2));
    port->MODER |=  ((uint32_t)mode << (pin * 2));
}

void DRV_GPIO_SetAF(GPIO_TypeDef *port, uint8_t pin, uint8_t af)
{
    uint8_t idx = pin / 8;
    uint8_t shift = (pin % 8) * 4;
    port->AFR[idx] &= ~(0xFU << shift);
    port->AFR[idx] |=  ((uint32_t)af << shift);
}

void DRV_GPIO_SetSpeed(GPIO_TypeDef *port, uint8_t pin, uint8_t speed)
{
    port->OSPEEDR &= ~(3U << (pin * 2));
    port->OSPEEDR |=  ((uint32_t)speed << (pin * 2));
}

void DRV_GPIO_SetPull(GPIO_TypeDef *port, uint8_t pin, uint8_t pull)
{
    port->PUPDR &= ~(3U << (pin * 2));
    port->PUPDR |=  ((uint32_t)pull << (pin * 2));
}

uint8_t DRV_GPIO_ReadPin(GPIO_TypeDef *port, uint8_t pin)
{
    return (uint8_t)((port->IDR >> pin) & 1U);
}

/* ----------------------------------------------------------------
 * Abre/fecha múltiplos injetores via máscara de bits
 * ---------------------------------------------------------------- */
void DRV_GPIO_OpenInjectors(uint8_t mask)
{
    /* INJ1=PA8, INJ2=PA9, INJ3=PA10, INJ4=PA11 */
    uint32_t set_bits = 0;
    if (mask & 1) set_bits |= (1U << 8);
    if (mask & 2) set_bits |= (1U << 9);
    if (mask & 4) set_bits |= (1U << 10);
    if (mask & 8) set_bits |= (1U << 11);
    GPIOA->BSRR = set_bits;
}

void DRV_GPIO_CloseInjectors(uint8_t mask)
{
    uint32_t clr_bits = 0;
    if (mask & 1) clr_bits |= (1U << (8  + 16));
    if (mask & 2) clr_bits |= (1U << (9  + 16));
    if (mask & 4) clr_bits |= (1U << (10 + 16));
    if (mask & 8) clr_bits |= (1U << (11 + 16));
    GPIOA->BSRR = clr_bits;
}

/* ----------------------------------------------------------------
 * Inicialização de todos os GPIOs da ECU
 * ---------------------------------------------------------------- */
void DRV_GPIO_Init(void)
{
    /* Habilita todos os clocks */
    RCC_AHB2ENR |= (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3);

    /* ---- ENTRADAS ---- */
    /* CKP: PA0 — TIM2_CH1 (AF1) */
    DRV_GPIO_SetMode(GPIOA, 0, 2);   /* AF */
    DRV_GPIO_SetAF(GPIOA,   0, 1);   /* TIM2_CH1 */
    DRV_GPIO_SetPull(GPIOA, 0, 1);   /* Pull-up */

    /* CMP: PA1 — TIM2_CH2 (AF1) */
    DRV_GPIO_SetMode(GPIOA, 1, 2);
    DRV_GPIO_SetAF(GPIOA,   1, 1);
    DRV_GPIO_SetPull(GPIOA, 1, 1);

    /* ADC canais PA0-PA7 como analógico (modo 3) */
    /* TPS=PA0 conflita com CKP — use entrada separada em prod. Aqui: PA4-PA7 */
    DRV_GPIO_SetMode(GPIOA, 4, 3);   /* BATT */
    DRV_GPIO_SetMode(GPIOA, 5, 3);   /* O2   */
    DRV_GPIO_SetMode(GPIOA, 6, 3);   /* BOOST */
    DRV_GPIO_SetMode(GPIOA, 7, 3);   /* OIL  */
    DRV_GPIO_SetMode(GPIOC, 0, 3);   /* FLEX */
    DRV_GPIO_SetMode(GPIOC, 1, 3);   /* KNOCK (ADC2) */
    DRV_GPIO_SetMode(GPIOC, 2, 3);   /* SPARE */

    /* ---- SAÍDAS PP, 50MHz ---- */
    /* INJ1-4: PA8-PA11 */
    for (uint8_t p = 8; p <= 11; p++) {
        DRV_GPIO_SetMode(GPIOA,  p, 1);   /* Output */
        DRV_GPIO_SetSpeed(GPIOA, p, 3);   /* Very high */
        GPIOA->BSRR = (1U << (p + 16));   /* Fecha (LOW = injetor fechado) */
    }

    /* IGN1-4: PC6-PC9 */
    for (uint8_t p = 6; p <= 9; p++) {
        DRV_GPIO_SetMode(GPIOC,  p, 1);
        DRV_GPIO_SetSpeed(GPIOC, p, 3);
        GPIOC->BSRR = (1U << p);          /* HIGH = bobina sem corrente */
    }

    /* CDI1: PB10, CDI2: PB11 */
    DRV_GPIO_SetMode(GPIOB, 10, 1); DRV_GPIO_SetSpeed(GPIOB, 10, 3);
    DRV_GPIO_SetMode(GPIOB, 11, 1); DRV_GPIO_SetSpeed(GPIOB, 11, 3);
    GPIOB->BSRR = (1U << (10 + 16)) | (1U << (11 + 16));  /* LOW */

    /* FUEL_PUMP: PB13 */
    DRV_GPIO_SetMode(GPIOB, 13, 1);
    GPIOB->BSRR = (1U << (13 + 16));  /* OFF */

    /* FAN: PB14 */
    DRV_GPIO_SetMode(GPIOB, 14, 1);
    GPIOB->BSRR = (1U << (14 + 16));

    /* CHECK_ENG: PB15 */
    DRV_GPIO_SetMode(GPIOB, 15, 1);
    GPIOB->BSRR = (1U << (15 + 16));

    /* IAC stepper: PC10/PC11/PC12 */
    DRV_GPIO_SetMode(GPIOC, 10, 1);
    DRV_GPIO_SetMode(GPIOC, 11, 1);
    DRV_GPIO_SetMode(GPIOC, 12, 1);

    /* Wastegate: PB8 — TIM16_CH1 (AF14) */
    DRV_GPIO_SetMode(GPIOB,  8, 2);
    DRV_GPIO_SetAF(GPIOB,    8, 14);
    DRV_GPIO_SetSpeed(GPIOB, 8, 2);

    /* CDI Boost: PB5 — TIM17_CH1 (AF14) */
    DRV_GPIO_SetMode(GPIOB,  5, 2);
    DRV_GPIO_SetAF(GPIOB,    5, 14);
    DRV_GPIO_SetSpeed(GPIOB, 5, 3);

    /* SPI1: PB3(SCK)/PB4(MISO)/PB5(MOSI) — conflito com CDI Boost em hardware final */
    /* PA15 = NSS (CS_CAN) */

    /* LEDs de diagnóstico: PB0/PB1 */
    DRV_GPIO_SetMode(GPIOB, 0, 1);
    DRV_GPIO_SetMode(GPIOB, 1, 1);
    GPIOB->BSRR = (1U << (0 + 16)) | (1U << (1 + 16));

    /* SPI CS para MCP2515: PD0 */
    DRV_GPIO_SetMode(GPIOD, 0, 1);
    GPIOD->BSRR = (1U << 0);  /* CS HIGH = inativo */
}
