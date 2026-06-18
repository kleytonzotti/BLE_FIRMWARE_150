/**
 * pinout.h — Mapeamento de pinos do STM32WB55 para ECU Programável.
 * Macros de acesso direto por registrador (sem HAL).
 *
 * IMPORTANTE: Todos os pinos são para a placa STM32WB5MM-DK.
 * Ajuste conforme a PCB final.
 */
#ifndef PINOUT_H
#define PINOUT_H

/* GPIO register layout (STM32 family) — define aqui para não depender do HAL */
typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;

/* STM32WB GPIO peripheral instances (RM0434, Table 3) */
#define GPIOA   ((GPIO_TypeDef *)0x48000000UL)
#define GPIOB   ((GPIO_TypeDef *)0x48000400UL)
#define GPIOC   ((GPIO_TypeDef *)0x48000800UL)
#define GPIOD   ((GPIO_TypeDef *)0x48000C00UL)
#define GPIOE   ((GPIO_TypeDef *)0x48001000UL)
#define GPIOH   ((GPIO_TypeDef *)0x48001C00UL)

/* ================================================================
 * HELPERS DE REGISTRADOR GPIO
 * SET  = coloca em 1 (BSRR bits 0-15)
 * CLR  = coloca em 0 (BSRR bits 16-31)
 * READ = lê estado
 * ================================================================ */
#define GPIO_SET(port, pin)    ((port)->BSRR  = (1U << (pin)))
#define GPIO_CLR(port, pin)    ((port)->BSRR  = (1U << ((pin) + 16U)))
#define GPIO_TOG(port, pin)    ((port)->ODR   ^= (1U << (pin)))
#define GPIO_READ(port, pin)   (((port)->IDR  >> (pin)) & 1U)

/* ================================================================
 * ENTRADAS — SENSORES
 * ================================================================ */

/* CKP — Sensor de Posição do Virabrequim
 * PA0 = TIM2_CH1 (Input Capture) — Hall effect ou VR com MAX9924 */
#define CKP_PORT        GPIOA
#define CKP_PIN         0

/* CMP — Sensor de Posição do Comando
 * PA1 = TIM2_CH2 (Input Capture) */
#define CMP_PORT        GPIOA
#define CMP_PIN         1

/* VSS — Sensor de Velocidade (frequência)
 * PB3 = TIM2_CH2 ou TIM3_CH1 */
#define VSS_PORT        GPIOB
#define VSS_PIN         3

/* FLEX — Sensor de Composição do Combustível (frequência 50-150Hz)
 * PB4 = TIM3_CH1 */
#define FLEX_PORT       GPIOB
#define FLEX_PIN        4

/* PARTIDA — Sinal de ignição ligada
 * PB5 = GPIO Input */
#define START_PORT      GPIOB
#define START_PIN       5

/* ================================================================
 * ADC — Canais Analógicos
 * Todos lidos via DMA — não usar GPIO_READ nestas
 * ================================================================ */
/* PC0 = ADC1_IN1 = TPS
 * PC1 = ADC1_IN2 = MAP
 * PC2 = ADC1_IN3 = CLT
 * PC3 = ADC1_IN4 = IAT
 * PC4 = ADC1_IN13 = BATT (com divisor 39k/10k)
 * PC5 = ADC1_IN14 = O2 (analógico, se sem CJ125 SPI)
 * PA4 = ADC1_IN9  = KNOCK (alta velocidade, ADC2)
 * PA5 = ADC1_IN10 = BOOST
 * PA6 = ADC1_IN11 = OIL PRESSURE
 * PA7 = ADC1_IN12 = FUEL PRESSURE
 * PB0 = ADC1_IN15 = EGT (saída 0-3.3V do MAX31855)
 * PB1 = ADC1_IN16 = SPARE
 */

/* ================================================================
 * SAÍDAS — INJETORES (via VNQ860SP ou MOSFET individual)
 * TIM1 Output Compare — precisão de timer, não GPIO direto
 * PA8  = TIM1_CH1 = INJ1
 * PA9  = TIM1_CH2 = INJ2
 * PA10 = TIM1_CH3 = INJ3
 * PA11 = TIM1_CH4 = INJ4
 * ================================================================ */
#define INJ1_PORT       GPIOA
#define INJ1_PIN        8
#define INJ2_PORT       GPIOA
#define INJ2_PIN        9
#define INJ3_PORT       GPIOA
#define INJ3_PIN        10
#define INJ4_PORT       GPIOA
#define INJ4_PIN        11

