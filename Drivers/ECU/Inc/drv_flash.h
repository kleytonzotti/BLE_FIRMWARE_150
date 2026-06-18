/**
 * drv_flash.h — Gravação de perfil em flash interna STM32WB55.
 *
 * Estratégia dual-bank: sempre grava na página inativa e inverte.
 * Nunca apaga a página ativa enquanto a escrita não for concluída.
 * Páginas de 4KB cada: ECU_CONFIG_PAGE_A e ECU_CONFIG_PAGE_B.
 */
#ifndef DRV_FLASH_H
#define DRV_FLASH_H

#include <stdint.h>
#include "ecu_types.h"

EcuResult_t DRV_Flash_SaveProfile(const EcuProfile_t *prof);
EcuResult_t DRV_Flash_LoadProfile(EcuProfile_t *prof);
EcuResult_t DRV_Flash_Write(uint32_t addr, const uint8_t *data, uint32_t len);
EcuResult_t DRV_Flash_Read(uint32_t addr, uint8_t *data, uint32_t len);
EcuResult_t DRV_Flash_ErasePage(uint32_t page_addr);
uint32_t    DRV_CRC32_Calc(const uint8_t *data, uint32_t len);

#endif /* DRV_FLASH_H */
