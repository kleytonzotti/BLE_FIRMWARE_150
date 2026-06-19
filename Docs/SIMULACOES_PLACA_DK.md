# SIMULAÇÕES — STM32WB5MM-DK
**Versão:** 1.0  
**Data:** 2026-06-19  
**Hardware:** STM32WB5MM Discovery Kit (STM32WB5MMG)

---

## Visão Geral

O módulo de simulação permite testar a ECU programável **sem nenhum sensor físico conectado**, usando apenas o hardware onboard da placa STM32WB5MM-DK:

| Hardware onboard | Função no simulador |
|-----------------|---------------------|
| OLED 128×64 (SSD1315) | Exibe o cenário atual e os parâmetros simulados |
| Tecla SW1 (PC4) | Avança para o próximo cenário |
| Tecla SW2 (PE4) | Volta para o cenário anterior |
| LED Verde (PB1) | Pisca ao pressionar SW1 |
| LED Azul (PB0)  | Pisca ao pressionar SW2 |
| LED Vermelho (PB5) | Aceso quando o cenário tem falha ativa |

---

## Conexão de Pinos (STM32WB5MM-DK)

```
STM32WB5MM-DK          Função
─────────────────────────────────────────────
PC0   I2C3_SCL         OLED SSD1315 — SCL
PC1   I2C3_SDA         OLED SSD1315 — SDA
PC4   GPIO Input        SW1 — Próximo cenário (ativo em nível baixo)
PE4   GPIO Input        SW2 — Cenário anterior (ativo em nível baixo)
PB0   GPIO Output       LED Azul   (feedback SW2)
PB1   GPIO Output       LED Verde  (feedback SW1)
PB5   GPIO Output       LED Vermelho (falha ativa)
─────────────────────────────────────────────
```

> **Nota sobre PC0/PC1:** Estes pinos são configurados como ADC analógico
> no modo ECU real (FLEX e KNOCK). O `DRV_OLED_Init()` reconfigura os
> registradores MODER automaticamente para GPIO saída — sem conflito pois
> não há sensores conectados durante o desenvolvimento.

---

## Layout do Display OLED

Fonte 6×8 px → **21 caracteres × 8 linhas** em 128×64 pixels.

```
┌─────────────────────┐
│SIM X/8: NOME________│  ← linha 0: número e nome do cenário
│RPM:XXXXX TPS:  XX%  │  ← linha 1: rotação e posição do acelerador
│MAP:XXXkPa CLT:XXXC  │  ← linha 2: pressão e temperatura
│AFR:XX.X  BAT:XX.XV  │  ← linha 3: lambda e bateria (ou CDI BTDC)
│ESTADO: XXXXXXXXXXX  │  ← linha 4: estado da máquina ECU
│INFO: XXXXXXXXXXXXXX │  ← linha 5: informação / alerta (invertida se falha)
│[SW1] PROXIMO        │  ← linha 6: dica de botão
│[SW2] ANTERIOR       │  ← linha 7: dica de botão
└─────────────────────┘
```

A **linha 5 é invertida** (texto branco em fundo preto) quando o cenário
tem falha ativa — destaque visual imediato.

---

## Cenários de Simulação

### SIM 1/8 — MARCHA LENTA (Idle quente)

| Parâmetro | Valor |
|-----------|-------|
| RPM | 800 |
| TPS | 0% |
| MAP | 30 kPa |
| CLT | 90°C |
| IAT | 35°C |
| Bateria | 13,8 V |
| AFR | 14,7 (estequiométrico) |
| Avanço | 12° BTDC |
| Estado ECU | MARCHA LENTA |
| Falhas | Nenhuma |

Verifica o controle de marcha lenta (IAC PID), eficiência volumétrica em
baixa carga e estabilidade do sinal BLE em loop.

---

### SIM 2/8 — ACELERAÇÃO PARCIAL

