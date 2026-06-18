/**
 * ble_ecu_service.c — Serviço GATT da ECU (BLE stack STM32WB WPAN).
 *
 * Usa aci_gatt_* / hci_* da STM32WB BLE stack (não HAL).
 * Todos os handles são descobertos no add_service e guardados em g_ecu_handles.
 *
 * Fluxo de dados:
 *   App → GATT Write → BLE_ECU_GATT_Event → cmd_dispatch → execute → Ack Notify
 *   ECU → Notify Telemetry (100ms) / Faults / SensorRaw
 *
 * Chunking para tabelas grandes (>60 bytes):
 *   CMD_WRITE_MAP: payload[0]=map_id, payload[1]=chunk_idx,
 *                  payload[2..]=64 bytes de dados
 *   Após 4 chunks → tabela completa → aplica e salva
 */

#include "ble_ecu_service.h"
#include "ble_protocol.h"
#include "ecu_engine.h"
#include "ecu_fuel.h"
#include "ecu_ignition.h"
#include "ecu_sensors.h"
#include "ecu_vehicles.h"
#include "ecu_diagnostics.h"
#include "drv_flash.h"
#include "ecu_config.h"
#include <string.h>

/* BLE WPAN types já incluídos via ble_ecu_service.h → ble_wpan_api.h */

/* ----------------------------------------------------------------
 * Estado global
 * ---------------------------------------------------------------- */
EcuGattHandles_t g_ecu_handles = {0};
static uint16_t  s_conn_handle  = 0xFFFF;
static uint8_t   s_connected    = 0;

extern uint32_t g_ble_telem_rate_ms;  /* Definido em ecu_engine.c */

/* Buffer de montagem de chunks (máx 256 bytes por tabela) */
static uint8_t  s_chunk_buf[256];
static uint8_t  s_chunk_map_id  = 0;
static uint8_t  s_chunks_recv   = 0;

/* UUID base da ECU: 00000000-EC01-1234-5678-ABCDEF000000 */
static const uint8_t s_uuid_base[16] = {
    0x00,0x00,0x00,0x00, 0xEF,0xCD, 0xAB,0x00,
    0x78,0x56, 0x34,0x12, 0x01,0xEC, 0x00,0x00
};

/* ----------------------------------------------------------------
 * Inicialização do serviço GATT
 * ---------------------------------------------------------------- */
tBleStatus BLE_ECU_Service_Init(void)
{
    tBleStatus ret;

    /* Registra UUID base */
    uint8_t uuid_base[16];
    memcpy(uuid_base, s_uuid_base, 16);

    /* Add custom service (128-bit UUID) */
    Service_UUID_t svc_uuid;
    svc_uuid.UUID_16 = BLE_ECU_SVC_UUID16;
    ret = aci_gatt_add_service(UUID_TYPE_128, &svc_uuid, PRIMARY_SERVICE,
                               BLE_ECU_MAX_ATTR_RECORDS, &g_ecu_handles.service);
    if (ret != BLE_STATUS_SUCCESS) return ret;

    /* Helper macro para adicionar característica */
#define ADD_CHAR(uuid16, props, perm, len, handle_ptr) \
    do { \
        Char_UUID_t char_uuid; \
        char_uuid.Char_UUID_16 = (uuid16); \
        ret = aci_gatt_add_char(g_ecu_handles.service, UUID_TYPE_128, \
                                &char_uuid, (len), (props), (perm), \
                                GATT_NOTIFY_ATTRIBUTE_WRITE, 16, 1, (handle_ptr)); \
        if (ret != BLE_STATUS_SUCCESS) return ret; \
    } while(0)

    /* Características de escrita (app → ECU) */
    ADD_CHAR(BLE_ECU_CMD_UUID16,
             CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP,
             ATTR_PERMISSION_AUTHENTICATED_WRITE,
             BLE_CMD_MAX_LEN,
             &g_ecu_handles.cmd_write);

    /* Características de notify (ECU → app) */
    ADD_CHAR(BLE_ECU_TELEM_UUID16,
             CHAR_PROP_NOTIFY,
             ATTR_PERMISSION_NONE,
             sizeof(BleTelemPacket_t),
             &g_ecu_handles.telem_notify);

    ADD_CHAR(BLE_ECU_FAULT_UUID16,
             CHAR_PROP_NOTIFY,
             ATTR_PERMISSION_NONE,
             sizeof(BleFaultPacket_t),
             &g_ecu_handles.fault_notify);

    ADD_CHAR(BLE_ECU_ACK_UUID16,
             CHAR_PROP_NOTIFY,
             ATTR_PERMISSION_NONE,
             sizeof(BleCmdAck_t),
             &g_ecu_handles.ack_notify);

    ADD_CHAR(BLE_ECU_SENSOR_RAW_UUID16,
             CHAR_PROP_NOTIFY,
             ATTR_PERMISSION_NONE,
             sizeof(BleSensorRaw_t),
             &g_ecu_handles.sensor_raw);

#undef ADD_CHAR

    return BLE_STATUS_SUCCESS;
}

