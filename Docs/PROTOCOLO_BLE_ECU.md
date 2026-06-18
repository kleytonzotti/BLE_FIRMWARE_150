# PROTOCOLO BLE — ECU PROGRAMÁVEL UNIVERSAL
**Versão:** 2.0  
**Data:** 2026-06-17  
**Aplicação:** Implementar no app Android/iOS, em outro hardware BLE, ou em painel externo  
**Stack BLE alvo:** BLE 4.2 e superior (5.0 preferencial)

---

## ÍNDICE
1. [Visão Geral](#1-visão-geral)
2. [Identificação do Dispositivo](#2-identificação-do-dispositivo)
3. [Serviço GATT — ECU Service](#3-serviço-gatt--ecu-service)
4. [Característica de Telemetria](#4-característica-de-telemetria-notify)
5. [Característica de Comandos](#5-característica-de-comandos-write)
6. [Característica de Configuração](#6-característica-de-configuração-readwrite)
7. [Transferência de Tabelas (Chunking)](#7-transferência-de-tabelas-chunking)
8. [Característica de Falhas](#8-característica-de-falhas-readnotify)
9. [Tabela Completa de Comandos](#9-tabela-completa-de-comandos)
10. [Fluxo de Conexão](#10-fluxo-de-conexão)
11. [Sequência de Operações Típicas](#11-sequência-de-operações-típicas)
12. [Códigos de Erro](#12-códigos-de-erro)
13. [Implementação em C (Registradores)](#13-implementação-em-c-registradores)
14. [Implementação em Android / iOS](#14-implementação-em-android--ios)

---

## 1. VISÃO GERAL

O protocolo ECU BLE usa **GATT** (Generic Attribute Profile) sobre BLE.

### Conceitos Chave

| Termo | O que é |
|-------|---------|
| GATT | Protocolo de camada de aplicação BLE para trocar dados estruturados |
| Service | Agrupamento lógico de características relacionadas |
| Characteristic | Um dado específico (ex: telemetria, comando) |
| Notify | ECU envia dado ao app sem o app pedir (push) |
| Read | App lê dado da ECU (pull) |
| Write | App escreve dado na ECU |
| MTU | Maximum Transmission Unit — tamanho máximo de um pacote BLE (default 23B, negociável até 512B) |
| CCCD | Client Characteristic Configuration Descriptor — o app escreve 0x0001 aqui para habilitar Notify |

### Topologia

```
┌─────────────────────┐         ┌───────────────────────────┐
│   App (Central)     │◄──BLE──►│   ECU (Peripheral)        │
│   Android / iOS     │         │   STM32WB55               │
│   Outro Hardware    │         │   Anunciando: "ECU-PRO"   │
└─────────────────────┘         └───────────────────────────┘

Fluxo de dados:
  ECU  →  App:  TELEMETRIA (Notify, automático a cada N ms)
  ECU  →  App:  FALHAS     (Notify, quando muda)
  App  →  ECU:  COMANDOS   (Write, sob demanda)
  App  ↔  ECU:  CONFIG     (Read/Write, configuração do veículo)
  App  ↔  ECU:  TABELAS    (Read/Write em chunks, mapas VE e ignição)
```

---

## 2. IDENTIFICAÇÃO DO DISPOSITIVO

### Advertising Packet

A ECU anuncia continuamente com os seguintes dados:

```
Nome completo (GAP):   "ECU-PRO"
Nome curto:            "ECU"

Manufacturer Specific Data (tipo 0xFF):
  Byte 0: 0xEC  (Company ID LSB — fabricante personalizado)
  Byte 1: 0x01  (Company ID MSB)
  Byte 2: VERSION_MAJOR    (ex: 0x03 = versão 3)
  Byte 3: VERSION_MINOR    (ex: 0x00)
  Byte 4: VEHICLE_TYPE     (0x00=moto, 0x01=carro)
  Byte 5: CYLINDERS        (0x01 a 0x04)
  Byte 6: STATUS_FLAGS     (bit0=motor ligado, bit1=falha ativa, bit2=BLE conectado)
  Byte 7: RSSI_TX_POWER    (potência de transmissão: 0x00=-20dBm, 0x06=0dBm)

Service UUID 128-bit:  00000001-EC01-1234-5678-ABCDEF000000
```

### Como Filtrar no App

```
// Android (Java/Kotlin)
scanFilter = new ScanFilter.Builder()
    .setServiceUuid(ParcelUuid.fromString("00000001-EC01-1234-5678-ABCDEF000000"))
    .build();

// iOS (Swift)
centralManager.scanForPeripherals(
    withServices: [CBUUID(string: "00000001-EC01-1234-5678-ABCDEF000000")],
    options: nil
)
```

---

## 3. SERVIÇO GATT — ECU SERVICE

### UUID Base (128-bit)

```
Base: 00000000-EC01-1234-5678-ABCDEF000000
      ^^^^^^^^                              ← 16-bit UUID vai aqui
```

Os UUIDs de 128-bit são derivados trocando os 4 primeiros bytes:

### Tabela de Características

| Nome | UUID 128-bit | UUID 16-bit | Propriedades | Tamanho | MTU Mín |
|------|-------------|-------------|--------------|---------|---------|
| ECU Service | `00000001-EC01-...` | `0x0001` | — | — | — |
| **Telemetria** | `00000002-EC01-...` | `0x0002` | **Notify** | 40 bytes | 44 |
| **Configuração** | `00000003-EC01-...` | `0x0003` | Read + Write | 64 bytes | 68 |
| **Mapa VE** | `00000004-EC01-...` | `0x0004` | Read + Write | 64 bytes/chunk | 68 |
| **Mapa Ign** | `00000005-EC01-...` | `0x0005` | Read + Write | 64 bytes/chunk | 68 |
| **Mapa AFR** | `00000006-EC01-...` | `0x0006` | Read + Write | 64 bytes/chunk | 68 |
| **Comando** | `00000007-EC01-...` | `0x0007` | Write | 64 bytes | 68 |
| **Falhas** | `00000008-EC01-...` | `0x0008` | Read + Notify | 8 bytes | 12 |
| **ACK/Resposta** | `00000009-EC01-...` | `0x0009` | Notify | 8 bytes | 12 |
| **Datalog** | `0000000A-EC01-...` | `0x000A` | Notify | 64 bytes | 68 |
| **Firmware Info** | `0000000B-EC01-...` | `0x000B` | Read | 16 bytes | 20 |
| **Sensor Raw** | `0000000C-EC01-...` | `0x000C` | Read + Notify | 32 bytes | 36 |

### MTU Recomendado

Negociar MTU de **100 bytes ou mais** logo após conexão.  
Com MTU = 100: payload máximo = 97 bytes (100 − 3 bytes de header ATT).

---

## 4. CARACTERÍSTICA DE TELEMETRIA (Notify)

**UUID:** `00000002-EC01-1234-5678-ABCDEF000000`  
**Formato:** struct C de **40 bytes**, little-endian  
**Periodicidade:** Configurável via comando `CMD_SET_TELEM_RATE` (padrão: 100ms)  
**Ativar:** Escrever `0x01 0x00` no CCCD (+2 do handle da característica)

### Estrutura do Pacote (40 bytes)

```c
/* Implementação C — exatamente assim na ECU e no parser do app */
typedef struct __attribute__((packed)) {
    /* Bytes 00-01 */ uint16_t rpm;            /* RPM atual (0-15000) */
    /* Bytes 02-03 */ uint16_t tps_pct_x10;   /* TPS × 10 → 0-1000 = 0.0-100.0% */
    /* Bytes 04-05 */ uint16_t map_kpa_x10;   /* MAP × 10 → ex: 1013 = 101.3 kPa */
    /* Bytes 06-07 */ int16_t  coolant_c_x10; /* CLT × 10 → ex: 850 = 85.0°C */
    /* Bytes 08-09 */ int16_t  iat_c_x10;     /* IAT × 10 → ex: 320 = 32.0°C */
    /* Bytes 10-11 */ uint16_t batt_mv;        /* Tensão bateria em mV → 13800 = 13.8V */
    /* Bytes 12-13 */ uint16_t o2_afr_x10;    /* AFR × 10 → 147 = 14.7 */
    /* Bytes 14-15 */ uint16_t knock_level;    /* Nível knock 0-1000 */
    /* Bytes 16-17 */ uint16_t vss_kmh;        /* Velocidade km/h */
    /* Bytes 18-19 */ uint16_t boost_kpa_x10; /* Boost × 10 → ex: 1500 = 150.0 kPa (1 bar boost) */
    /* Bytes 20-21 */ int16_t  ign_adv_x10;   /* Avanço ignição × 10 → 280 = 28.0°BTDC */
    /* Bytes 22-23 */ uint16_t inj_pw1_us;    /* Pulse width injetor 1 em µs */
    /* Bytes 24-25 */ uint16_t inj_pw2_us;    /* Pulse width injetor 2 em µs */
    /* Bytes 26-27 */ uint16_t flex_pct_x10;  /* % etanol × 10 → 550 = 55.0% etanol */
    /* Bytes 28-29 */ uint16_t oil_kpa;        /* Pressão óleo em kPa */
    /* Bytes 30-31 */ uint16_t fuel_kpa;       /* Pressão combustível em kPa */
    /* Byte  32    */ uint8_t  engine_state;   /* EngineStateMachine_t (0-7) */
    /* Byte  33    */ uint8_t  limp_mode;      /* 0=normal, 1=limp home ativo */
    /* Bytes 34-35 */ uint16_t egt_c;          /* Temperatura escapamento °C */
    /* Bytes 36-39 */ uint32_t active_faults;  /* Bitmask falhas ativas */
} BleTelemPacket_t; /* sizeof = 40 bytes */
```

### Tabela de Estados do Motor (engine_state)

| Valor | Estado | Descrição |
|-------|--------|-----------|
| 0x00 | OFF | Motor desligado |
| 0x01 | CRANKING | Partida — girando sem combustão |
| 0x02 | RUNNING | Funcionando normalmente |
| 0x03 | IDLE | Marcha lenta |
| 0x04 | DECEL | Desaceleração (corte combustível) |
| 0x05 | REV_LIMIT | Limitador RPM ativo |
| 0x06 | LIMP | Modo proteção (sensor falhou) |
| 0x07 | FAULT | Falha grave — motor parado |
| 0x08 | CRANKING_CDI | Partida CDI (modo carburado) |
| 0x09 | PRIMING | Priming da bomba (2s inicial) |

### Como Parsear no App (Kotlin)

```kotlin
fun parseTelemetry(data: ByteArray): TelemData {
    val buf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
    return TelemData(
        rpm         = buf.getShort(0).toInt() and 0xFFFF,
        tps         = (buf.getShort(2).toInt() and 0xFFFF) / 10.0f,   // %
        map         = (buf.getShort(4).toInt() and 0xFFFF) / 10.0f,   // kPa
        coolantTemp = buf.getShort(6).toFloat() / 10.0f,               // °C
        iatTemp     = buf.getShort(8).toFloat() / 10.0f,               // °C
        battMv      = buf.getShort(10).toInt() and 0xFFFF,             // mV
        afr         = (buf.getShort(12).toInt() and 0xFFFF) / 10.0f,  // AFR
        knock       = buf.getShort(14).toInt() and 0xFFFF,
        speed       = buf.getShort(16).toInt() and 0xFFFF,             // km/h
        boost       = (buf.getShort(18).toInt() and 0xFFFF) / 10.0f - 101.3f, // bar
        ignAdv      = buf.getShort(20).toFloat() / 10.0f,              // °BTDC
        injPw1      = buf.getShort(22).toInt() and 0xFFFF,             // µs
        injPw2      = buf.getShort(24).toInt() and 0xFFFF,
        flex        = (buf.getShort(26).toInt() and 0xFFFF) / 10.0f,  // %
        oilPres     = buf.getShort(28).toInt() and 0xFFFF,             // kPa
        fuelPres    = buf.getShort(30).toInt() and 0xFFFF,
        engineState = buf.get(32).toInt() and 0xFF,
        limpMode    = buf.get(33).toInt() and 0xFF,
        egt         = buf.getShort(34).toInt() and 0xFFFF,             // °C
        faults      = buf.getInt(36)
    )
}
```

### Como Parsear no App (Swift / iOS)

```swift
func parseTelemetry(_ data: Data) -> TelemData {
    var t = TelemData()
    data.withUnsafeBytes { ptr in
        t.rpm         = UInt16(littleEndian: ptr.load(fromByteOffset:  0, as: UInt16.self))
        t.tps         = Float(UInt16(littleEndian: ptr.load(fromByteOffset: 2, as: UInt16.self))) / 10.0
        t.map         = Float(UInt16(littleEndian: ptr.load(fromByteOffset: 4, as: UInt16.self))) / 10.0
        t.coolantTemp = Float(Int16(littleEndian: ptr.load(fromByteOffset:  6, as: Int16.self))) / 10.0
        t.iatTemp     = Float(Int16(littleEndian: ptr.load(fromByteOffset:  8, as: Int16.self))) / 10.0
        t.battMv      = UInt16(littleEndian: ptr.load(fromByteOffset: 10, as: UInt16.self))
        t.afr         = Float(UInt16(littleEndian: ptr.load(fromByteOffset: 12, as: UInt16.self))) / 10.0
        t.engineState = ptr.load(fromByteOffset: 32, as: UInt8.self)
        t.faults      = UInt32(littleEndian: ptr.load(fromByteOffset: 36, as: UInt32.self))
    }
    return t
}
```

---

## 5. CARACTERÍSTICA DE COMANDOS (Write)

**UUID:** `00000007-EC01-1234-5678-ABCDEF000000`  
**Propriedade:** Write Without Response (para velocidade) ou Write (para garantia)  
**Tamanho máximo:** 64 bytes

### Estrutura do Pacote de Comando

```c
typedef struct __attribute__((packed)) {
    uint8_t  cmd;        /* Código do comando (BleCmdCode_t) */
    uint8_t  seq;        /* Número de sequência 0-255 (para ACK matching) */
    uint8_t  len;        /* Tamanho do payload em bytes (0-61) */
    uint8_t  payload[61]; /* Dados do comando */
} BleCmdPacket_t; /* sizeof = 64 bytes */
```

### Pacote de ACK / Resposta

A ECU retorna na característica ACK (`0x0009`) após cada comando:

```c
typedef struct __attribute__((packed)) {
    uint8_t  cmd;        /* Eco do comando recebido */
    uint8_t  seq;        /* Eco do número de sequência */
    uint8_t  result;     /* 0x00=OK, 0x01=ERR, 0x02=INVALID, 0x03=BUSY */
    uint8_t  len;        /* Tamanho dos dados de retorno */
    uint8_t  data[4];    /* Dados de retorno (depende do comando) */
} BleCmdAck_t; /* sizeof = 8 bytes */
```

---

## 6. CARACTERÍSTICA DE CONFIGURAÇÃO (Read/Write)

**UUID:** `00000003-EC01-1234-5678-ABCDEF000000`  
**Read:** App solicita, ECU responde com perfil atual (em chunks se > MTU)  
**Write:** App envia novo perfil (pode ser parcial — só os campos que mudaram)

### Estrutura do Perfil (EcuProfile_t)

```c
/* Perfil completo do veículo — salvo em flash */
typedef struct __attribute__((packed)) {

    /* === IDENTIFICAÇÃO === */
    uint32_t magic;                /* 0xECU04321 — valida que é um perfil válido */
    uint8_t  fw_major;             /* Versão do firmware que gerou este perfil */
    uint8_t  fw_minor;
    uint8_t  fw_patch;
    uint8_t  profile_version;      /* Versão do layout do perfil (para migração) */
    char     vehicle_name[16];     /* Nome do veículo (ex: "CG 150 Titan") */

    /* === TIPO DE VEÍCULO === */
    uint8_t  vehicle_type;         /* 0=moto, 1=carro */
    uint8_t  cylinders;            /* 1-4 */
    uint16_t displacement_cc;      /* Cilindrada em cc */
    uint8_t  injection_type;       /* 0=sequencial, 1=batch, 2=semiseq */
    uint8_t  ignition_type;        /* 0=indutiva, 1=CDI, 2=DIS */
    uint8_t  fuel_type;            /* 0=gasolina, 1=etanol, 2=flex */
    uint8_t  load_type;            /* 0=MAP(speed-density), 1=TPS(alpha-N), 2=híbrido */

    /* === INJETORES === */
    uint16_t injector_flow_cc_min; /* Vazão do injetor em cc/min (ex: 260) */
    uint16_t injector_dead_time_us;/* Tempo morto do injetor em µs (ex: 800) */
    uint16_t fuel_pressure_kpa;    /* Pressão do combustível em kPa (ex: 300) */
    uint8_t  injector_angle[4];    /* Ângulo BTDC de abertura por cilindro (graus) */

    /* === IGNIÇÃO === */
    uint8_t  coil_dwell_ms;        /* Dwell padrão da bobina em ms (ex: 3) */
    int8_t   ign_base_advance_deg; /* Avanço base °BTDC (ex: 10) */
    uint8_t  crank_teeth;          /* Dentes no disco (ex: 60) */
    uint8_t  crank_missing_teeth;  /* Dentes faltando (ex: 2 → disco 60-2) */
    uint8_t  cam_teeth;            /* Dentes no comando (ex: 1) */
    uint8_t  ign_sequence[4];      /* Ordem de ignição (ex: [1,3,4,2] para 4 cil) */

    /* === LIMITES === */
    uint16_t rev_limit_rpm;        /* Limitador de rotação (ex: 10000) */
    uint16_t launch_rpm;           /* RPM de launch control (ex: 4000) */
    uint16_t idle_target_rpm;      /* Rotação alvo de idle (ex: 1200) */
    uint8_t  max_coolant_temp_c;   /* Temp máxima do motor °C (ex: 110) */
    uint16_t boost_target_kpa;     /* Boost alvo em kPa (ex: 1500 = 150kPa = 0.5bar) */

    /* === SENSORES — CALIBRAÇÃO === */
    uint16_t tps_min_mv;           /* TPS fechado em mV (ex: 500) */
    uint16_t tps_max_mv;           /* TPS aberto em mV (ex: 4500) */
    uint16_t map_sensor_min_mv;    /* MAP mínimo em mV (ex: 200) */
    uint16_t map_sensor_max_mv;    /* MAP máximo em mV (ex: 4800) */
    uint16_t map_sensor_max_kpa;   /* Pressão máxima do MAP em kPa (ex: 250) */
    uint16_t o2_min_mv;            /* Lambda mín mV (para analógico linear) */
    uint16_t o2_max_mv;            /* Lambda máx mV */
    float    o2_min_afr;           /* AFR mínimo da sonda (ex: 10.0) */
    float    o2_max_afr;           /* AFR máximo da sonda (ex: 20.0) */

    /* === TRIMS GLOBAIS === */
    int8_t   global_fuel_trim;     /* Trim combustível -50 a +50% */
    int8_t   global_ign_trim;      /* Trim ignição -20 a +20° */
    int8_t   global_boost_trim;    /* Trim boost -20 a +20 kPa */

    /* === IAC (MARCHA LENTA) === */
    uint8_t  iac_type;             /* 0=stepper, 1=duty solenóide, 2=nenhum */
    uint8_t  iac_steps_max;        /* Passos máximos do stepper (ex: 120) */
    int16_t  idle_p_gain;          /* Ganho P do PID × 100 */
    int16_t  idle_i_gain;          /* Ganho I do PID × 100 */
    int16_t  idle_d_gain;          /* Ganho D do PID × 100 */

    /* === FEATURES FLAGS === */
    uint32_t features;             /* Bitmask de funcionalidades ativas */
    /* bit0:  WIDEBAND_O2     bit8:  LAUNCH_CONTROL
       bit1:  KNOCK_SENSOR    bit9:  TRACTION_CONTROL
       bit2:  FLEX_FUEL       bit10: FLAT_SHIFT
       bit3:  BOOST_CTRL      bit11: AUTOTUNE
       bit4:  IAC_CTRL        bit12: DATALOG_BLE
       bit5:  VSS             bit13: CAN_DIAG
       bit6:  EGT             bit14: CDI_MODE
       bit7:  REV_LIMITER     bit15: ANTILAG */

    /* === TABELAS (16×16) — salvas inline no perfil === */
    uint8_t  fuel_ve_table[16][16];         /* 256B — eficiência volumétrica 0-200 */
    int8_t   ign_advance_table[16][16];     /* 256B — avanço ignição -20 a +60° */
    uint8_t  afr_target_table[16][16];      /* 256B — AFR alvo × 10 (ex: 147 = 14.7) */

    /* === TABELA CDI (para motor carburado) === */
    uint16_t cdi_advance_table[32];         /*  64B — avanço CDI por RPM */
    uint16_t cdi_rpm_table[32];             /*  64B — pontos de RPM da tabela CDI */

    /* === CORREÇÕES POR TEMPERATURA === */
    int8_t   coolant_fuel_corr[10];         /*  10B — correção comb por temp CLT */
    int8_t   iat_fuel_corr[10];             /*  10B — correção comb por temp IAT */
    int8_t   coolant_ign_corr[10];          /*  10B — correção ignição por CLT */
    int8_t   flex_fuel_corr[10];            /*  10B — correção comb por % etanol */
    int8_t   flex_ign_corr[10];             /*  10B — correção ignição por % etanol */

    /* === EIXOS DAS TABELAS === */
    uint16_t rpm_axis[16];                  /*  32B — eixo RPM das tabelas */
    uint8_t  load_axis[16];                 /*  16B — eixo carga 0-100% */

    /* === INTEGRIDADE === */
    uint32_t crc32;                         /* CRC32 de todo o struct exceto magic+crc */
} EcuProfile_t;
/* sizeof ≈ 1100 bytes — transferido em 18 chunks de 64 bytes via BLE */
```

---

## 7. TRANSFERÊNCIA DE TABELAS (CHUNKING)

Tabelas de 256 bytes (16×16) são transferidas em **4 chunks de 64 bytes** cada.  
O perfil completo (~1100 bytes) é transferido em **18 chunks de 64 bytes**.

### Protocolo de Chunk

```c
/* Cabeçalho de chunk — sempre 4 bytes antes do dado */
typedef struct __attribute__((packed)) {
    uint8_t chunk_id;       /* Número do chunk: 0, 1, 2, 3 ... */
    uint8_t total_chunks;   /* Total de chunks para este dado */
    uint8_t map_type;       /* Qual tabela: 0=VE, 1=IGN, 2=AFR, 3=CDI, 4=PROFILE */
    uint8_t reserved;       /* 0x00 */
    uint8_t data[60];       /* 60 bytes de dado (64 total - 4 header) */
} BleChunk_t;
```

### Fluxo de Escrita de Tabela (App → ECU)

```
App                               ECU
 |                                  |
 |── CMD: START_TABLE_WRITE ──────►| Inicia recepção
 |◄─ ACK: OK ──────────────────────|
 |                                  |
 |── CHUNK 0 (bytes 0-59) ────────►| Armazena em buffer temporário
 |◄─ ACK: OK ──────────────────────|
 |                                  |
 |── CHUNK 1 (bytes 60-119) ──────►|
 |◄─ ACK: OK ──────────────────────|
 |                                  |
 |── CHUNK 2 (bytes 120-179) ─────►|
 |◄─ ACK: OK ──────────────────────|
 |                                  |
 |── CHUNK 3 (bytes 180-239) ─────►| Último chunk — valida CRC
 |◄─ ACK: TABLE_WRITTEN ───────────|
 |                                  |
 |── CMD: SAVE_TO_FLASH ──────────►| (opcional — salva permanentemente)
 |◄─ ACK: SAVED ───────────────────|
```

### Fluxo de Leitura de Tabela (App ← ECU)

```
App                               ECU
 |                                  |
 |── CMD: GET_VE_TABLE ────────────►|
 |                                  |
 |◄─ CHUNK 0 (bytes 0-59) ─────────| (via characteristic notify ou read)
 |── ACK: OK ───────────────────── ►|
 |                                  |
 |◄─ CHUNK 1 (bytes 60-119) ───────|
 |── ACK: OK ──────────────────────►|
 |                                  |
 |◄─ CHUNK 2 (bytes 120-179) ──────|
 |◄─ CHUNK 3 (bytes 180-239) ──────| Fim
```

---

## 8. CARACTERÍSTICA DE FALHAS (Read/Notify)

**UUID:** `00000008-EC01-1234-5678-ABCDEF000000`  
**Notify:** Ativado automaticamente quando uma falha muda de estado  
**Tamanho:** 8 bytes

```c
typedef struct __attribute__((packed)) {
    uint32_t active_faults;   /* Falhas ATIVAS agora (bitmask) */
    uint32_t stored_faults;   /* Falhas históricas (persistem mesmo após reset) */
} BleFaultPacket_t;
```

### Códigos de Falha (Bitmask)

| Bit | Código OBD | Falha | Ação da ECU |
|-----|-----------|-------|-------------|
| 0 | P0120 | TPS circuito aberto/curto | Limp mode — usa TPS = 15% fixo |
| 1 | P0121 | TPS fora de faixa | Limp mode |
| 2 | P0105 | MAP circuito aberto/curto | Limp mode — usa MAP = 100kPa fixo |
| 3 | P0106 | MAP fora de faixa | Limp mode |
| 4 | P0115 | CLT circuito aberto/curto | Usa CLT = 80°C (temperatura padrão) |
| 5 | P0116 | CLT fora de faixa | Usa CLT = 80°C |
| 6 | P0110 | IAT circuito aberto/curto | Usa IAT = 25°C |
| 7 | P0111 | IAT fora de faixa | Usa IAT = 25°C |
| 8 | P0130 | Lambda circuito aberto/curto | Desativa closed-loop |
| 9 | P0133 | Lambda resposta lenta | Desativa closed-loop |
| 10 | P0325 | Knock circuito aberto | Desativa controle de knock |
| 11 | P0562 | Bateria baixa (< 9.5V) | Alerta, não interfere |
| 12 | P0563 | Bateria alta (> 16V) | Alerta, não interfere |
| 13 | P0201 | Injetor 1 circuito aberto | Corta cilindro 1 |
| 14 | P0202 | Injetor 2 circuito aberto | Corta cilindro 2 |
| 15 | P0203 | Injetor 3 circuito aberto | Corta cilindro 3 |
| 16 | P0204 | Injetor 4 circuito aberto | Corta cilindro 4 |
| 17 | P0351 | Bobina 1 primário falhou | Corta ignição 1 |
| 18 | P0352 | Bobina 2 primário falhou | Corta ignição 2 |
| 19 | P0353 | Bobina 3 primário falhou | Corta ignição 3 |
| 20 | P0354 | Bobina 4 primário falhou | Corta ignição 4 |
| 21 | P0335 | CKP sinal ausente | Para motor (CRÍTICO) |
| 22 | P0340 | CMP sinal ausente | Modo batch (sem sequencial) |
| 23 | P0336 | Sincronismo perdido | Tenta ressincronizar |
| 24 | P0217 | Motor superaquecido (> max_temp) | Corte de combustível |
| 25 | P0324 | Detonação severa | Recua avanço -15° emergência |
| 26 | P0087 | Pressão combustível baixa | Alerta + limp mode |
| 27 | — | Limitador RPM ativo | Informativo |
| 28 | P0600 | CAN bus off | Reinicia CAN |
| 29 | — | Erro de flash (config corrompida) | Usa perfil padrão |
| 30 | — | Reset por watchdog | Informativo |
| 31 | — | Falha interna ECU | Para motor (CRÍTICO) |

---

## 9. TABELA COMPLETA DE COMANDOS

### Estrutura geral do campo `payload[]` para cada comando:

---

### 0x01 — CMD_SET_PROFILE
**Descrição:** Define perfil completo do veículo  
**Payload:** *(usar chunking — ver seção 7)*  
```
payload[0-60]: chunk de dados do EcuProfile_t
```
**ACK:** `result=0x00` quando todos os chunks recebidos e CRC válido

---

### 0x02 — CMD_GET_PROFILE
**Descrição:** Solicita perfil atual da ECU  
**Payload:** vazio (len=0)  
**Resposta:** ECU envia EcuProfile_t em chunks pela característica CONFIG (0x0003)

---

### 0x03 — CMD_SET_VE_TABLE
**Descrição:** Escreve tabela VE 16×16 em chunks  
**Payload:**
```
payload[0]: chunk_id (0-3)
payload[1]: total_chunks (4)
payload[2-61]: 60 bytes de dado da tabela
```

---

### 0x04 — CMD_GET_VE_TABLE
**Payload:** vazio  
**Resposta:** 4 chunks de 60 bytes pela característica VE_MAP (0x0004)

---

### 0x05 — CMD_SET_IGN_TABLE
**Payload:** igual ao 0x03 (chunks da tabela de ignição)

---

### 0x06 — CMD_GET_IGN_TABLE
**Payload:** vazio. Resposta via IGN_MAP (0x0005)

---

### 0x07 — CMD_SET_AFR_TABLE
**Payload:** chunks da tabela de AFR alvo

---

### 0x08 — CMD_GET_AFR_TABLE
**Payload:** vazio. Resposta via AFR_MAP (0x0006)

---

### 0x09 — CMD_SET_CDI_TABLE
**Descrição:** Define curva de avanço CDI (32 pontos RPM×Avanço)  
**Payload:**
```
payload[0]:    chunk_id (0-1, pois 64 bytes em 2 chunks)
payload[1]:    total_chunks (2)
payload[2-61]: 30 pontos uint16_t (RPM ou graus alternado)
```
**Formato da tabela CDI (64 bytes = 32 pares RPM/avanço):**
```c
struct { uint16_t rpm; uint16_t advance_deg_x10; } cdi_points[16]; /* por chunk */
```

---

### 0x10 — CMD_SAVE_TO_FLASH
**Descrição:** Persiste configuração atual na flash  
**Payload:** vazio  
**ACK:** `result=0x00` quando flash gravada e verificada  
**Atenção:** ECU pode demorar até 200ms para gravar. Motor continua funcionando.

---

### 0x11 — CMD_RESET_TO_DEFAULT
**Descrição:** Carrega perfil padrão de fábrica para um veículo  
**Payload:**
```
payload[0]: vehicle_id (VehicleProfileId_t)
```
**IDs de veículo:**
| ID | Veículo |
|----|---------|
| 0x00 | GENERICO_1CIL_MOTO |
| 0x01 | GENERICO_2CIL_MOTO |
| 0x02 | GENERICO_4CIL_CARRO |
| 0x03 | GENERICO_4CIL_TURBO |
| 0x10 | HONDA_CG150 |
| 0x11 | HONDA_CG160 |
| 0x12 | HONDA_CB160 |
| 0x13 | HONDA_TITAN160_FLEX |
| 0x14 | HONDA_PCX150 |
| 0x20 | YAMAHA_YBR125 |
| 0x21 | YAMAHA_FACTOR150 |
| 0x30 | CDI_GENERICO_1CIL |
| 0x31 | CDI_HONDA_CG150 |

---

### 0x12 — CMD_CLEAR_FAULTS
**Payload:** vazio  
**Ação:** Limpa `stored_faults`. Falhas ativas só somem quando o problema físico for resolvido.

---

### 0x13 — CMD_START_AUTOTUNE
**Descrição:** Inicia rotina de autotune (ajuste automático da tabela VE baseado no lambda)  
**Payload:**
```
payload[0]: autotune_mode (0=suave, 1=agressivo)
payload[1]: target_afr_x10 (ex: 147 = 14.7)
```

---

### 0x14 — CMD_STOP_AUTOTUNE
**Payload:** vazio

---

### 0x15 — CMD_SET_TELEM_RATE
**Descrição:** Configura periodicidade do notify de telemetria  
**Payload:**
```
payload[0-1]: period_ms (uint16_t, little-endian) — mínimo 20ms, máximo 5000ms
```
**Exemplos:** 50ms = `0x32 0x00`, 100ms = `0x64 0x00`, 1000ms = `0xE8 0x03`

---

### 0x16 — CMD_SET_DATALOG_MODE
**Payload:**
```
payload[0]: mode (0=desativado, 1=BLE notify, 2=flash interna, 3=BLE+flash)
payload[1-2]: sample_rate_ms (uint16_t)
```

---

### 0x20 — CMD_EMERGENCY_STOP
**Descrição:** Corta combustível e ignição imediatamente  
**Payload:** vazio  
**ACK:** imediato, motor já cortado antes do ACK chegar  
**Para religar:** CMD_RESET_TO_DEFAULT ou religar a ignição

---

### 0x21 — CMD_SET_FUEL_TRIM
**Descrição:** Ajusta trim global de combustível  
**Payload:**
```
payload[0]: trim (int8_t) — -50 a +50 (= -50% a +50%)
```

---

### 0x22 — CMD_SET_IGN_TRIM
**Payload:**
```
payload[0]: trim (int8_t) — -20 a +20 (graus)
```

---

### 0x23 — CMD_SET_BOOST_TARGET
**Payload:**
```
payload[0-1]: target_kpa (uint16_t, little-endian) — kPa absoluto (ex: 1500 = 150kPa = 0.5bar boost)
```

---

### 0x24 — CMD_SET_IDLE_TARGET
**Payload:**
```
payload[0-1]: idle_rpm (uint16_t) — ex: 1200
```

---

### 0x25 — CMD_SET_REV_LIMIT
**Payload:**
```
payload[0-1]: rev_limit_rpm (uint16_t)
```

---

### 0x26 — CMD_SET_LAUNCH_RPM
**Payload:**
```
payload[0-1]: launch_rpm (uint16_t)
```

---

### 0x30 — CMD_CALIBRATE_TPS_MIN
**Descrição:** Captura posição mínima do TPS (acelerador fechado)  
**Payload:** vazio — ECU lê TPS agora e salva como mínimo  
**Procedimento:** App envia este comando com pedal fechado

---

### 0x31 — CMD_CALIBRATE_TPS_MAX
**Payload:** vazio — ECU lê TPS agora e salva como máximo  
**Procedimento:** App envia com pedal totalmente aberto

---

### 0x32 — CMD_CALIBRATE_MAP_ATMO
**Descrição:** Calibra MAP para pressão atmosférica atual  
**Payload:** 
```
payload[0-1]: local_pressure_kpa (uint16_t) — pressão local em kPa × 10 (ex: 1013 = 101.3kPa)
              Se 0x0000, ECU usa a leitura atual como referência atmosférica
```

---

### 0x33 — CMD_CALIBRATE_O2
**Payload:** vazio — reinicia controlador CJ125 e refaz aquecimento da sonda

---

### 0x34 — CMD_CALIBRATE_FLEX
**Payload:** 
```
payload[0]: known_ethanol_pct (0-100) — informa a % real de etanol no tanque para calibração
```

---

### 0x40 — CMD_SET_CYLINDER_COUNT
**Payload:**
```
payload[0]: cylinders (1-4)
```

---

### 0x41 — CMD_SET_INJECTION_TYPE
**Payload:**
```
payload[0]: type (0=sequencial, 1=batch, 2=semi-sequencial)
```

---

### 0x42 — CMD_SET_IGNITION_TYPE
**Payload:**
```
payload[0]: type (0=indutiva, 1=CDI, 2=DIS)
```

---

### 0x43 — CMD_SET_FUEL_TYPE
**Payload:**
```
payload[0]: type (0=gasolina, 1=etanol, 2=flex)
```

---

### 0x50 — CMD_SET_VE_CELL
**Descrição:** Modifica UMA célula da tabela VE (para tuning célula a célula)  
**Payload:**
```
payload[0]: row (0-15) — índice de RPM
payload[1]: col (0-15) — índice de carga
payload[2]: value (0-200) — VE em % (100 = 100%)
```

---

### 0x51 — CMD_SET_IGN_CELL
**Payload:**
```
payload[0]: row (0-15)
payload[1]: col (0-15)
payload[2]: value (int8_t) — graus BTDC (-20 a +60)
```

---

### 0x52 — CMD_SET_AFR_CELL
**Payload:**
```
payload[0]: row (0-15)
payload[1]: col (0-15)
payload[2]: value — AFR × 10 (ex: 147 = 14.7)
```

---

### 0x60 — CMD_TEST_OUTPUT
**Descrição:** Ativa uma saída para teste (motor deve estar parado)  
**Payload:**
```
payload[0]: output_id:
  0x01 = INJ1       0x02 = INJ2       0x03 = INJ3       0x04 = INJ4
  0x11 = IGN1       0x12 = IGN2       0x13 = IGN3       0x14 = IGN4
  0x21 = FUEL_PUMP  0x22 = FAN        0x23 = IAC        0x24 = WASTEGATE
  0x31 = CDI1       0x32 = CDI2
payload[1]: action (0=desativa, 1=ativa, 2=pulso único 100ms)
payload[2]: duty_pct (0-100, para saídas PWM)
```

---

### 0xF0 — CMD_GET_FIRMWARE_INFO
**Payload:** vazio  
**Resposta** (via ACK data[]):
```c
typedef struct __attribute__((packed)) {
    uint8_t  major;
    uint8_t  minor;
    uint8_t  patch;
    uint8_t  vehicle_type;
    uint8_t  cylinders;
    uint16_t displacement_cc;
    uint8_t  features_hi;
    uint8_t  features_lo;
    char     build_date[6];  /* "AAMMDD" */
} BleFwInfo_t; /* 16 bytes, cabe no ACK */
```

---

### 0xF1 — CMD_GET_SENSOR_RAW
**Descrição:** Retorna valores brutos dos ADCs para diagnóstico  
**Payload:** vazio  
**Resposta** (via característica SENSOR_RAW 0x000C, 32 bytes):
```c
typedef struct __attribute__((packed)) {
    uint16_t tps_mv;
    uint16_t map_mv;
    uint16_t clt_mv;
    uint16_t iat_mv;
    uint16_t batt_mv;
    uint16_t o2_mv;
    uint16_t knock_mv;
    uint16_t flex_hz;   /* frequência do sensor flex em Hz */
    uint16_t boost_mv;
    uint16_t oil_mv;
    uint16_t fuel_mv;
    uint16_t egt_raw;
    uint32_t ckp_period_us;
    uint32_t reserved;
} BleSensorRaw_t; /* 32 bytes */
```

---

### 0xFF — CMD_PING
**Payload:** vazio  
**ACK:** `result=0x00, data[0]=0xEA, data[1]=0xCU` — confirma que ECU está respondendo

---

## 10. FLUXO DE CONEXÃO

### Sequência Obrigatória ao Conectar

```
1. App escaneia → encontra dispositivo com Service UUID ECU
2. App conecta (GAP connect)
3. App negocia MTU: requestMtu(100) ou maior
4. App descobre serviços GATT
5. App habilita Notify na telemetria:
   → Escreve 0x01 0x00 no handle+2 (CCCD) da característica TELEMETRIA
6. App habilita Notify nas falhas:
   → Escreve 0x01 0x00 no handle+2 da característica FALHAS
7. App habilita Notify no ACK:
   → Escreve 0x01 0x00 no handle+2 da ACK_RESPONSE
8. App envia CMD_GET_FIRMWARE_INFO (0xF0) → obtém versão e tipo de veículo
9. App envia CMD_GET_PROFILE (0x02) → obtém configuração atual
10. ECU começa a enviar telemetria automaticamente
```

### Comportamento ao Desconectar

```
ECU:
- Mantém a última configuração em RAM
- Continua rodando normalmente
- Se flash foi salva: reinicia com a mesma config
- Se não foi salva: reinicia com config da flash (pode ser anterior)
- Volta a anunciar via BLE para nova conexão
```

---

## 11. SEQUÊNCIA DE OPERAÇÕES TÍPICAS

### Alterar uma célula da tabela VE e salvar

```
App → ECU: CMD_SET_VE_CELL [row=5, col=8, value=92]  (seq=0x01)
ECU → App: ACK [cmd=0x50, seq=0x01, result=0x00]

App → ECU: CMD_SAVE_TO_FLASH []  (seq=0x02)
ECU → App: ACK [cmd=0x10, seq=0x02, result=0x00] após ~150ms
```

### Carregar uma tabela VE completa

```
App → ECU: CMD_START_TABLE_WRITE [map_type=0]  (opcional — indica início)
App → ECU: Chunk 0: [0x00, 0x04, 0x00, 0x00, <60 bytes dados>]
ECU → App: ACK [result=0x00]
App → ECU: Chunk 1: [0x01, 0x04, 0x00, 0x00, <60 bytes dados>]
ECU → App: ACK [result=0x00]
App → ECU: Chunk 2: [0x02, 0x04, 0x00, 0x00, <60 bytes dados>]
ECU → App: ACK [result=0x00]
App → ECU: Chunk 3: [0x03, 0x04, 0x00, 0x00, <60 bytes dados>]  ← último
ECU → App: ACK [result=0x00, data[0]=TABLE_WRITE_OK]
App → ECU: CMD_SAVE_TO_FLASH
ECU → App: ACK [result=0x00]
```

### Calibrar TPS

```
Procedimento:
1. Manter pedal fechado
   App → ECU: CMD_CALIBRATE_TPS_MIN []
   ECU → App: ACK [result=0x00, data[0-1]=valor_mv_lido]

2. Pressionar pedal totalmente
   App → ECU: CMD_CALIBRATE_TPS_MAX []
   ECU → App: ACK [result=0x00, data[0-1]=valor_mv_lido]

3. Salvar
   App → ECU: CMD_SAVE_TO_FLASH []
```

---

## 12. CÓDIGOS DE ERRO

| result | Código | Significado |
|--------|--------|-------------|
| 0x00 | OK | Sucesso |
| 0x01 | ERR_GENERIC | Erro genérico |
| 0x02 | ERR_INVALID_CMD | Comando desconhecido |
| 0x03 | ERR_INVALID_PAYLOAD | Payload inválido (tamanho, range) |
| 0x04 | ERR_BUSY | ECU ocupada (ex: gravando flash) |
| 0x05 | ERR_ENGINE_RUNNING | Operação não permitida com motor ligado |
| 0x06 | ERR_FLASH_WRITE | Falha ao gravar flash |
| 0x07 | ERR_CRC | CRC do chunk não bateu |
| 0x08 | ERR_CHUNK_ORDER | Chunk fora de ordem (esperava outro chunk_id) |
| 0x09 | ERR_RANGE | Valor fora da faixa permitida |
| 0x0A | ERR_NOT_CALIBRATED | Sensor não calibrado para esta operação |
| 0x0B | ERR_TIMEOUT | Timeout aguardando recurso |
| 0x0C | ERR_NO_PROFILE | Nenhum perfil válido carregado |
| 0xFF | ERR_FATAL | Falha grave — reiniciar ECU |

---

## 13. IMPLEMENTAÇÃO EM C (REGISTRADORES)

### Definição das características GATT no STM32WB (sem HAL)

```c
/* Registro do serviço ECU via ACI (Application Command Interface do stack BLE ST) */

/* UUIDs em formato little-endian para o stack BLE ST */
static const uint8_t ECU_SVC_UUID[16] = {
    0x00, 0x00, 0x00, 0xEF, 0xCD, 0xAB, 0x78, 0x56,
    0x34, 0x12, 0x01, 0xEC, 0x01, 0x00, 0x00, 0x00
};

/* Registra o serviço e todas as características */
static tBleStatus ECU_GATT_Register(void)
{
    tBleStatus ret;
    Char_UUID_t char_uuid;
    Service_UUID_t svc_uuid;

    /* Registra serviço primário */
    memcpy(svc_uuid.Service_UUID_128, ECU_SVC_UUID, 16);
    svc_uuid.Service_UUID_128[12] = 0x01; /* ECU Service = 0x0001 */
    svc_uuid.Service_UUID_128[13] = 0x00;

    ret = aci_gatt_add_service(UUID_TYPE_128, &svc_uuid,
                               PRIMARY_SERVICE,
                               30,              /* max attribute records */
                               &g_svc_handle);
    if (ret != BLE_STATUS_SUCCESS) return ret;

    /* === TELEMETRIA === Notify, 40 bytes */
    memcpy(char_uuid.Char_UUID_128, ECU_SVC_UUID, 16);
    char_uuid.Char_UUID_128[12] = 0x02; char_uuid.Char_UUID_128[13] = 0x00;
    ret = aci_gatt_add_char(g_svc_handle, UUID_TYPE_128, &char_uuid,
                             40,                  /* max len */
                             CHAR_PROP_NOTIFY,
                             ATTR_PERMISSION_NONE,
                             GATT_NOTIFY_ATTRIBUTE_WRITE,
                             10, 1,               /* enc key, var len */
                             &g_telem_handle);
    if (ret != BLE_STATUS_SUCCESS) return ret;

    /* === COMANDO === Write (without response para velocidade) */
    char_uuid.Char_UUID_128[12] = 0x07; char_uuid.Char_UUID_128[13] = 0x00;
    ret = aci_gatt_add_char(g_svc_handle, UUID_TYPE_128, &char_uuid,
                             64,
                             CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP,
                             ATTR_PERMISSION_NONE,
                             GATT_NOTIFY_ATTRIBUTE_WRITE,
                             10, 1,
                             &g_cmd_handle);
    if (ret != BLE_STATUS_SUCCESS) return ret;

    /* ... registrar demais características ... */

    return BLE_STATUS_SUCCESS;
}

/* Envia telemetria — chamado do main loop a cada N ms */
void ECU_BLE_SendTelemetry(const volatile EngineState_t *eng,
                            EngineStateMachine_t state)
{
    static BleTelemPacket_t pkt;

    /* Preenche sem alocação dinâmica */
    pkt.rpm            = eng->rpm;
    pkt.tps_pct_x10   = eng->tps_pct;
    pkt.map_kpa_x10   = eng->map_kpa;
    pkt.coolant_c_x10 = eng->coolant_temp_c;
    pkt.iat_c_x10     = eng->iat_c;
    pkt.batt_mv        = eng->batt_mv;
    pkt.o2_afr_x10    = eng->o2_afr_x10;
    pkt.knock_level    = eng->knock_level;
    pkt.vss_kmh        = eng->vss_kmh;
    pkt.boost_kpa_x10 = eng->boost_kpa;
    pkt.ign_adv_x10   = eng->ign_advance_deg;
    pkt.inj_pw1_us     = (uint16_t)(eng->pulse_width_us[0] > 65535
                                     ? 65535 : eng->pulse_width_us[0]);
    pkt.inj_pw2_us     = (uint16_t)(eng->pulse_width_us[1] > 65535
                                     ? 65535 : eng->pulse_width_us[1]);
    pkt.flex_pct_x10  = eng->flex_pct;
    pkt.oil_kpa        = eng->oil_pressure_kpa;
    pkt.fuel_kpa       = eng->fuel_pressure_kpa;
    pkt.engine_state   = (uint8_t)state;
    pkt.limp_mode      = eng->limp_mode;
    pkt.egt_c          = eng->egt_c;
    pkt.active_faults  = eng->active_faults;

    /* Atualiza característica — stack BLE envia no próximo connection event */
    aci_gatt_update_char_value(g_svc_handle, g_telem_handle,
                                0, sizeof(BleTelemPacket_t),
                                (const uint8_t *)&pkt);
}

/* Processa comando recebido — chamado pelo evento GATT do stack BLE */
void ECU_BLE_OnCmdWrite(const uint8_t *data, uint8_t len)
{
    if (len < 3) return; /* mínimo: cmd + seq + len */

    const BleCmdPacket_t *cmd = (const BleCmdPacket_t *)data;
    BleCmdAck_t ack = { .cmd = cmd->cmd, .seq = cmd->seq,
                         .result = 0x00, .len = 0 };

    switch (cmd->cmd) {
        case 0x10: /* CMD_SAVE_TO_FLASH */
            if (Flash_SaveProfile(&g_profile) == ECU_OK) {
                ack.result = 0x00;
            } else {
                ack.result = 0x06; /* ERR_FLASH_WRITE */
            }
            break;

        case 0x15: /* CMD_SET_TELEM_RATE */
            if (cmd->len >= 2) {
                uint16_t rate = (uint16_t)(cmd->payload[0] | (cmd->payload[1] << 8));
                if (rate < 20) rate = 20;
                if (rate > 5000) rate = 5000;
                g_ble_telem_rate_ms = rate;
                ack.result = 0x00;
            } else {
                ack.result = 0x03; /* ERR_INVALID_PAYLOAD */
            }
            break;

        case 0x21: /* CMD_SET_FUEL_TRIM */
            if (cmd->len >= 1) {
                int8_t trim = (int8_t)cmd->payload[0];
                if (trim < -50 || trim > 50) { ack.result = 0x09; break; }
                g_profile.global_fuel_trim = trim;
                ack.result = 0x00;
            }
            break;

        case 0x50: /* CMD_SET_VE_CELL */
            if (cmd->len >= 3) {
                uint8_t row = cmd->payload[0];
                uint8_t col = cmd->payload[1];
                uint8_t val = cmd->payload[2];
                if (row > 15 || col > 15) { ack.result = 0x09; break; }
                g_profile.fuel_ve_table[row][col] = val;
                ack.result = 0x00;
            }
            break;

        case 0xFF: /* CMD_PING */
            ack.result = 0x00;
            ack.data[0] = 0xEA;
            ack.data[1] = 0xCU; /* ECU alive */
            ack.len = 2;
            break;

        default:
            ack.result = 0x02; /* ERR_INVALID_CMD */
            break;
    }

    /* Envia ACK via característica ACK_RESPONSE (0x0009) */
    aci_gatt_update_char_value(g_svc_handle, g_ack_handle,
                                0, sizeof(BleCmdAck_t),
                                (const uint8_t *)&ack);
}
```

---

## 14. IMPLEMENTAÇÃO EM ANDROID / iOS

### Android (Kotlin) — Estrutura básica

```kotlin
class EcuBleManager(context: Context) {

    companion object {
        /* UUIDs — copiar exatamente estes valores */
        val ECU_SVC   = UUID.fromString("00000001-EC01-1234-5678-ABCDEF000000")
        val TELEM     = UUID.fromString("00000002-EC01-1234-5678-ABCDEF000000")
        val CONFIG    = UUID.fromString("00000003-EC01-1234-5678-ABCDEF000000")
        val VE_MAP    = UUID.fromString("00000004-EC01-1234-5678-ABCDEF000000")
        val IGN_MAP   = UUID.fromString("00000005-EC01-1234-5678-ABCDEF000000")
        val AFR_MAP   = UUID.fromString("00000006-EC01-1234-5678-ABCDEF000000")
        val CMD       = UUID.fromString("00000007-EC01-1234-5678-ABCDEF000000")
        val FAULTS    = UUID.fromString("00000008-EC01-1234-5678-ABCDEF000000")
        val ACK       = UUID.fromString("00000009-EC01-1234-5678-ABCDEF000000")
        val CCCD      = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB") /* Standard CCCD */
    }

    private var seqNum: Byte = 0

    /* Constrói pacote de comando */
    fun buildCmd(cmdCode: Byte, payload: ByteArray = byteArrayOf()): ByteArray {
        val pkt = ByteArray(3 + payload.size.coerceAtMost(61))
        pkt[0] = cmdCode
        pkt[1] = seqNum++
        pkt[2] = payload.size.coerceAtMost(61).toByte()
        payload.copyInto(pkt, 3, 0, pkt[2].toInt())
        return pkt
    }

    /* Envia comando para ECU */
    fun sendCommand(gatt: BluetoothGatt, cmdCode: Byte, payload: ByteArray = byteArrayOf()) {
        val char = gatt.getService(ECU_SVC)?.getCharacteristic(CMD) ?: return
        char.value = buildCmd(cmdCode, payload)
        char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        gatt.writeCharacteristic(char)
    }

    /* Salva na flash */
    fun saveToFlash(gatt: BluetoothGatt) = sendCommand(gatt, 0x10)

    /* Define trim de combustível */
    fun setFuelTrim(gatt: BluetoothGatt, trim: Int) {
        sendCommand(gatt, 0x21, byteArrayOf(trim.toByte()))
    }

    /* Define taxa de telemetria */
    fun setTelemRate(gatt: BluetoothGatt, rateMs: Int) {
        val payload = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN)
            .putShort(rateMs.toShort()).array()
        sendCommand(gatt, 0x15, payload)
    }

    /* Habilita notify de telemetria */
    fun enableTelemNotify(gatt: BluetoothGatt) {
        val char = gatt.getService(ECU_SVC)?.getCharacteristic(TELEM) ?: return
        gatt.setCharacteristicNotification(char, true)
        val desc = char.getDescriptor(CCCD)
        desc.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        gatt.writeDescriptor(desc)
    }

    /* Callback de notify — parseia telemetria */
    fun onCharacteristicChanged(char: BluetoothGattCharacteristic) {
        when (char.uuid) {
            TELEM  -> handleTelemetry(char.value)
            FAULTS -> handleFaults(char.value)
            ACK    -> handleAck(char.value)
        }
    }

    private fun handleTelemetry(data: ByteArray) {
        if (data.size < 40) return
        val telem = parseTelemetry(data)
        /* Atualiza UI */
    }
}
```

### iOS (Swift) — Constantes e parse

```swift
struct EcuBLE {
    static let serviceUUID  = CBUUID(string: "00000001-EC01-1234-5678-ABCDEF000000")
    static let telemUUID    = CBUUID(string: "00000002-EC01-1234-5678-ABCDEF000000")
    static let configUUID   = CBUUID(string: "00000003-EC01-1234-5678-ABCDEF000000")
    static let veMapUUID    = CBUUID(string: "00000004-EC01-1234-5678-ABCDEF000000")
    static let ignMapUUID   = CBUUID(string: "00000005-EC01-1234-5678-ABCDEF000000")
    static let afrMapUUID   = CBUUID(string: "00000006-EC01-1234-5678-ABCDEF000000")
    static let cmdUUID      = CBUUID(string: "00000007-EC01-1234-5678-ABCDEF000000")
    static let faultsUUID   = CBUUID(string: "00000008-EC01-1234-5678-ABCDEF000000")
    static let ackUUID      = CBUUID(string: "00000009-EC01-1234-5678-ABCDEF000000")

    static var seqNum: UInt8 = 0

    static func buildCmd(_ code: UInt8, payload: Data = Data()) -> Data {
        var pkt = Data(count: 3 + min(payload.count, 61))
        pkt[0] = code
        pkt[1] = seqNum; seqNum &+= 1
        pkt[2] = UInt8(min(payload.count, 61))
        pkt.replaceSubrange(3..., with: payload.prefix(61))
        return pkt
    }
}
```

---

*Documento de protocolo — Use este arquivo como referência única de implementação.*  
*Versão do protocolo deve ser verificada em CMD_GET_FIRMWARE_INFO antes de qualquer operação.*  
*Arquivos relacionados: `ESCOPO_ECU_PROGRAMAVEL.md`*
