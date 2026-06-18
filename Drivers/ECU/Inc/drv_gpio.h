/**
 * drv_gpio.h — Inicialização e acesso a GPIO via registradores STM32WB55.
 */
#ifndef DRV_GPIO_H
#define DRV_GPIO_H

#include <stdint.h>
#include "pinout.h"

void    DRV_GPIO_Init(void);
void    DRV_GPIO_SetMode(GPIO_TypeDef *port, uint8_t pin, uint8_t mode);
/* mode: 0=Input, 1=Output, 2=AltFunc, 3=Analog */
void    DRV_GPIO_SetAF(GPIO_TypeDef *port, uint8_t pin, uint8_t af);
void    DRV_GPIO_SetSpeed(GPIO_TypeDef *port, uint8_t pin, uint8_t speed);
/* speed: 0=Low, 1=Medium, 2=High, 3=VeryHigh */
void    DRV_GPIO_SetPull(GPIO_TypeDef *port, uint8_t pin, uint8_t pull);
/* pull: 0=None, 1=PullUp, 2=PullDown */
void    DRV_GPIO_OpenInjectors(uint8_t mask);    /* bit0=INJ1..bit3=INJ4, via BSRR */
void    DRV_GPIO_CloseInjectors(uint8_t mask);
uint8_t DRV_GPIO_ReadPin(GPIO_TypeDef *port, uint8_t pin);

#endif /* DRV_GPIO_H */
