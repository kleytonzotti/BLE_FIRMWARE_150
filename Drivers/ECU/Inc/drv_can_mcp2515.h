/**
 * drv_can_mcp2515.h — Driver CAN 2.0B via MCP2515 (SPI1).
 *
 * Cristal externo do MCP2515: 8MHz.
 * SPI: modo 0,0 — até 10MHz.
 */
#ifndef DRV_CAN_MCP2515_H
#define DRV_CAN_MCP2515_H

#include <stdint.h>
#include "ecu_types.h"

typedef struct {
    uint32_t id;        /* 11-bit standard ou 29-bit extended */
    uint8_t  extended;  /* 0=standard, 1=extended */
    uint8_t  dlc;       /* 0-8 bytes */
    uint8_t  data[8];
} CanFrame_t;

EcuResult_t DRV_CAN_Init(uint32_t baud);
EcuResult_t DRV_CAN_Send(const CanFrame_t *frame);
uint8_t     DRV_CAN_Receive(CanFrame_t *frame);   /* 1 se recebeu */
uint8_t     DRV_CAN_IsPresent(void);               /* Testa comunicação SPI */
void        DRV_CAN_Reset(void);

/* Mensagens padronizadas da ECU no barramento CAN */
void DRV_CAN_SendTelemetry(const volatile EngineState_t *eng);
void DRV_CAN_SendFaults(const volatile EngineState_t *eng);

/* SPI1 compartilhado (CAN + O2 CJ125) */
void    DRV_SPI1_Init(void);
uint8_t DRV_SPI1_TransferByte(uint8_t tx);

#endif /* DRV_CAN_MCP2515_H */
