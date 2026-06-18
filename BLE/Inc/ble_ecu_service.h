/**
 * ble_ecu_service.h — Serviço GATT customizado da ECU.
 * Registra características e gerencia conexão BLE.
 */
#ifndef BLE_ECU_SERVICE_H
#define BLE_ECU_SERVICE_H

#include "ble_protocol.h"
#include "ble_wpan_api.h"
#include "ecu_types.h"
#include "ecu_sensors.h"

/* Handles das características GATT (preenchidos em BLE_ECU_Service_Init) */
typedef struct {
    uint16_t service;
    uint16_t telem_notify;
    uint16_t fault_notify;
    uint16_t ack_notify;
    uint16_t cmd_write;
    uint16_t sensor_raw;
} EcuGattHandles_t;

#define BLE_ECU_MAX_ATTR_RECORDS    20
#define BLE_TABLE_CHUNK_COUNT       4
#define BLE_CHUNK_MAX_DATA          60

extern EcuGattHandles_t g_ecu_handles;

/* Registra serviço e todas as características no stack BLE ST */
tBleStatus BLE_ECU_Service_Init(void);

/* Envia dados via notify */
tBleStatus BLE_ECU_Notify_Telemetry(const BleTelemPacket_t *pkt);
tBleStatus BLE_ECU_Notify_Faults(const BleFaultPacket_t *pkt);
tBleStatus BLE_ECU_Notify_Ack(const BleCmdAck_t *ack);

/* Callback chamado pelo app_ble.c ao receber escrita do cliente */
void BLE_ECU_GATT_Event(uint16_t handle, const uint8_t *data, uint8_t len);

/* Tarefas periódicas do main loop */
void BLE_ECU_SendTelemetry(const volatile EngineState_t *eng,
                            EngineStateMachine_t state);
void BLE_ECU_SendFaults(const volatile EngineState_t *eng);
void BLE_ECU_SendSensorRaw(const volatile AdcData_t *adc);

/* Gerenciamento de conexão */
uint8_t BLE_ECU_IsConnected(void);
void    BLE_ECU_SetConnectionHandle(uint16_t h);
void    BLE_ECU_OnDisconnect(void);

#endif /* BLE_ECU_SERVICE_H */