| Parâmetro | Valor |
|-----------|-------|
| RPM | 3.500 |
| TPS | 55% |
| MAP | 75 kPa |
| CLT | 90°C |
| AFR | 13,5 (rico na aceleração) |
| Avanço | 28° BTDC |
| Estado ECU | RODANDO |

Verifica a tabela VE em carga média, enriquecimento na aceleração (AE)
e responsividade do avanço de ignição.

---

### SIM 3/8 — CARGA PLENA

| Parâmetro | Valor |
|-----------|-------|
| RPM | 6.000 |
| TPS | 100% |
| MAP | 100 kPa |
| CLT | 95°C |
| AFR | 12,5 (potência máxima) |
| Knock | 50 (baixo) |
| Avanço | 32° BTDC |
| Estado ECU | RODANDO |

Verifica a tabela VE em carga plena, telemetria BLE sob alta taxa de dados
e limites de pulse width do injetor.

---

### SIM 4/8 — PARTIDA A FRIO

| Parâmetro | Valor |
|-----------|-------|
| RPM | 300 (cranking) |
| TPS | 5% |
| MAP | 25 kPa |
| **CLT** | **15°C** |
| IAT | 18°C |
| Bateria | 11,8 V (bateria fria) |
| AFR | 11,0 (muito rico — partida fria) |
| Avanço | 5° BTDC |
| Estado ECU | PARTIDA |

Verifica o enriquecimento de partida a frio (`clt_fuel_corr`), priming da
bomba e comportamento com bateria baixa (compensação de dead time do injetor).

---

### SIM 5/8 — SUPERAQUECIMENTO

| Parâmetro | Valor |
|-----------|-------|
| RPM | 900 |
| TPS | 0% |
| **CLT** | **118°C** (acima do limite 115°C) |
| IAT | 50°C |
| Estado ECU | LIMP MODE |
| **Falha ativa** | **FAULT_OVERTEMP (P0217)** |

Verifica a detecção de superaquecimento, ativação do limp mode, sinalização
via LED vermelho e alerta via BLE.  
O display inverte a linha de status para destaque visual.

---

### SIM 6/8 — CDI MOTO CARBURADA

| Parâmetro | Valor |
|-----------|-------|
| RPM | 4.000 |
| TPS | 0% (carburador, sem TPS) |
| MAP | 0 (não aplicável) |
| CLT | 80°C |
| **CDI Avanço** | **28° BTDC** |
| Estado ECU | CDI RODANDO |
| Falhas | Nenhuma |

Verifica o módulo CDI (`ecu_cdi.c`), cálculo do ângulo via lookup table
e disparo do SCR. Linha 3 exibe "CDI: 28 BTDC" em vez do AFR (sem lambda
em motor carburado).

---

### SIM 7/8 — DETONAÇÃO (Knock)

| Parâmetro | Valor |
|-----------|-------|
| RPM | 5.000 |
| TPS | 85% |
| MAP | 95 kPa |
| CLT | 93°C |
| **Knock** | **850/1000** |
| Avanço | 18° BTDC (recuado 8° pela proteção) |
| **Falha ativa** | **FAULT_KNOCK_SEVERE (P0324)** |
| Estado ECU | RODANDO |

Verifica o algoritmo anti-knock (`ecu_ignition.c`), recuo do avanço,
alerta via BLE e LED vermelho. O avanço exibido já está recuado para
mostrar a proteção em ação.

---

### SIM 8/8 — CORTE DE COMBUSTÍVEL (Rev Limit)

| Parâmetro | Valor |
|-----------|-------|
| **RPM** | **11.000** (no limite configurado) |
| TPS | 100% |
| MAP | 100 kPa |
| CLT | 96°C |
| Avanço | 30° BTDC |
| **Falha ativa** | **FAULT_REV_LIMIT** (informativo) |
| Estado ECU | LIM RPM |

Verifica o limitador de rotação (`ecu_fuel.c` — corte de injeção),
sinalização de rev limit e comportamento da telemetria BLE no limite.

---