/* ----------------------------------------------------------------
 * Notificações ECU → App
 * ---------------------------------------------------------------- */
tBleStatus BLE_ECU_Notify_Telemetry(const BleTelemPacket_t *pkt)
{
    if (!s_connected) return BLE_STATUS_SUCCESS;
    return aci_gatt_update_char_value(g_ecu_handles.service,
                                      g_ecu_handles.telem_notify,
                                      0, sizeof(BleTelemPacket_t),
                                      (const uint8_t *)pkt);
}

tBleStatus BLE_ECU_Notify_Faults(const BleFaultPacket_t *pkt)
{
    if (!s_connected) return BLE_STATUS_SUCCESS;
    return aci_gatt_update_char_value(g_ecu_handles.service,
                                      g_ecu_handles.fault_notify,
                                      0, sizeof(BleFaultPacket_t),
                                      (const uint8_t *)pkt);
}

tBleStatus BLE_ECU_Notify_Ack(const BleCmdAck_t *ack)
{
    if (!s_connected) return BLE_STATUS_SUCCESS;
    return aci_gatt_update_char_value(g_ecu_handles.service,
                                      g_ecu_handles.ack_notify,
                                      0, sizeof(BleCmdAck_t),
                                      (const uint8_t *)ack);
}

/* ----------------------------------------------------------------
 * Helpers para montar e enviar telemetria / falhas
 * ---------------------------------------------------------------- */
void BLE_ECU_SendTelemetry(const volatile EngineState_t *eng,
                            EngineStateMachine_t state)
{
    BleTelemPacket_t pkt = {0};

    pkt.rpm            = eng->rpm;
    pkt.tps_pct_x10    = (uint16_t)(eng->tps_pct);
    pkt.map_kpa_x10    = (uint16_t)(eng->map_kpa);
    pkt.coolant_c_x10  = (int16_t)(eng->coolant_c);
    pkt.iat_c_x10      = (int16_t)(eng->iat_c);
    pkt.batt_mv        = eng->batt_mv;
    pkt.o2_afr_x10     = eng->o2_afr_x10;
    pkt.ign_adv_x10    = (int16_t)(eng->ign_advance_deg);
    pkt.inj_pw1_us     = eng->pulse_width_us[0];
    pkt.boost_kpa_x10  = eng->boost_kpa;
    pkt.oil_kpa        = eng->oil_pressure_kpa;
    pkt.fuel_kpa       = eng->fuel_pressure_kpa;
    pkt.engine_state   = (uint8_t)state;
    pkt.active_faults  = eng->active_faults;
    pkt.flex_pct_x10   = eng->flex_pct;
    pkt.knock_level    = eng->knock_level;

    BLE_ECU_Notify_Telemetry(&pkt);
}

void BLE_ECU_SendFaults(const volatile EngineState_t *eng)
{
    BleFaultPacket_t pkt = {0};
    pkt.active_faults  = eng->active_faults;
    pkt.stored_faults  = eng->stored_faults;
    BLE_ECU_Notify_Faults(&pkt);
}