/* Macros de injetor — abrir = nível alto (MOSFET N-CH ativa alto) */
#define INJ1_OPEN()     GPIO_SET(INJ1_PORT, INJ1_PIN)
#define INJ1_CLOSE()    GPIO_CLR(INJ1_PORT, INJ1_PIN)
#define INJ2_OPEN()     GPIO_SET(INJ2_PORT, INJ2_PIN)
#define INJ2_CLOSE()    GPIO_CLR(INJ2_PORT, INJ2_PIN)
#define INJ3_OPEN()     GPIO_SET(INJ3_PORT, INJ3_PIN)
#define INJ3_CLOSE()    GPIO_CLR(INJ3_PORT, INJ3_PIN)
#define INJ4_OPEN()     GPIO_SET(INJ4_PORT, INJ4_PIN)
#define INJ4_CLOSE()    GPIO_CLR(INJ4_PORT, INJ4_PIN)

/* ================================================================
 * SAÍDAS — IGNIÇÃO (IGBT)
 * TIM8 Output Compare
 * PC6 = TIM8_CH1 = IGN1
 * PC7 = TIM8_CH2 = IGN2
 * PC8 = TIM8_CH3 = IGN3
 * PC9 = TIM8_CH4 = IGN4
 * ================================================================ */
#define IGN1_PORT       GPIOC
#define IGN1_PIN        6
#define IGN2_PORT       GPIOC
#define IGN2_PIN        7
#define IGN3_PORT       GPIOC
#define IGN3_PIN        8
#define IGN4_PORT       GPIOC
#define IGN4_PIN        9

/* IGBT N-CH: alto = bobina energizando (dwell), baixo = disparo */
#define IGN1_DWELL()    GPIO_SET(IGN1_PORT, IGN1_PIN)
#define IGN1_FIRE()     GPIO_CLR(IGN1_PORT, IGN1_PIN)
#define IGN2_DWELL()    GPIO_SET(IGN2_PORT, IGN2_PIN)
#define IGN2_FIRE()     GPIO_CLR(IGN2_PORT, IGN2_PIN)
#define IGN3_DWELL()    GPIO_SET(IGN3_PORT, IGN3_PIN)
#define IGN3_FIRE()     GPIO_CLR(IGN3_PORT, IGN3_PIN)
#define IGN4_DWELL()    GPIO_SET(IGN4_PORT, IGN4_PIN)
#define IGN4_FIRE()     GPIO_CLR(IGN4_PORT, IGN4_PIN)

/* ================================================================
 * CDI — Gate do SCR (via optocoupler PC817)
 * PB10 = CDI1, PB11 = CDI2
 * Pulso curto (100µs) dispara o SCR
 * ================================================================ */
#define CDI1_PORT       GPIOB
#define CDI1_PIN        10
#define CDI2_PORT       GPIOB
#define CDI2_PIN        11

#define CDI1_TRIGGER()  GPIO_SET(CDI1_PORT, CDI1_PIN)
#define CDI1_CLEAR()    GPIO_CLR(CDI1_PORT, CDI1_PIN)
#define CDI2_TRIGGER()  GPIO_SET(CDI2_PORT, CDI2_PIN)
#define CDI2_CLEAR()    GPIO_CLR(CDI2_PORT, CDI2_PIN)

/* ================================================================
 * CDI BOOST PWM — Conversor 12V→350V
 * PB12 = TIM1_CH1N ou TIM16_CH1 — PWM 100kHz
 * ================================================================ */
#define CDI_BOOST_PORT  GPIOB
#define CDI_BOOST_PIN   12

/* ================================================================
 * SAÍDAS AUXILIARES
 * ================================================================ */
#define FUEL_PUMP_PORT  GPIOB
#define FUEL_PUMP_PIN   13
#define FAN_PORT        GPIOB
#define FAN_PIN         14
#define CHECK_ENG_PORT  GPIOB
#define CHECK_ENG_PIN   15

#define FUEL_PUMP_ON()  GPIO_SET(FUEL_PUMP_PORT, FUEL_PUMP_PIN)
#define FUEL_PUMP_OFF() GPIO_CLR(FUEL_PUMP_PORT, FUEL_PUMP_PIN)
#define FAN_ON()        GPIO_SET(FAN_PORT, FAN_PIN)
#define FAN_OFF()        GPIO_CLR(FAN_PORT, FAN_PIN)
#define CHECK_ENG_ON()  GPIO_SET(CHECK_ENG_PORT, CHECK_ENG_PIN)
#define CHECK_ENG_OFF() GPIO_CLR(CHECK_ENG_PORT, CHECK_ENG_PIN)

/* ================================================================
 * IAC — Stepper Motor (A4988)
 * PC10 = STEP, PC11 = DIR, PC12 = ENABLE (ativo baixo)
 * ================================================================ */
