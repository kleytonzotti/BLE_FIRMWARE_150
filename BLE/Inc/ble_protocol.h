/**
 * ble_protocol.h — Definição completa do protocolo BLE da ECU.
 *
 * UUID base: 00000000-EC01-1234-5678-ABCDEF000000
 * Nome BLE:  "ECU-PRO"
 * MTU recomendado: 100 bytes ou mais.
 */
#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include "ecu_types.h"

/* UUID base em formato little-endian para o stack BLE ST */
#define BLE_ECU_UUID_BASE { \
    0x00, 0x00, 0x00, 0xEF, 0xCD, 0xAB, 0x78, 0x56, \
    0x34, 0x12, 0x01, 0xEC, 0x00, 0x00, 0x00, 0x00  \
}

/* UUIDs de 16-bit das características (bytes 12-13 do UUID base) */
#define BLE_ECU_SERVICE_UUID        0x0001
#define BLE_ECU_TELEM_UUID          0x0002  /* Notify, 40 bytes */
#define BLE_ECU_CONFIG_UUID         0x0003  /* Read+Write, 64 bytes */
#define BLE_ECU_VE_MAP_UUID         0x0004  /* Read+Write, chunks 64 bytes */
#define BLE_ECU_IGN_MAP_UUID        0x0005
#define BLE_ECU_AFR_MAP_UUID        0x0006
#define BLE_ECU_CMD_UUID            0x0007  /* Write, 64 bytes */
#define BLE_ECU_FAULT_UUID          0x0008  /* Read+Notify, 8 bytes */
#define BLE_ECU_ACK_UUID            0x0009  /* Notify, 8 bytes */
#define BLE_ECU_DATALOG_UUID        0x000A  /* Notify, 64 bytes */
#define BLE_ECU_FWINFO_UUID         0x000B  /* Read, 16 bytes */
#define BLE_ECU_SENSOR_RAW_UUID     0x000C  /* Read+Notify, 32 bytes */

/* Número total de características (para alocar atributos no stack) */
#define BLE_ECU_NUM_CHARS           11

/* ================================================================
 * COMANDOS BLE
 * ================================================================ */
typedef enum {
    BLE_CMD_SET_PROFILE         = 0x01,
    BLE_CMD_GET_PROFILE         = 0x02,
    BLE_CMD_SET_VE_TABLE        = 0x03,
    BLE_CMD_GET_VE_TABLE        = 0x04,
    BLE_CMD_SET_IGN_TABLE       = 0x05,
    BLE_CMD_GET_IGN_TABLE       = 0x06,
    BLE_CMD_SET_AFR_TABLE       = 0x07,
    BLE_CMD_GET_AFR_TABLE       = 0x08,
    BLE_CMD_SET_CDI_TABLE       = 0x09,
    BLE_CMD_SAVE_TO_FLASH       = 0x10,
    BLE_CMD_RESET_TO_DEFAULT    = 0x11,
    BLE_CMD_CLEAR_FAULTS        = 0x12,
    BLE_CMD_START_AUTOTUNE      = 0x13,
    BLE_CMD_STOP_AUTOTUNE       = 0x14,
    BLE_CMD_SET_TELEM_RATE      = 0x15,
    BLE_CMD_SET_DATALOG_MODE    = 0x16,
    BLE_CMD_EMERGENCY_STOP      = 0x20,
    BLE_CMD_SET_FUEL_TRIM       = 0x21,
    BLE_CMD_SET_IGN_TRIM        = 0x22,
    BLE_CMD_SET_BOOST_TARGET    = 0x23,
    BLE_CMD_SET_IDLE_TARGET     = 0x24,
    BLE_CMD_SET_REV_LIMIT       = 0x25,
    BLE_CMD_SET_LAUNCH_RPM      = 0x26,
    BLE_CMD_CALIBRATE_TPS_MIN   = 0x30,
    BLE_CMD_CALIBRATE_TPS_MAX   = 0x31,
    BLE_CMD_CALIBRATE_MAP_ATMO  = 0x32,
    BLE_CMD_CALIBRATE_O2        = 0x33,
    BLE_CMD_CALIBRATE_FLEX      = 0x34,
    BLE_CMD_SET_CYLINDERS       = 0x40,
    BLE_CMD_SET_INJ_TYPE        = 0x41,
    BLE_CMD_SET_IGN_TYPE        = 0x42,
    BLE_CMD_SET_FUEL_TYPE       = 0x43,
    BLE_CMD_SET_VE_CELL         = 0x50,
    BLE_CMD_SET_IGN_CELL        = 0x51,
    BLE_CMD_SET_AFR_CELL        = 0x52,
    BLE_CMD_TEST_OUTPUT         = 0x60,
    BLE_CMD_GET_FIRMWARE_INFO   = 0xF0,
    BLE_CMD_GET_SENSOR_RAW      = 0xF1,
    BLE_CMD_PING                = 0xFF,
} BleCmdCode_t;