void BLE_ECU_SendSensorRaw(const volatile AdcData_t *adc)
{
    BleSensorRaw_t pkt = {0};
    pkt.tps_mv   = adc->mv[ADC_CH_TPS];
    pkt.map_mv   = adc->mv[ADC_CH_MAP];
    pkt.clt_mv   = adc->mv[ADC_CH_CLT];
    pkt.iat_mv   = adc->mv[ADC_CH_IAT];
    pkt.batt_mv  = adc->mv[ADC_CH_BATT];
    pkt.o2_mv    = adc->mv[ADC_CH_O2];
    pkt.knock_mv = adc->mv[ADC_CH_KNOCK];
    pkt.boost_mv = adc->mv[ADC_CH_BOOST];
    pkt.oil_mv   = adc->mv[ADC_CH_OIL];
    pkt.fuel_mv  = adc->mv[ADC_CH_FUEL_PRESS];
    pkt.egt_raw  = adc->mv[ADC_CH_EGT];
    if (!s_connected) return;
    aci_gatt_update_char_value(g_ecu_handles.service,
                                g_ecu_handles.sensor_raw,
                                0, sizeof(BleSensorRaw_t),
                                (const uint8_t *)&pkt);
}

/* ----------------------------------------------------------------
 * Dispatcher de comandos BLE
 * ---------------------------------------------------------------- */
static void _send_ack(uint8_t cmd, uint8_t seq, uint8_t result,
                      const uint8_t *data, uint8_t data_len)
{
    BleCmdAck_t ack = {0};
    ack.cmd    = cmd;
    ack.seq    = seq;
    ack.result = result;
    ack.len    = data_len;
    if (data && data_len <= 4) {
        for (uint8_t i = 0; i < data_len; i++) ack.data[i] = data[i];
    }
    BLE_ECU_Notify_Ack(&ack);
}

