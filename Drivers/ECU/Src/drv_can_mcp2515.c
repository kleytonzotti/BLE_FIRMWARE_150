/**
 * drv_can_mcp2515.c — Driver MCP2515 via SPI1 (500kbps, CAN 2.0B).
 *
 * SPI1: PB3=SCK, PB4=MISO, PB5=MOSI, PD0=CS_CAN
 * Frequência SPI: 8MHz (SPI1 a 64MHz / 8 = 8MHz)
 * MCP2515 clock externo: 16MHz xtal → 500kbps = TQ=2µs, BRP=1, PHSEG=16TQ
 *
 * Identificadores CAN:
 *   0x700 — Telemetria (8 bytes, 10Hz)
 *   0x701 — Falhas ativas (4 bytes)
 */

#include "drv_can_mcp2515.h"
#include "ecu_config.h"
#include "pinout.h"

/* ----------------------------------------------------------------
 * SPI1 em nível de registrador
 * ---------------------------------------------------------------- */
#define RCC_APB2ENR  (*((volatile uint32_t *)0x40021060UL))

typedef struct {
    volatile uint32_t CR1, CR2, SR, DR, CRCPR, RXCRCR, TXCRCR, _r;
} SPI_Regs_t;

#define SPI1  ((SPI_Regs_t *)0x40013000UL)
#define SPI1_SR_TXE   (1U << 1)
#define SPI1_SR_RXNE  (1U << 0)
#define SPI1_SR_BSY   (1U << 7)

void DRV_SPI1_Init(void)
{
    RCC_APB2ENR |= (1U << 12);  /* SPI1EN */

    SPI1->CR1 = 0;
    /* Master, CPOL=0, CPHA=0, BR=010(÷8=8MHz), SSM+SSI, 8-bit */
    SPI1->CR1 = (1U << 2)    /* MSTR */
              | (2U << 3)    /* BR=010: /8 = 8MHz */
              | (1U << 8)    /* SSI */
              | (1U << 9);   /* SSM */
    SPI1->CR2 = (7U << 8);   /* DS=0111: 8 bits */
    SPI1->CR1 |= (1U << 6);  /* SPE */
}

uint8_t DRV_SPI1_TransferByte(uint8_t tx)
{
    while (!(SPI1->SR & SPI1_SR_TXE));
    *((volatile uint8_t *)&SPI1->DR) = tx;
    while (!(SPI1->SR & SPI1_SR_RXNE));
    return (uint8_t)SPI1->DR;
}

/* ----------------------------------------------------------------
 * MCP2515 Instruções
 * ---------------------------------------------------------------- */
#define MCP_RESET       0xC0
#define MCP_READ        0x03
#define MCP_WRITE       0x02
#define MCP_RTS_TX0     0x81
#define MCP_READ_STATUS 0xA0
#define MCP_BIT_MODIFY  0x05
#define MCP_READ_RX0    0x90

/* Registradores MCP2515 */
#define MCP_CANSTAT  0x0E
#define MCP_CANCTRL  0x0F
#define MCP_CNF3     0x28
#define MCP_CNF2     0x29
#define MCP_CNF1     0x2A
#define MCP_CANINTE  0x2B
#define MCP_CANINTF  0x2C
#define MCP_TXB0CTRL 0x30
#define MCP_TXB0SIDH 0x31
#define MCP_TXB0SIDL 0x32
#define MCP_TXB0DLC  0x35
#define MCP_TXB0D0   0x36
#define MCP_RXB0CTRL 0x60
#define MCP_RXB0SIDH 0x61
#define MCP_RXB0SIDL 0x62
#define MCP_RXB0DLC  0x65
#define MCP_RXB0D0   0x66

#define CS_LOW()   GPIO_CLR(GPIOD, 0)
#define CS_HIGH()  GPIO_SET(GPIOD, 0)

static uint8_t _mcp_read(uint8_t reg)
{
    CS_LOW();
    DRV_SPI1_TransferByte(MCP_READ);
    DRV_SPI1_TransferByte(reg);
    uint8_t val = DRV_SPI1_TransferByte(0xFF);
    CS_HIGH();
    return val;
}

static void _mcp_write(uint8_t reg, uint8_t val)
{
    CS_LOW();
    DRV_SPI1_TransferByte(MCP_WRITE);
    DRV_SPI1_TransferByte(reg);
    DRV_SPI1_TransferByte(val);
    CS_HIGH();
}

static void _mcp_bit_modify(uint8_t reg, uint8_t mask, uint8_t val)
{
    CS_LOW();
    DRV_SPI1_TransferByte(MCP_BIT_MODIFY);
    DRV_SPI1_TransferByte(reg);
    DRV_SPI1_TransferByte(mask);
    DRV_SPI1_TransferByte(val);
    CS_HIGH();
}

uint8_t DRV_CAN_IsPresent(void)
{
    DRV_SPI1_Init();
    /* Reset e testa leitura de CANSTAT */
    CS_LOW();
    DRV_SPI1_TransferByte(MCP_RESET);
    CS_HIGH();
    for (volatile uint32_t i = 0; i < 10000; i++) __asm volatile("nop");
    uint8_t stat = _mcp_read(MCP_CANSTAT);
    return ((stat & 0xE0) == 0x80) ? 1 : 0;  /* Modo configuração = 0x80 */
}

