/**
 * drv_flash.c — Acesso à flash interna do STM32WB55 em nível de registrador.
 *
 * Estratégia dual-bank:
 *   Page A (0x080FE000): perfil ativo
 *   Page B (0x080FF000): backup / nova escrita
 *
 * Fluxo de gravação:
 *   1. Apaga página inativa
 *   2. Escreve nova configuração + CRC
 *   3. Apaga página ativa
 *   4. Copia novo para ativo (ou reutiliza banco B como ativo via flag)
 *
 * Simplificado: escreve direto em B, depois apaga A e copia para A.
 */

#include "drv_flash.h"
#include "ecu_config.h"

/* ----------------------------------------------------------------
 * Registradores FLASH STM32WB55
 * ---------------------------------------------------------------- */
#define FLASH_BASE_REG  0x58004000UL

typedef struct {
    volatile uint32_t ACR;
    volatile uint32_t _r0;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t ECCR;
    volatile uint32_t _r1;
    volatile uint32_t OPTR;
    volatile uint32_t PCROP1ASR;
    volatile uint32_t PCROP1AER;
    volatile uint32_t WRP1AR;
    volatile uint32_t WRP1BR;
    volatile uint32_t PCROP1BSR;
    volatile uint32_t PCROP1BER;
} FLASH_Regs_t;

#define FLASH_REG   ((FLASH_Regs_t *)FLASH_BASE_REG)

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL
#define FLASH_SR_BSY  (1U << 16)
#define FLASH_CR_PG   (1U << 0)
#define FLASH_CR_PER  (1U << 1)
#define FLASH_CR_STRT (1U << 16)
#define FLASH_CR_LOCK (1U << 31)

static void _flash_unlock(void)
{
    if (FLASH_REG->CR & FLASH_CR_LOCK) {
        FLASH_REG->KEYR = FLASH_KEY1;
        FLASH_REG->KEYR = FLASH_KEY2;
    }
}

static void _flash_lock(void)
{
    FLASH_REG->CR |= FLASH_CR_LOCK;
}

static void _flash_wait_busy(void)
{
    while (FLASH_REG->SR & FLASH_SR_BSY);
}

EcuResult_t DRV_Flash_ErasePage(uint32_t page_addr)
{
    /* Página = (addr - 0x08000000) / 4096 */
    uint32_t page = (page_addr - 0x08000000UL) / 4096UL;

    _flash_unlock();
    _flash_wait_busy();

    FLASH_REG->CR = FLASH_CR_PER | ((page & 0xFFU) << 3);
    FLASH_REG->CR |= FLASH_CR_STRT;
    _flash_wait_busy();

    FLASH_REG->CR &= ~(FLASH_CR_PER | (0xFFU << 3));
    _flash_lock();

    return (FLASH_REG->SR & 0xC3FAU) ? ECU_ERR_FLASH : ECU_OK;
}

EcuResult_t DRV_Flash_Write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    /* STM32WB55 exige escrita em double-word (64 bits / 8 bytes) */
    if ((addr % 8) != 0 || (len % 8) != 0) return ECU_ERR_PARAM;

    _flash_unlock();
    _flash_wait_busy();

    FLASH_REG->SR = 0xC3FAUL;  /* Limpa flags de erro */
    FLASH_REG->CR |= FLASH_CR_PG;

    const uint32_t *src32 = (const uint32_t *)data;
    volatile uint32_t *dst32 = (volatile uint32_t *)addr;

    for (uint32_t i = 0; i < len / 4U; i += 2U) {
        dst32[i]     = src32[i];
        dst32[i + 1] = src32[i + 1];
        _flash_wait_busy();
        if (FLASH_REG->SR & 0xC3FAUL) {
            FLASH_REG->CR &= ~FLASH_CR_PG;
            _flash_lock();
            return ECU_ERR_FLASH;
        }
    }

    FLASH_REG->CR &= ~FLASH_CR_PG;
    _flash_lock();
    return ECU_OK;
}

EcuResult_t DRV_Flash_Read(uint32_t addr, uint8_t *data, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)addr;
    for (uint32_t i = 0; i < len; i++) data[i] = src[i];
    return ECU_OK;
}

EcuResult_t DRV_Flash_SaveProfile(const EcuProfile_t *prof)
{
    EcuProfile_t tmp;
    __builtin_memcpy(&tmp, prof, sizeof(EcuProfile_t));

    /* Atualiza magic e CRC */
    tmp.magic = ECU_CONFIG_MAGIC;
    uint32_t len_no_crc = sizeof(EcuProfile_t) - 4U;
    tmp.crc32 = DRV_CRC32_Calc((const uint8_t *)&tmp, len_no_crc);

    /* Alinha para múltiplo de 8 bytes */
    uint32_t write_len = (sizeof(EcuProfile_t) + 7U) & ~7U;

    /* Apaga Page B e escreve lá */
    EcuResult_t r = DRV_Flash_ErasePage(ECU_CONFIG_PAGE_B);
    if (r != ECU_OK) return r;

    r = DRV_Flash_Write(ECU_CONFIG_PAGE_B, (const uint8_t *)&tmp, write_len);
    if (r != ECU_OK) return r;

    /* Apaga Page A e copia de B para A */
    r = DRV_Flash_ErasePage(ECU_CONFIG_PAGE_A);
    if (r != ECU_OK) return r;

    r = DRV_Flash_Write(ECU_CONFIG_PAGE_A, (const uint8_t *)ECU_CONFIG_PAGE_B, write_len);
    return r;
}

EcuResult_t DRV_Flash_LoadProfile(EcuProfile_t *prof)
{
    const EcuProfile_t *p_a = (const EcuProfile_t *)ECU_CONFIG_PAGE_A;
    const EcuProfile_t *p_b = (const EcuProfile_t *)ECU_CONFIG_PAGE_B;

    /* Valida page A */
    if (p_a->magic == ECU_CONFIG_MAGIC) {
        uint32_t crc = DRV_CRC32_Calc((const uint8_t *)p_a, sizeof(EcuProfile_t) - 4U);
        if (crc == p_a->crc32) {
            __builtin_memcpy(prof, p_a, sizeof(EcuProfile_t));
            return ECU_OK;
        }
    }

    /* Tenta page B */
    if (p_b->magic == ECU_CONFIG_MAGIC) {
        uint32_t crc = DRV_CRC32_Calc((const uint8_t *)p_b, sizeof(EcuProfile_t) - 4U);
        if (crc == p_b->crc32) {
            __builtin_memcpy(prof, p_b, sizeof(EcuProfile_t));
            return ECU_OK;
        }
    }

    return ECU_ERR_FLASH;
}

/* CRC32 por software (polinômio IEEE 802.3 = 0xEDB88320) */
uint32_t DRV_CRC32_Calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}
