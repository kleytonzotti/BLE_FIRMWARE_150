/**
 * app_ble.c — Integração com o stack BLE do STM32WB (WPAN).
 *
 * O STM32WB55 tem dois núcleos:
 *   CPU1 (Cortex-M4): aplicação (este código)
 *   CPU2 (Cortex-M0+): firmware BLE (carregado via IPCC)
 *
 * Comunicação: IPCC (Inter-Process Communication Controller) + shared RAM
 * Configuração padrão:
 *   Nome do dispositivo: "ECU_BLE"
 *   Advertising: 100ms interval, connectable undirected
 *   GATT: serviço personalizado (12 características)
 *
 * Após conexão, o app usa as funções de ble_ecu_service.c para notificar.
 */

#include "app_ble.h"
#include "ble_ecu_service.h"
#include "ble_protocol.h"
#include "ecu_config.h"
#include <string.h>

/* BLE WPAN types incluídos via ble_ecu_service.h → ble_wpan_api.h */

/* Advertising data */
static const uint8_t s_adv_data[] = {
    /* Flags: LE General Discoverable + BR/EDR Not Supported */
    2, AD_TYPE_FLAGS, FLAG_BIT_LE_GENERAL_DISCOVERABLE_MODE | FLAG_BIT_BR_EDR_NOT_SUPPORTED,
    /* Complete local name */
    8, AD_TYPE_COMPLETE_LOCAL_NAME, 'E','C','U','_','B','L','E',
    /* Manufacturer specific (Vendor ID) */
    3, AD_TYPE_MANUFACTURER_SPECIFIC_DATA, 0xEC, 0x01,
};

static uint16_t s_gap_service_handle   = 0;
static uint16_t s_dev_name_char_handle = 0;
static uint16_t s_appearance_char_handle = 0;

void APP_BLE_Init(void)
{
    tBleStatus ret;

    /* Configura clock HSE para RF (32MHz) */
    /* Habilitado pelo SystemInit gerado pelo STM32CubeMX */

    /* Inicializa BLE stack via SHCI */
    SHCI_C2_Ble_Init_Cmd_Packet_t ble_init_cmd = {0};
    ble_init_cmd.Header.Type = SHCI_OPCODE_C2_BLE_INIT;
    ble_init_cmd.Param.NumAttrRecord        = 68;
    ble_init_cmd.Param.NumAttrServ          = 8;
    ble_init_cmd.Param.AttrValueArrSize     = 1344;
    ble_init_cmd.Param.NumOfLinks           = 1;
    ble_init_cmd.Param.DataLengthExt        = 1;
    ble_init_cmd.Param.PrepareWriteListSize = 3;
    ble_init_cmd.Param.MaxCoEvnt            = 2;
    ble_init_cmd.Param.MaxConnEventLength   = 0xFFFFFFFF;
    ble_init_cmd.Param.SleepClockAccuracy   = 500;
    ble_init_cmd.Param.NumOfAdvDataSet      = 1;
    ble_init_cmd.Param.NumOfScAtt           = 0;
    ble_init_cmd.Param.MaxNumOfConnectionOrientedChannels = 0;
    ble_init_cmd.Param.MinTransmitPower     = -40;
    ble_init_cmd.Param.MaxTransmitPower     =   6;
    SHCI_C2_BLE_Init(&ble_init_cmd);

    /* GAP Init */
    ret = aci_gap_init(GAP_PERIPHERAL_ROLE, PRIVACY_DISABLED, 8,
                       &s_gap_service_handle,
                       &s_dev_name_char_handle,
                       &s_appearance_char_handle);
    if (ret != BLE_STATUS_SUCCESS) return;

    /* Nome do dispositivo */
    const char *name = "ECU_BLE";
    aci_gatt_update_char_value(s_gap_service_handle,
                                s_dev_name_char_handle,
                                0, (uint8_t)strlen(name), (const uint8_t *)name);

    /* TX Power: 0 dBm */
    aci_hal_set_tx_power_level(1, 0x19);

    /* Registra serviço GATT da ECU */
    BLE_ECU_Service_Init();

    /* Inicia advertising */
    APP_BLE_AdvStart();
}

void APP_BLE_AdvStart(void)
{
    aci_gap_set_discoverable(ADV_IND,
                              200,   /* interval min × 0.625ms = 125ms */
                              200,   /* interval max */
                              PUBLIC_ADDR,
                              NO_WHITE_LIST_USE,
                              strlen("ECU_BLE"),
                              (const uint8_t *)"ECU_BLE",
                              0, NULL,
                              0, 0);
}

void APP_BLE_AdvStop(void)
{
    aci_gap_set_non_discoverable();
}

/* ----------------------------------------------------------------
 * Callback do stack BLE (chamada no HCI event loop)
 * Implementado pelo usuário — nomes variam por versão do WPAN
 * ---------------------------------------------------------------- */
void hci_le_connection_complete_event(uint8_t status,
                                      uint16_t conn_handle,
                                      uint8_t role,
                                      uint8_t peer_addr_type,
                                      const uint8_t *peer_addr,
                                      uint16_t conn_interval,
                                      uint16_t conn_latency,
                                      uint16_t supervision_timeout,
                                      uint8_t master_clock_accuracy)
{
    (void)role; (void)peer_addr_type; (void)peer_addr;
    (void)conn_interval; (void)conn_latency;
    (void)supervision_timeout; (void)master_clock_accuracy;

    if (status == 0x00) {
        BLE_ECU_SetConnectionHandle(conn_handle);
    }
}

void hci_disconnection_complete_event(uint8_t status,
                                       uint16_t conn_handle,
                                       uint8_t reason)
{
    (void)status; (void)conn_handle; (void)reason;
    BLE_ECU_OnDisconnect();
    APP_BLE_AdvStart();  /* Reinicia advertising após desconexão */
}

void aci_gatt_attribute_modified_event(uint16_t conn_handle,
                                        uint16_t attr_handle,
                                        uint16_t offset,
                                        uint16_t attr_data_length,
                                        const uint8_t *attr_data)
{
    (void)conn_handle; (void)offset;
    BLE_ECU_GATT_Event(attr_handle, attr_data, (uint8_t)attr_data_length);
}

void APP_BLE_Process(void)
{
    /* Processa eventos pendentes do stack (polling — sem RTOS) */
    hci_user_evt_proc();
}