EcuResult_t DRV_CAN_Init(uint32_t baud)
{
    (void)baud;
    DRV_SPI1_Init();

    CS_LOW();
    DRV_SPI1_TransferByte(MCP_RESET);
    CS_HIGH();
    for (volatile uint32_t i = 0; i < 10000; i++) __asm volatile("nop");

    /* Configura 500kbps com xtal 16MHz:
     * TQ = 2/(16MHz) = 0.125µs → 16TQ/bit
     * CNF1: BRP=1 (÷4 → 4MHz), SJW=1
     * CNF2: BTLMODE=1, SAM=0, PHSEG1=3(4TQ), PRSEG=2(3TQ)
     * CNF3: PHSEG2=3(4TQ)
     * Total: 1(sync)+3+4+4 = 12TQ → 12×0.25µs = 3µs... ajustar para projeto */
    _mcp_write(MCP_CNF1, 0x00);  /* BRP=0 (÷2), SJW=1TQ */
    _mcp_write(MCP_CNF2, 0xD0);  /* BTLMODE=1, SAM=0, PHSEG1=6TQ, PRSEG=1TQ */
    _mcp_write(MCP_CNF3, 0x05);  /* PHSEG2=6TQ */

    /* Sem interrupções (polling simples) */
    _mcp_write(MCP_CANINTE, 0x00);

    /* RXB0: aceita todos os frames */
    _mcp_write(MCP_RXB0CTRL, 0x60);

    /* Sai do modo configuração → Normal */
    _mcp_write(MCP_CANCTRL, 0x00);

    /* Aguarda entrar em modo normal */
    uint8_t tries = 100;
    while (tries-- && (_mcp_read(MCP_CANSTAT) & 0xE0) != 0x00) {
        for (volatile uint32_t i = 0; i < 1000; i++) __asm volatile("nop");
    }

    return ((_mcp_read(MCP_CANSTAT) & 0xE0) == 0x00) ? ECU_OK : ECU_ERR_CAN;
}

void DRV_CAN_Reset(void)
{
    CS_LOW();
    DRV_SPI1_TransferByte(MCP_RESET);
    CS_HIGH();
}

EcuResult_t DRV_CAN_Send(const CanFrame_t *frame)
{
    /* Carrega TXB0 */
    uint16_t sid = frame->id & 0x7FF;
    _mcp_write(MCP_TXB0SIDH, (uint8_t)(sid >> 3));
    _mcp_write(MCP_TXB0SIDL, (uint8_t)((sid & 0x7) << 5));
    _mcp_write(MCP_TXB0DLC,  frame->dlc & 0x0F);

    for (uint8_t i = 0; i < frame->dlc; i++) {
        _mcp_write((uint8_t)(MCP_TXB0D0 + i), frame->data[i]);
    }

    /* Solicita transmissão */
    CS_LOW();
    DRV_SPI1_TransferByte(MCP_RTS_TX0);
    CS_HIGH();

    /* Aguarda conclusão (máx 1ms) */
    for (uint16_t t = 0; t < 1000; t++) {
        uint8_t ctrl = _mcp_read(MCP_TXB0CTRL);
        if (!(ctrl & 0x08)) return ECU_OK;  /* TXREQ=0: enviado */
        for (volatile uint32_t i = 0; i < 100; i++) __asm volatile("nop");
    }
    return ECU_ERR_CAN;
}

uint8_t DRV_CAN_Receive(CanFrame_t *frame)
{
    uint8_t intf = _mcp_read(MCP_CANINTF);
    if (!(intf & 0x01)) return 0;  /* RX0IF não setado */

    uint8_t sidh = _mcp_read(MCP_RXB0SIDH);
    uint8_t sidl = _mcp_read(MCP_RXB0SIDL);
    frame->id  = ((uint32_t)sidh << 3) | (sidl >> 5);
    frame->dlc = _mcp_read(MCP_RXB0DLC) & 0x0F;

    for (uint8_t i = 0; i < frame->dlc; i++) {
        frame->data[i] = _mcp_read((uint8_t)(MCP_RXB0D0 + i));
    }

    _mcp_bit_modify(MCP_CANINTF, 0x01, 0x00);  /* Limpa RX0IF */
    return 1;
}

void DRV_CAN_SendTelemetry(const volatile EngineState_t *eng)
{
    CanFrame_t f = {0};
    f.id  = CAN_ID_ECU_TELEM;
    f.dlc = 8;
    /* Empacota RPM, TPS, MAP, CLT em 8 bytes */
    f.data[0] = (uint8_t)(eng->rpm >> 8);
    f.data[1] = (uint8_t)(eng->rpm & 0xFF);
    f.data[2] = (uint8_t)(eng->tps_pct / 10);
    f.data[3] = (uint8_t)(eng->map_kpa / 10);
    f.data[4] = (uint8_t)((eng->coolant_c / 10) + 40);  /* +40 offset */
    f.data[5] = (uint8_t)(eng->iat_c / 10 + 40);
    f.data[6] = (uint8_t)(eng->batt_mv / 100);
    f.data[7] = (uint8_t)((eng->o2_afr_x10 / 10) & 0xFF);
    DRV_CAN_Send(&f);
}

void DRV_CAN_SendFaults(const volatile EngineState_t *eng)
{
    CanFrame_t f = {0};
    f.id  = CAN_ID_ECU_FAULTS;
    f.dlc = 4;
    f.data[0] = (uint8_t)(eng->active_faults >> 24);
    f.data[1] = (uint8_t)(eng->active_faults >> 16);
    f.data[2] = (uint8_t)(eng->active_faults >> 8);
    f.data[3] = (uint8_t)(eng->active_faults & 0xFF);
    DRV_CAN_Send(&f);
}