static void _cmd_dispatch(const BleCmdPacket_t *pkt)
{
    uint8_t  cmd = pkt->cmd;
    uint8_t  seq = pkt->seq;
    const uint8_t *payload = pkt->payload;
    uint8_t  len = pkt->len;

    switch (cmd) {
        case BLE_CMD_PING:
            _send_ack(cmd, seq, BLE_RESULT_OK, (uint8_t[]){0xEC,0x01,0x00,0x00}, 4);
            break;

        case BLE_CMD_GET_INFO: {
            FirmwareInfo_t info;
            ECU_Diag_GetFirmwareInfo(&info, &g_profile);
            uint8_t d[4] = {info.major, info.minor, info.patch, ECU_HW_REV};
            _send_ack(cmd, seq, BLE_RESULT_OK, d, 4);
            break;
        }

        case BLE_CMD_SET_PROFILE:
            if (len < 2) { _send_ack(cmd, seq, BLE_RESULT_ERR_LEN, NULL, 0); break; }
            ECU_LoadDefaultProfile((VehicleProfileId_t)payload[0]);
            _send_ack(cmd, seq, BLE_RESULT_OK, &payload[0], 1);
            break;

        case BLE_CMD_SAVE_FLASH:
            if (DRV_Flash_SaveProfile(&g_profile) == ECU_OK) {
                _send_ack(cmd, seq, BLE_RESULT_OK, NULL, 0);
            } else {
                _send_ack(cmd, seq, BLE_RESULT_ERR_FLASH, NULL, 0);
            }
            break;

        case BLE_CMD_LOAD_FLASH:
            if (DRV_Flash_LoadProfile(&g_profile) == ECU_OK) {
                _send_ack(cmd, seq, BLE_RESULT_OK, NULL, 0);
            } else {
                _send_ack(cmd, seq, BLE_RESULT_ERR_FLASH, NULL, 0);
            }
            break;

        case BLE_CMD_WRITE_MAP: {
            /* payload: [map_id][chunk_idx][data × 60] */
            if (len < 3) { _send_ack(cmd, seq, BLE_RESULT_ERR_LEN, NULL, 0); break; }
            uint8_t map_id    = payload[0];
            uint8_t chunk_idx = payload[1];
            uint8_t data_len  = (uint8_t)(len - 2);

            if (chunk_idx == 0) {
                s_chunk_map_id = map_id;
                s_chunks_recv  = 0;
                memset(s_chunk_buf, 0, sizeof(s_chunk_buf));
            }
            if (map_id != s_chunk_map_id) {
                _send_ack(cmd, seq, BLE_RESULT_ERR_SEQ, NULL, 0); break;
            }

            uint16_t offset = (uint16_t)(chunk_idx * 60);
            if ((uint16_t)(offset + data_len) <= 256) {
                memcpy(&s_chunk_buf[offset], &payload[2], data_len);
            }
            s_chunks_recv++;

            if (s_chunks_recv >= BLE_TABLE_CHUNK_COUNT) {
                /* Aplica tabela completa */
                switch (map_id) {
                    case BLE_MAP_VE:
                        memcpy(g_profile.fuel_ve_table, s_chunk_buf,
                               FUEL_TABLE_ROWS * FUEL_TABLE_COLS);
                        break;
                    case BLE_MAP_IGN:
                        memcpy(g_profile.ign_advance_table, s_chunk_buf,
                               IGN_TABLE_ROWS * IGN_TABLE_COLS);
                        break;
                    case BLE_MAP_AFR:
                        memcpy(g_profile.afr_target_table, s_chunk_buf,
                               AFR_TABLE_ROWS * AFR_TABLE_COLS);
                        break;
                    default:
                        _send_ack(cmd, seq, BLE_RESULT_ERR_CMD, NULL, 0);
                        return;
                }
                s_chunks_recv = 0;
                _send_ack(cmd, seq, BLE_RESULT_OK, &map_id, 1);
            } else {
                /* Aguarda próximo chunk */
                _send_ack(cmd, seq, BLE_RESULT_CHUNK_ACK, &chunk_idx, 1);
            }
            break;
        }

        case BLE_CMD_READ_MAP: {
            uint8_t map_id = payload[0];
            uint8_t *src = NULL;
            uint16_t map_size = FUEL_TABLE_ROWS * FUEL_TABLE_COLS;

            switch (map_id) {
                case BLE_MAP_VE:      src = (uint8_t *)g_profile.fuel_ve_table;     break;
                case BLE_MAP_IGN:     src = (uint8_t *)g_profile.ign_advance_table; break;
                case BLE_MAP_AFR:     src = (uint8_t *)g_profile.afr_target_table;  break;
                default: _send_ack(cmd, seq, BLE_RESULT_ERR_CMD, NULL, 0); return;
            }

            /* Envia 4 chunks de 60 bytes (último pode ser menor) */
            for (uint8_t i = 0; i < BLE_TABLE_CHUNK_COUNT; i++) {
                uint8_t buf[BLE_CHUNK_MAX_DATA + 2];
                buf[0] = map_id;
                buf[1] = i;
                uint8_t dlen = (uint8_t)ECU_MIN(BLE_CHUNK_MAX_DATA,
                                                  (int32_t)(map_size - i * BLE_CHUNK_MAX_DATA));
                memcpy(&buf[2], &src[i * BLE_CHUNK_MAX_DATA], dlen);
                aci_gatt_update_char_value(g_ecu_handles.service,
                                            g_ecu_handles.ack_notify,
                                            0, (uint8_t)(dlen + 2), buf);
            }
            _send_ack(cmd, seq, BLE_RESULT_OK, &map_id, 1);
            break;
        }

        case BLE_CMD_SET_PARAM: {
            /* payload: [param_id_lo][param_id_hi][value_lo][value_hi] */
            if (len < 4) { _send_ack(cmd, seq, BLE_RESULT_ERR_LEN, NULL, 0); break; }
            uint16_t param_id = (uint16_t)(payload[0] | (payload[1] << 8));
            int16_t  value    = (int16_t)(payload[2] | (payload[3] << 8));

            switch (param_id) {
                case 0x0001: g_profile.idle_target_rpm = (uint16_t)value; break;
                case 0x0002: g_profile.rev_limit_rpm   = (uint16_t)value; break;
                case 0x0003: g_profile.global_fuel_trim = (int8_t)value;  break;
                case 0x0004: g_profile.global_ign_trim  = (int8_t)value;  break;
                case 0x0005: g_profile.boost_target_kpa = (uint16_t)value; break;
                case 0x0006: g_profile.coil_dwell_ms   = (uint8_t)value;  break;
                case 0x0010: g_profile.tps_min_mv       = (uint16_t)value; break;
                case 0x0011: g_profile.tps_max_mv       = (uint16_t)value; break;
                case 0x0020: g_profile.map_min_mv       = (uint16_t)value; break;
                case 0x0021: g_profile.map_max_mv       = (uint16_t)value; break;
                case 0x00FF: g_ble_telem_rate_ms = (uint32_t)value;        break;
                default:
                    _send_ack(cmd, seq, BLE_RESULT_ERR_CMD, NULL, 0); return;
            }
            _send_ack(cmd, seq, BLE_RESULT_OK, payload, 4);
            break;
        }

        case BLE_CMD_GET_PARAM: {
            uint16_t param_id = (uint16_t)(payload[0] | (payload[1] << 8));
            int16_t  value = 0;
            switch (param_id) {
                case 0x0001: value = (int16_t)g_profile.idle_target_rpm; break;
                case 0x0002: value = (int16_t)g_profile.rev_limit_rpm;   break;
                case 0x0003: value = g_profile.global_fuel_trim;         break;
                case 0x0004: value = g_profile.global_ign_trim;          break;
                default: _send_ack(cmd, seq, BLE_RESULT_ERR_CMD, NULL, 0); return;
            }
            uint8_t d[2] = {(uint8_t)(value & 0xFF), (uint8_t)(value >> 8)};
            _send_ack(cmd, seq, BLE_RESULT_OK, d, 2);
            break;
        }

        case BLE_CMD_START_TELEM:
            s_connected = 1;
            _send_ack(cmd, seq, BLE_RESULT_OK, NULL, 0);
            break;

        case BLE_CMD_STOP_TELEM:
            s_connected = 0;
            _send_ack(cmd, seq, BLE_RESULT_OK, NULL, 0);
            break;

        case BLE_CMD_CLEAR_FAULTS:
            ECU_Diag_ClearAll(&g_engine);
            _send_ack(cmd, seq, BLE_RESULT_OK, NULL, 0);
            break;

        case BLE_CMD_EMERGENCY_STOP:
            ECU_Engine_EmergencyStop();
            _send_ack(cmd, seq, BLE_RESULT_OK, NULL, 0);
            break;

        default:
            _send_ack(cmd, seq, BLE_RESULT_ERR_CMD, NULL, 0);
            break;
    }
}