#define IAC_STEP_PORT   GPIOC
#define IAC_STEP_PIN    10
#define IAC_DIR_PORT    GPIOC
#define IAC_DIR_PIN     11
#define IAC_EN_PORT     GPIOC
#define IAC_EN_PIN      12

#define IAC_ENABLE()    GPIO_CLR(IAC_EN_PORT, IAC_EN_PIN)
#define IAC_DISABLE()   GPIO_SET(IAC_EN_PORT, IAC_EN_PIN)
#define IAC_STEP()      { GPIO_SET(IAC_STEP_PORT, IAC_STEP_PIN); \
                          GPIO_CLR(IAC_STEP_PORT, IAC_STEP_PIN); }
#define IAC_DIR_OPEN()  GPIO_SET(IAC_DIR_PORT, IAC_DIR_PIN)
#define IAC_DIR_CLOSE() GPIO_CLR(IAC_DIR_PORT, IAC_DIR_PIN)

/* ================================================================
 * WASTEGATE — PWM via TIM16_CH1
 * PB8 = TIM16_CH1
 * ================================================================ */
#define WG_PORT         GPIOB
#define WG_PIN          8

/* ================================================================
 * SPI1 — CAN (MCP2515) + O2 (CJ125)
 * PA5 = SCK, PA6 = MISO, PA7 = MOSI
 * PD0 = CS_CAN (MCP2515 chip select)
 * PD1 = CS_O2  (CJ125 chip select)
 * PD2 = INT_CAN (MCP2515 interrupt)
 * ================================================================ */
#define SPI1_CS_CAN_PORT    GPIOD
#define SPI1_CS_CAN_PIN     0
#define SPI1_CS_O2_PORT     GPIOD
#define SPI1_CS_O2_PIN      1
#define CAN_INT_PORT        GPIOD
#define CAN_INT_PIN         2

#define CAN_CS_LOW()    GPIO_CLR(SPI1_CS_CAN_PORT, SPI1_CS_CAN_PIN)
#define CAN_CS_HIGH()   GPIO_SET(SPI1_CS_CAN_PORT, SPI1_CS_CAN_PIN)
#define O2_CS_LOW()     GPIO_CLR(SPI1_CS_O2_PORT, SPI1_CS_O2_PIN)
#define O2_CS_HIGH()    GPIO_SET(SPI1_CS_O2_PORT, SPI1_CS_O2_PIN)
#define CAN_INT_READ()  GPIO_READ(CAN_INT_PORT, CAN_INT_PIN)

/* ================================================================
 * SPI2 — Flash externa W25Q64 (datalogging)
 * PB13 = SCK, PB14 = MISO, PB15 = MOSI
 * PC13 = CS_FLASH
 * ================================================================ */
#define FLASH_CS_PORT   GPIOC
#define FLASH_CS_PIN    13
#define FLASH_CS_LOW()  GPIO_CLR(FLASH_CS_PORT, FLASH_CS_PIN)
#define FLASH_CS_HIGH() GPIO_SET(FLASH_CS_PORT, FLASH_CS_PIN)

/* ================================================================
 * USART1 — Debug / Tuning serial (115200 baud)
 * PA9 = TX (compartilhado com INJ2 — desabilitar se injetor ativo)
 * PA10 = RX (compartilhado com INJ3)
 * ATENÇÃO: em placa final usar pinos exclusivos
 * ================================================================ */
#define UART_TX_PORT    GPIOA
#define UART_TX_PIN     9
#define UART_RX_PORT    GPIOA
#define UART_RX_PIN     10

/* ================================================================
 * LED DE STATUS (STM32WB5MM-DK onboard LEDs)
 * PB0 = LED_BLUE, PB1 = LED_GREEN, PB5 = LED_RED
 * ================================================================ */
#define LED_BLUE_PORT   GPIOB
#define LED_BLUE_PIN    0
#define LED_GREEN_PORT  GPIOB
#define LED_GREEN_PIN   1
#define LED_RED_PORT    GPIOB
#define LED_RED_PIN     5

#define LED_BLUE_ON()   GPIO_SET(LED_BLUE_PORT, LED_BLUE_PIN)
#define LED_BLUE_OFF()  GPIO_CLR(LED_BLUE_PORT, LED_BLUE_PIN)
#define LED_GREEN_ON()  GPIO_SET(LED_GREEN_PORT, LED_GREEN_PIN)
#define LED_GREEN_OFF() GPIO_CLR(LED_GREEN_PORT, LED_GREEN_PIN)
#define LED_RED_ON()    GPIO_SET(LED_RED_PORT, LED_RED_PIN)
#define LED_RED_OFF()   GPIO_CLR(LED_RED_PORT, LED_RED_PIN)

#endif /* PINOUT_H */
