/**
 * ble_wpan_api.h — Stub do STM32WB WPAN/BLE stack para compilação sem middleware.
 *
 * Para usar o stack real:
 *  1. Adicione o componente STM32CubeWB via extensão STM32Cube
 *  2. Adicione os include paths em CMakeLists.txt:
 *     ${STM32CUBEWB_PATH}/Middlewares/ST/STM32_WPAN/ble
 *     ${STM32CUBEWB_PATH}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread
 *  3. Defina BLE_WPAN_REAL antes de incluir este arquivo
 */
#ifndef BLE_WPAN_API_H
#define BLE_WPAN_API_H

#include <stdint.h>
#include <string.h>

/* ---- Tipos base ---- */
typedef uint8_t  tBleStatus;
#define BLE_STATUS_SUCCESS          0x00
#define BLE_STATUS_ERROR            0x47
#define BLE_STATUS_FAILED           0x41

/* ---- UUID ---- */
#define UUID_TYPE_16    0x01
#define UUID_TYPE_128   0x02

typedef struct { uint16_t UUID_16; }            Service_UUID_t;
typedef struct { uint16_t Char_UUID_16; }        Char_UUID_t;

/* ---- Service / Characteristic constants ---- */
#define PRIMARY_SERVICE                         0x01
#define SECONDARY_SERVICE                       0x02
#define GATT_NOTIFY_ATTRIBUTE_WRITE             0x01

#define ATTR_PERMISSION_NONE                    0x00
#define ATTR_PERMISSION_AUTHENTICATED_WRITE     0x08

#define CHAR_PROP_BROADCAST                     0x01
#define CHAR_PROP_READ                          0x02
#define CHAR_PROP_WRITE_WITHOUT_RESP            0x04
#define CHAR_PROP_WRITE                         0x08
#define CHAR_PROP_NOTIFY                        0x10
#define CHAR_PROP_INDICATE                      0x20

/* ---- Advertising ---- */
#define ADV_IND                                 0x00
#define PUBLIC_ADDR                             0x00
#define NO_WHITE_LIST_USE                       0x00
#define AD_TYPE_FLAGS                           0x01
#define AD_TYPE_COMPLETE_LOCAL_NAME             0x09
#define AD_TYPE_MANUFACTURER_SPECIFIC_DATA      0xFF
#define FLAG_BIT_LE_GENERAL_DISCOVERABLE_MODE   0x02
#define FLAG_BIT_BR_EDR_NOT_SUPPORTED           0x04

/* ---- GAP ---- */
#define GAP_PERIPHERAL_ROLE                     0x01
#define PRIVACY_DISABLED                        0x00

/* ---- SHCI (System Host Controller Interface) ---- */
#define SHCI_OPCODE_C2_BLE_INIT                 0xFC66

typedef struct {
    struct { uint16_t Type; } Header;
    struct {
        uint16_t NumAttrRecord;
        uint16_t NumAttrServ;
        uint16_t AttrValueArrSize;
        uint8_t  NumOfLinks;
        uint8_t  DataLengthExt;
        uint8_t  PrepareWriteListSize;
        uint8_t  MaxCoEvnt;
        uint32_t MaxConnEventLength;
        uint16_t SleepClockAccuracy;
        uint8_t  NumOfAdvDataSet;
        uint8_t  NumOfScAtt;
        uint8_t  MaxNumOfConnectionOrientedChannels;
        int8_t   MinTransmitPower;
        int8_t   MaxTransmitPower;
    } Param;
} SHCI_C2_Ble_Init_Cmd_Packet_t;

/* ---- Stub functions (NO-OP enquanto WPAN não estiver configurado) ---- */
static inline tBleStatus SHCI_C2_BLE_Init(SHCI_C2_Ble_Init_Cmd_Packet_t *p)
{ (void)p; return BLE_STATUS_SUCCESS; }

static inline tBleStatus aci_gap_init(uint8_t role, uint8_t privacy,
    uint8_t name_len, uint16_t *svc, uint16_t *dev, uint16_t *app)
{ (void)role; (void)privacy; (void)name_len;
  *svc = 0; *dev = 0; *app = 0; return BLE_STATUS_SUCCESS; }

static inline tBleStatus aci_gatt_add_service(uint8_t uuid_type,
    const Service_UUID_t *uuid, uint8_t type, uint8_t max_attr, uint16_t *h)
{ (void)uuid_type; (void)uuid; (void)type; (void)max_attr; *h = 0x0001; return BLE_STATUS_SUCCESS; }

static inline tBleStatus aci_gatt_add_char(uint16_t svc_h, uint8_t uuid_type,
    const Char_UUID_t *uuid, uint16_t max_len, uint8_t props, uint8_t perm,
    uint8_t evt, uint8_t encr, uint8_t var_len, uint16_t *h)
{
    static uint16_t s_h = 0x0002;
    (void)svc_h; (void)uuid_type; (void)uuid; (void)max_len;
    (void)props; (void)perm; (void)evt; (void)encr; (void)var_len;
    *h = s_h; s_h += 2; return BLE_STATUS_SUCCESS;
}

static inline tBleStatus aci_gatt_update_char_value(uint16_t svc_h,
    uint16_t char_h, uint8_t offset, uint8_t len, const uint8_t *data)
{ (void)svc_h; (void)char_h; (void)offset; (void)len; (void)data; return BLE_STATUS_SUCCESS; }

static inline tBleStatus aci_hal_set_tx_power_level(uint8_t en, uint8_t level)
{ (void)en; (void)level; return BLE_STATUS_SUCCESS; }

static inline tBleStatus aci_gap_set_discoverable(uint8_t adv_type,
    uint16_t int_min, uint16_t int_max, uint8_t addr_type, uint8_t filter,
    uint8_t name_len, const uint8_t *name, uint8_t num_groups,
    const void *groups, uint16_t slave_min, uint16_t slave_max)
{ (void)adv_type; (void)int_min; (void)int_max; (void)addr_type; (void)filter;
  (void)name_len; (void)name; (void)num_groups; (void)groups;
  (void)slave_min; (void)slave_max; return BLE_STATUS_SUCCESS; }

static inline tBleStatus aci_gap_set_non_discoverable(void)
{ return BLE_STATUS_SUCCESS; }

static inline void hci_user_evt_proc(void) {}

#endif /* BLE_WPAN_API_H */
