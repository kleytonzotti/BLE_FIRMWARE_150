/**
 * drv_oled.h — Driver OLED SSD1315/SSD1306 128×64, I2C software (bit-bang).
 *
 * Bare-metal, sem HAL. Pinos configurados em pinout.h:
 *   OLED_SCL_PORT/PIN → PC0   (normalmente ADC FLEX, reconfigurado em sim)
 *   OLED_SDA_PORT/PIN → PC1   (normalmente ADC KNOCK, reconfigurado em sim)
 *   OLED_I2C_ADDR     → 0x3C  (SA0 = GND na STM32WB5MM-DK)
 *
 * CubeMX: PC0 e PC1 devem estar configurados como GPIO_Output em modo simulação
 *         OU deixar DRV_OLED_Init() reconfigurá-los.
 *
 * Uso:
 *   DRV_OLED_Init();
 *   DRV_OLED_Clear();
 *   DRV_OLED_Print(0, 0, "HELLO ECU");
 *   DRV_OLED_Flush();
 *
 * Layout: fonte 6×8 px → 21 colunas × 8 linhas (128×64 px)
 */
#ifndef DRV_OLED_H
#define DRV_OLED_H

#include <stdint.h>

#define OLED_WIDTH      128U
#define OLED_HEIGHT     64U
#define OLED_PAGES      8U      /* 64px / 8px por página */
#define OLED_CHAR_W     6U      /* largura da fonte (5px glifo + 1px espaço) */
#define OLED_COLS       21U     /* 128 / 6 = 21 chars/linha */

void DRV_OLED_Init(void);
void DRV_OLED_Clear(void);

/* Escreve str a partir da coluna col_px (em pixels 0-127) na linha row (0-7) */
void DRV_OLED_Print(uint8_t col_px, uint8_t row, const char *str);

/* Inverte todos os pixels de uma linha (efeito destaque) */
void DRV_OLED_InvertRow(uint8_t row);

/* Envia o framebuffer completo para o display */
void DRV_OLED_Flush(void);

#endif /* DRV_OLED_H */