/* ----------------------------------------------------------------
 * Evento GATT (chamado pelo app_ble.c na notificação do stack)
 * ---------------------------------------------------------------- */
void BLE_ECU_GATT_Event(uint16_t handle, const uint8_t *data, uint8_t len)
{
    if (handle == (g_ecu_handles.cmd_write + 1)) {
        /* +1 porque o handle de valor é sempre handle_char + 1 */
        if (len < 3 || len > BLE_CMD_MAX_LEN) return;
        BleCmdPacket_t pkt = {0};
        pkt.cmd     = data[0];
        pkt.seq     = data[1];
        pkt.len     = data[2];
        if (len > 3 && pkt.len > 0) {
            uint8_t copy_len = (uint8_t)ECU_MIN(pkt.len, (int32_t)(len - 3));
            memcpy(pkt.payload, &data[3], copy_len);
        }
        _cmd_dispatch(&pkt);
    }
}

/* ----------------------------------------------------------------
 * Gerenciamento de conexão
 * ---------------------------------------------------------------- */
uint8_t BLE_ECU_IsConnected(void)
{
    return s_connected;
}

void BLE_ECU_SetConnectionHandle(uint16_t handle)
{
    s_conn_handle = handle;
    s_connected   = 1;
}

void BLE_ECU_OnDisconnect(void)
{
    s_conn_handle = 0xFFFF;
    s_connected   = 0;
    s_chunks_recv = 0;
}