/* ================================================================
 * CÓDIGOS DE RESULTADO
 * ================================================================ */
typedef enum {
    BLE_RESULT_OK               = 0x00,
    BLE_RESULT_ERR              = 0x01,
    BLE_RESULT_INVALID_CMD      = 0x02,
    BLE_RESULT_INVALID_PAYLOAD  = 0x03,
    BLE_RESULT_BUSY             = 0x04,
    BLE_RESULT_ENGINE_RUNNING   = 0x05,
    BLE_RESULT_FLASH_ERR        = 0x06,
    BLE_RESULT_CRC_ERR          = 0x07,
    BLE_RESULT_CHUNK_ORDER      = 0x08,
    BLE_RESULT_RANGE            = 0x09,
    BLE_RESULT_NOT_CALIBRATED   = 0x0A,
    BLE_RESULT_TIMEOUT          = 0x0B,
    BLE_RESULT_NO_PROFILE       = 0x0C,
    BLE_RESULT_FATAL            = 0xFF,
} BleCmdResult_t;

/* map_type para BleChunk_t */
#define BLE_MAP_VE      0
#define BLE_MAP_IGN     1
#define BLE_MAP_AFR     2
#define BLE_MAP_CDI     3
#define BLE_MAP_PROFILE 4

/* Callbacks — implementados em ble_ecu_service.c */
void BLE_ECU_OnCommand(const BleCmdPacket_t *cmd);
void BLE_ECU_OnConfigWrite(const uint8_t *data, uint8_t len);
void BLE_ECU_OnVEMapWrite(const uint8_t *data, uint8_t len);
void BLE_ECU_OnIgnMapWrite(const uint8_t *data, uint8_t len);
void BLE_ECU_OnAFRMapWrite(const uint8_t *data, uint8_t len);

/* ================================================================
 * ALIASES — normalizam nomes entre protocolo e implementação
 * ================================================================ */
/* UUID aliases */
#define BLE_ECU_SVC_UUID16          BLE_ECU_SERVICE_UUID
#define BLE_ECU_CMD_UUID16          BLE_ECU_CMD_UUID
#define BLE_ECU_TELEM_UUID16        BLE_ECU_TELEM_UUID
#define BLE_ECU_FAULT_UUID16        BLE_ECU_FAULT_UUID
#define BLE_ECU_ACK_UUID16          BLE_ECU_ACK_UUID
#define BLE_ECU_SENSOR_RAW_UUID16   BLE_ECU_SENSOR_RAW_UUID

/* CMD aliases e extensões */
#define BLE_CMD_GET_INFO            BLE_CMD_GET_FIRMWARE_INFO
#define BLE_CMD_SAVE_FLASH          BLE_CMD_SAVE_TO_FLASH
#define BLE_CMD_LOAD_FLASH          ((BleCmdCode_t)0xE0)
#define BLE_CMD_WRITE_MAP           ((BleCmdCode_t)0xE1)
#define BLE_CMD_READ_MAP            ((BleCmdCode_t)0xE2)
#define BLE_CMD_SET_PARAM           ((BleCmdCode_t)0xE3)
#define BLE_CMD_GET_PARAM           ((BleCmdCode_t)0xE4)
#define BLE_CMD_START_TELEM         ((BleCmdCode_t)0xE5)
#define BLE_CMD_STOP_TELEM          ((BleCmdCode_t)0xE6)

/* Result aliases */
#define BLE_RESULT_ERR_LEN          BLE_RESULT_INVALID_PAYLOAD
#define BLE_RESULT_ERR_FLASH        BLE_RESULT_FLASH_ERR
#define BLE_RESULT_ERR_SEQ          BLE_RESULT_CHUNK_ORDER
#define BLE_RESULT_ERR_CMD          BLE_RESULT_INVALID_CMD
#define BLE_RESULT_CHUNK_ACK        ((BleCmdResult_t)0xC0)

/* Tamanho máximo de payload de comando */
#define BLE_CMD_MAX_LEN             64U

#endif /* BLE_PROTOCOL_H */