## Arquitetura do Módulo

```
main.c
  │
  ├── ECU_Sim_Init()          ← configura botões PC4/PE4 + OLED PC0/PC1
  │
  └── loop
        ├── ECU_Engine_Task_10ms()   ← processa ECU (lê ADC real — zerado)
        ├── ECU_Engine_Task_50ms()
        ├── ECU_Engine_Task_100ms()
        └── ECU_Sim_Task_10ms()      ← sobrescreve g_engine com valores sim
              ├── _btn_update()        lê SW1/SW2 com debounce 30ms
              ├── _apply_scenario()    injeta valores no g_engine
              └── _render_display()    atualiza OLED a cada 200ms
```

### Debounce dos botões

```
SW pressionado →  count++ a cada 10ms
SW solto após ≥3 amostras (≥30ms) → gera evento de pressão
Evento consumido → avança/recua cenário, marca display como dirty
```

### Atualização do display

- Normal: a cada **200ms** (20 × 10ms)
- Imediata: no momento da troca de cenário (`s_dirty = 1`)
- Flush: envia as 8 páginas do framebuffer via I2C software (~12ms @ 100kHz)

---

## Como Usar

### 1. Build & Flash

```bash
# Na pasta raiz do projeto (CMake já configurado)
cd build/Debug
cmake --build .
# Grave o .hex na placa via STM32CubeProgrammer ou openocd
```

### 2. Operação

1. Ligue a placa — o OLED exibe **SIM 1/8: MARCHA LENTA**
2. Pressione **SW1** para ir ao próximo cenário
3. Pressione **SW2** para voltar ao cenário anterior
4. Conecte o app BLE — os dados simulados aparecem em tempo real
5. Observe os LEDs:
   - **Verde pisca** → SW1 foi pressionado
   - **Azul pisca** → SW2 foi pressionado
   - **Vermelho aceso** → cenário com falha ativa

### 3. Verificando via BLE

Todos os valores simulados são transmitidos via BLE no `BleTelemPacket_t`
exatamente como seriam com sensores reais. Use o app de diagnóstico para:
- Ver RPM, TPS, MAP, CLT em tempo real
- Receber alertas de falha (P0217, P0324, etc.)
- Testar o datalogging em diferentes condições simuladas

---

## Adicionando Novos Cenários

Edite o array `s_scenarios[]` em [ECU/Src/ecu_sim.c](../ECU/Src/ecu_sim.c):

```c
static const SimScenario_t s_scenarios[SIM_COUNT] = {
    /* Seu novo cenário: */
    { "NOME_CURTO",
      rpm, tps_x10, map_kpa_x10, clt_c_x10, iat_c_x10,
      batt_mv, afr_x10, knock_level, ign_adv_x10,
      faults_bitmask, ENGINE_STATE, cdi_mode,
      "Texto info linha 5" },
    ...
};
```

Atualize `SIM_COUNT` em [ECU/Inc/ecu_sim.h](../ECU/Inc/ecu_sim.h) e o enum `SimMode_t`.

---

## Arquivos Relacionados

| Arquivo | Descrição |
|---------|-----------|
| [ECU/Src/ecu_sim.c](../ECU/Src/ecu_sim.c) | Engine de simulação — cenários, botões, display |
| [ECU/Inc/ecu_sim.h](../ECU/Inc/ecu_sim.h) | API pública do simulador |
| [Drivers/ECU/Src/drv_oled.c](../Drivers/ECU/Src/drv_oled.c) | Driver OLED SSD1315 (I2C software + fonte 6×8) |
| [Drivers/ECU/Inc/drv_oled.h](../Drivers/ECU/Inc/drv_oled.h) | API do driver OLED |
| [Inc/pinout.h](../Inc/pinout.h) | Definição dos pinos BTN_NEXT/PREV e OLED_SCL/SDA |
| [Src/main.c](../Src/main.c) | Integração: ECU_Sim_Init() e ECU_Sim_Task_10ms() |
