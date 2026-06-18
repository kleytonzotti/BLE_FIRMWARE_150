# ECU PROGRAMÁVEL UNIVERSAL — ESCOPO COMPLETO
**Versão do documento:** 1.0  
**Data:** 2026-06-17  
**Plataforma de desenvolvimento:** STM32WB5MM-DK  
**Autor:** Pesquisa e escopo técnico

---

## ÍNDICE
1. [Pesquisa de Mercado](#1-pesquisa-de-mercado)
2. [O Que é uma ECU Programável](#2-o-que-é-uma-ecu-programável)
3. [Sensores — Quais São e Por Que Precisam](#3-sensores--quais-são-e-por-que-precisam)
4. [Circuitos de Conexão dos Sensores](#4-circuitos-de-conexão-dos-sensores)
5. [CDI para Motor Carburado](#5-cdi-para-motor-carburado)
6. [Escopo 1 — ECU via BLE (sem tela)](#6-escopo-1--ecu-via-ble-sem-tela)
7. [Escopo 2 — ECU com Display Touchscreen](#7-escopo-2--ecu-com-display-touchscreen)
8. [Recomendação de Display](#8-recomendação-de-display)
9. [Microcontrolador para Produto Final](#9-microcontrolador-para-produto-final)
10. [Mapa de Memória Flash](#10-mapa-de-memória-flash)
11. [Arquitetura de Software em Baixo Nível](#11-arquitetura-de-software-em-baixo-nível)
12. [Restrições de Tempo Crítico](#12-restrições-de-tempo-crítico)

---

## 1. PESQUISA DE MERCADO

### 1.1 Concorrentes Diretos

| Produto            | Empresa        | Injetores | Ignições | CAN | BLE/Wi-Fi | Tela | Preço (R$) |
|--------------------|----------------|-----------|----------|-----|-----------|------|------------|
| FuelTech FT700     | FuelTech (BR)  | 8         | 8        | Sim | Wi-Fi     | 4" touch | R$ 8.000–12.000 |
| FuelTech FT600     | FuelTech (BR)  | 8         | 8        | Sim | Wi-Fi     | Não  | R$ 5.000–7.000 |
| FuelTech FT550     | FuelTech (BR)  | 4         | 4        | Sim | Wi-Fi     | Não  | R$ 3.000–4.500 |
| Haltech Elite 750  | Haltech (AU)   | 4         | 4        | Sim | Não       | Não  | R$ 4.000–6.000 |
| Haltech Elite 2500 | Haltech (AU)   | 8         | 8        | Sim | Não       | Não  | R$ 12.000+ |
| MoTeC M130         | MoTeC (AU)     | 8         | 8        | Sim | Não       | Não  | R$ 25.000+ |
| AEM Infinity 506   | AEM (US)       | 6         | 6        | Sim | Não       | Não  | R$ 5.000–8.000 |
| MS3Pro EVO         | MegaSquirt(US) | 8         | 8        | Sim | Não       | Não  | R$ 2.500–4.000 |
| Speeduino          | Open Source    | 4         | 4        | Sim | Não       | Não  | R$ 300–800 |
| Ecumaster EMU Black| Ecumaster (PL) | 8         | 8        | Sim | Não       | Não  | R$ 4.500–6.500 |
| Link G4X           | Link ECU (NZ)  | 8         | 8        | Sim | Não       | Não  | R$ 5.000–8.000 |

### 1.2 Referência: FuelTech FT700

A FuelTech é empresa **brasileira** de Sumaré-SP, fundada em 2005. O FT700 é o produto topo de linha e inspiração do nosso projeto. Funcionalidades que tornam ele referência:

- **Tela 4" colorida touchscreen** com dashboard configurável
- **Lambda wideband integrado** (não precisa controlador externo)
- **Wi-Fi** para configuração via app Android/iOS
- **USB** para datalogging e firmware update
- **CAN bus** para comunicação com painel digital, câmbio sequencial, PDM
- **8 canais de injeção sequencial** por cilindro
- **8 canais de ignição** com dwell individual
- **Launch control, traction control, flat shift**
- **Auto-tune** (ajusta tabela VE automaticamente pelo lambda)
- **Mapa de combustível 20×20** com múltiplas dimensões
- **Boost control** com mapa de pressão
- **Flex fuel** com sensor de composição
- Suporte a **motores 1-8 cilindros** (monocilíndrico de moto até V8)

### 1.3 O Que Nosso Produto Deve Ter (Diferencial)

- ✅ **BLE 5.0** em vez de Wi-Fi (menor custo, conexão mais rápida, sem roteador)
- ✅ **Suporte a CDI** (carburadores de moto — mercado brasileiro enorme)
- ✅ **1-4 cilindros** configurável por BLE
- ✅ **Perfis de veículo** pré-carregados (Honda CG, Yamaha, etc.)
- ✅ **CAN 2.0B + CAN FD** (comunicação com painel, câmbio, ABS)
- ✅ **Código aberto em nível de registradores** (sem licença, sem dependência de HAL)
- ✅ **Flash onboard** salva última configuração (sem conexão, funciona igual)
- ✅ **Datalogging via BLE** em tempo real
- ✅ **Versão com tela** para produto completo

---

## 2. O QUE É UMA ECU PROGRAMÁVEL

### Explicação Simples

Uma ECU (Engine Control Unit) é o "cérebro" do motor. Ela lê sensores, calcula quanto combustível injetar e quando dar a centelha, e faz isso **milhares de vezes por segundo**, com precisão de **microssegundos**.

### O Que Pode Dar Errado Se Atrasar

- **Faltar combustível:** Motor engasga, perde potência
- **Combustível em excesso:** Motor afoga, polui, consome demais
- **Centelha atrasada:** Detonação, dano ao pistão
- **Centelha adiantada:** Batida de biela, motor destrói em segundos

Por isso o código **não pode usar HAL_Delay nem bloqueios de software**. Tudo é orientado a interrupções e registradores diretos.

### Ciclo Básico do Motor (4 Tempos)

```
VIRABREQUIM gira 720° = 1 ciclo completo de 4 tempos

  0°      180°      360°      540°      720°
  |--------|---------|---------|---------|
  ADMISSÃO  COMPRESSÃO COMBUSTÃO  ESCAPE
  (aspira)  (comprime) (explode)  (sai fumaça)
     ↑                   ↑
  Injetor abre       Vela dispara
  (BTDC 90°)         (BTDC 10-35°)
```

**BTDC** = Before Top Dead Center = antes do ponto morto superior

---

## 3. SENSORES — QUAIS SÃO E POR QUE PRECISAM

### 3.1 OBRIGATÓRIOS — Sem eles o motor NÃO funciona

---

#### CKP — Sensor de Posição do Virabrequim (Crankshaft Position)
**Por que precisa:** Diz ao ECU onde está o motor a cada instante. Sem ele, impossível saber quando injetar ou dar centelha.  
**Tipo:** Hall Effect (digital 0-5V) ou Indutivo (senoidal ±30V)  
**Saída:** Pulso a cada dente do disco dentado (ex: disco 60-2 = 60 dentes, 2 faltando)  
**O gap (dente faltando)** sinaliza o ponto de referência (TDC — ponto morto superior)

---

#### TPS — Sensor de Posição do Acelerador (Throttle Position Sensor)
**Por que precisa:** Diz ao ECU o quanto o motorista está pedindo de aceleração. Com isso o ECU calcula a carga do motor.  
**Tipo:** Potenciômetro linear 0-5V (ou APPS dual channel em carros modernos)  
**Saída:** 0.5V = acelerador fechado, 4.5V = acelerador totalmente aberto  

---

#### MAP — Sensor de Pressão do Coletor (Manifold Absolute Pressure)
**Por que precisa:** Mede a pressão de ar que entra no motor. Junto com o RPM, calcula a massa de ar = quanto combustível injetar.  
**Tipo:** Semiconductor piezoelétrico, saída 0.5-4.5V  
**Escalas comuns:** 1 Bar (atmosférico), 2.5 Bar (turbo até 150kPa boost), 3.5 Bar  
**Formula:** Combustível = VE × RPM × MAP / IAT / AFR_stoich  

---

#### CLT — Temperatura do Refrigerante (Coolant Temperature)
**Por que precisa:** Motor frio precisa de mais combustível. Motor quente demais pode ser danificado. ECU corta combustível ou liga ventilador.  
**Tipo:** NTC (resistor que cai com temperatura). Típico: 2.5kΩ @ 20°C, 177Ω @ 100°C  
**Saída:** Tensão analógica (circuito divisor de tensão com pull-up)  

---

#### IAT — Temperatura do Ar de Admissão (Intake Air Temperature)
**Por que precisa:** Ar mais quente é menos denso = menos oxigênio = menos combustível necessário. Sem isso, a mistura fica errada em diferentes temperaturas ambientes.  
**Tipo:** NTC igual ao CLT (resistência diferente, mas mesmo circuito)  
**Saída:** Tensão analógica  

---

#### BATT — Tensão da Bateria
**Por que precisa:** O tempo de abertura do injetor depende da tensão. Bateria baixa = injetor abre mais devagar = menos combustível real.  
**Leitura:** Divisor de tensão (12V → 3.3V) no pino ADC do MCU  

---

### 3.2 IMPORTANTES — Sem eles perde qualidade e diagnóstico

---

#### CMP — Sensor de Posição do Comando (Camshaft Position)
**Por que precisa:** Para injeção **sequencial** (cada cilindro tem sua injeção independente), o ECU precisa saber qual cilindro está no tempo de admissão. Sem CMP funciona em modo **batch** (injeta em pares).  
**Tipo:** Hall Effect, normalmente 1 ou 3 dentes por rotação do comando (que gira a metade do virabrequim)  

---

#### O2 / Lambda Wideband — Sonda de Oxigênio
**Por que precisa:** Mede o AFR (Air-Fuel Ratio) real do escapamento. Permite que o ECU se autocorrija (closed-loop) e garante que a mistura esteja ideal.  
**Sonda recomendada:** Bosch LSU 4.9 (wideband, lê 10:1 a 20:1 AFR)  
**Controlador externo necessário:** CJ125 (Bosch IC) ou LC-2 (Innovate)  
**Saída:** 0-5V linear (configura via app) ou protocolo serial (CAN)  

---

#### Knock — Sensor de Detonação
**Por que precisa:** Detecta vibração no bloco causada por detonação ("batida de pino"). ECU recua o avanço para proteger o motor.  
**Tipo:** Acelerômetro piezoelétrico, frequência de ressonância 6-15kHz (Bosch 0 261 231 173)  
**Saída:** Tensão AC senoidal, precisa de amplificador e filtro passa-banda no hardware  

---

### 3.3 OPCIONAIS — Para produto completo de alta performance

---

#### VSS — Velocidade do Veículo (Vehicle Speed Sensor)
**Para:** Launch control (limita RPM na saída), traction control, corte de combustível no decel  
**Tipo:** Hall effect no câmbio/cubo de roda, ou sinal OBD  

#### Flex Fuel — Sensor de Composição
**Para:** Detecta % de etanol no combustível (E0-E100). ECU ajusta combustível e avanço automaticamente  
**Tipo:** Continental CGAS ou Delphi 25318386. Saída: frequência 50-150Hz (% etanol)  
**Importante para o Brasil** (flex é padrão nacional)

#### Boost / Pressão do Turbo
**Para:** Controle do wastegate (válvula que limita pressão do turbo)  
**Tipo:** MAP sensor de maior escala (2.5 Bar ou 3.5 Bar)  

#### Pressão do Óleo
**Para:** Diagnóstico e proteção. ECU corta motor se cair abaixo do mínimo  
**Tipo:** Sensor 0-10 Bar, saída 0.5-4.5V  

#### Pressão do Combustível
**Para:** Diagnóstico. Se cair, ECU compensa ou alerta  
**Tipo:** Sensor 0-10 Bar, saída 0.5-4.5V  

#### EGT — Temperatura do Escapamento (Exhaust Gas Temperature)
**Para:** Proteção em alta performance. Acima de 900°C indica mistura pobre perigosa  
**Tipo:** Termopar tipo K + amplificador MAX31855 (SPI)  

#### IMU — Acelerômetro/Giroscópio
**Para:** Traction control real (detecta escorregamento por aceleração lateral)  
**Tipo:** MPU-6050 (I2C), ICM-42688 (SPI)  

---

### 3.4 Tabela Resumo: O Que Realmente Precisa

| Sensor | Obrigatório | Importante | Opcional | CDI Carburado |
|--------|-------------|------------|----------|---------------|
| CKP    | ✅ SIM      |            |          | ✅ SIM        |
| TPS    | ✅ SIM      |            |          | ❌ Não (carb) |
| MAP    | ✅ SIM      |            |          | ⚠️ Opcional  |
| CLT    | ✅ SIM      |            |          | ⚠️ Opcional  |
| IAT    | ✅ SIM      |            |          | ❌ Não        |
| BATT   | ✅ SIM      |            |          | ✅ SIM        |
| CMP    |             | ✅ SIM     |          | ❌ Não        |
| O2 WB  |             | ✅ SIM     |          | ❌ Não        |
| Knock  |             | ✅ SIM     |          | ❌ Não        |
| VSS    |             |            | ✅ SIM   | ❌ Não        |
| Flex   |             |            | ✅ SIM   | ❌ Não        |
| Boost  |             |            | ✅ SIM   | ❌ Não        |
| Óleo   |             |            | ✅ SIM   | ❌ Não        |
| Combustível |        |            | ✅ SIM   | ❌ Não        |
| EGT    |             |            | ✅ SIM   | ❌ Não        |

---

## 4. CIRCUITOS DE CONEXÃO DOS SENSORES

> **Nota:** Todos os circuitos assumem MCU com ADC de 12 bits e Vref = 3.3V.  
> Entradas analógicas de 5V precisam de divisor de tensão para não danificar o MCU.  
> Sempre use capacitor de filtro (100nF) no pino ADC.

---

### 4.1 CKP — Sensor Hall Effect (digital)

```
         +5V
          │
         10kΩ (pull-up)
          │
          ├──────────────────── MCU GPIO (TIM Input Capture)
          │                      │
         [Hall Effect CKP]      100nF
          │                      │
         GND                    GND

Sensor Hall tem 3 fios:
  Fio VERMELHO  ──── +5V (alimentação)
  Fio PRETO     ──── GND
  Fio AMARELO   ──── Sinal (coleta open-drain)

O resistor pull-up é OBRIGATÓRIO (sensor open-drain).
Sinal: 0V = dente presente, 5V = espaço entre dentes
```

---

### 4.2 CKP — Sensor Indutivo (VR - Variable Reluctance)

```
Sensor gera tensão AC senoidal proporcional ao RPM.

  Sensor VR       MAX9924     MCU
  ┌──────┐        ┌──────┐
  │ VR+  ├───────►│ IN+  │
  │      │        │      ├──► GPIO (Timer Input Capture)
  │ VR-  ├───────►│ IN-  │
  └──────┘        │ OUT  │   Saída digital 0-3.3V
                  └──────┘

Alternativa simples: Comparador LM393
  VR+  ──── IN+  ──┬── 3.3V (sinal retificado)
  VR-  ──── IN-  ──┤ referência 0.5V (divisor)
  OUT  ──── GPIO do MCU com pull-up 10kΩ
```

---

### 4.3 TPS — Sensor de Posição do Acelerador (0-5V → 0-3.3V)

```
Acelerador
  │
 [Potenciômetro TPS]
  │           │           │
+5V         Sinal        GND
              │
             ─┤ 10kΩ ├──────────────────── MCU ADC_IN
              │         │
              │        6.8kΩ                100nF
              │         │                    │
             GND       GND                  GND

Divisor de tensão 10k/6.8k:
  Vout = 5V × 6.8k / (10k + 6.8k) = 2.02V (escala 0-2V para 0-100%)
  Melhor: 10k/10k = Vout_max = 2.5V (fácil de calibrar)

OU usar buffer OPA341 (rail-to-rail) com divisor 10k/10k.
```

---

### 4.4 MAP — Sensor de Pressão (MPX4250, 0.2-4.8V)

```
              +5V
               │
  [MAP Sensor 3 fios]
               │
  VCC ─────── +5V
  GND ─────── GND
  OUT ─────── Sinal
               │
              10kΩ                        MCU ADC_IN
               │                           │
              6.8kΩ  ──────────────────────┤
               │                          100nF
              GND                          │
                                          GND

MPX4250: 0 kPa = 0.20V, 250 kPa = 4.80V
MPX4115: 15–115 kPa = 0.20–4.80V (para motor sem turbo)
MPX4200: para turbo até 200 kPa (2 bar de boost)

Após divisor 10k/6.8k:
  0.20V → 0.08V (ADC ≈ 99)
  4.80V → 1.95V (ADC ≈ 2420)
```

---

### 4.5 CLT e IAT — Sensor de Temperatura NTC

```
  +3.3V
    │
   2.2kΩ (pull-up de precisão — resistência depende da faixa desejada)
    │
    ├───────────────── MCU ADC_IN
    │                    │
  [NTC Sensor]          100nF
    │                    │
   GND                  GND

Valores típicos NTC automotivo (Bosch 0 280 130 026):
  -40°C = 100.7 kΩ
    0°C =  5.9 kΩ
   20°C =  2.5 kΩ
   80°C =  323 Ω
  100°C =  177 Ω
  120°C =  100 Ω

Com pull-up 2.2kΩ e Vref 3.3V:
  Vadc = 3.3 × NTC / (2200 + NTC)

Tabela de lookup de 10 pontos converte tensão em temperatura.
```

---

### 4.6 Tensão da Bateria (12V → ADC)

```
  Bateria 12V (até 16V)
         │
        33kΩ
         │
         ├─────────────── MCU ADC_IN
         │                    │
        10kΩ                100nF
         │                    │
        GND                  GND

Divisor 33k/10k:
  Vout = Vbat × 10k / (33k + 10k) = Vbat × 0.2326
  16V → 3.72V (ACIMA de 3.3V — CUIDADO!)

CORRETO: usar 39k/10k:
  Vout = Vbat × 10k / 49k = Vbat × 0.204
  16V → 3.26V ✅ (dentro dos 3.3V)

Adicionar diodo Zener 3.3V na saída para proteção extra.
```

---

### 4.7 Sensor Knock (Bosch 0 261 231 173)

```
Sensor Knock gera tensão AC de ~0-2V pico na frequência de ressonância (6kHz)

  [Knock Sensor]       Filtro + Amplificador        MCU
       │
      SIG ─────────────────┐
                           │
                          ┌┴─────────────────────┐
                          │ LM358 / TL072         │
                          │                       │
                          │ Ganho: 10-50×         │
                          │ Filtro passa-banda:   ├──► ADC_IN (DMA, amostras rápidas)
                          │ 4kHz – 15kHz          │
                          │                       │
                          │ Opcional: MCU analisa  │
                          │ FFT em software        │
                          └───────────────────────┘
      GND ─────────────── GND

Alternativa: usar entrada de comparador do STM32 com threshold ajustável.
O STM32G4 tem HRTIM perfeito para análise de knock.
```

---

### 4.8 Wideband O2 — Bosch LSU 4.9 com CJ125

```
LSU 4.9 tem 6 fios:
  1 (APE) ─── Bomba de corrente positiva ─── CJ125 IP
  2 (VGND)─── Virtual GND ─────────────────── CJ125 VM
  3 (H-)  ─── Aquecedor negativo ──────────── GND (via MOSFET)
  4 (H+)  ─── Aquecedor positivo ──────────── +12V (via MOSFET)
  5 (UN)  ─── Referência Nernst ──────────── CJ125 UN
  6 (IA)  ─── Corrente de bomba ──────────── CJ125 IA

  ┌─────────────────────────────────────────┐
  │          CJ125 (Bosch IC)               │
  │                                         │
  │  SPI: MOSI, MISO, SCK, CS ─────────── MCU SPI │
  │  DIAG ──────────────────────────────── MCU GPIO │
  │                                         │
  │  PWM aquecedor ─── MOSFET ─── H+ LSU  │
  └─────────────────────────────────────────┘

O CJ125 retorna AFR via SPI em formato digital (14-bit).
Alternativa mais barata: AEM 30-2853 (saída 0-5V analógica linear).
```

---

### 4.9 Driver de Injetor (Alta Corrente, 1-4A pico)

```
Injetor automotivo: bobina indutiva ~12-16Ω, pico de corrente 1-4A

  MCU GPIO ────── 150Ω ────┬── Gate MOSFET (IRLZ44N ou IRL540N)
                            │
                        [MOSFET N-CH]
                            │
  +12V ─── [Injetor] ─────── Drain
                            │
  Diodo Flyback             Source ──── GND
  (1N4007 em paralelo       │
   com o injetor,           │
   cátodo para +12V)        │

Para 4 cilindros: 4× este circuito em paralelo.
MOSFET recomendado: IRLZ44N (55V, 47A, RdsON 22mΩ) — R$2 cada
Alternativa IC: VNQ860SP (4 canais num CI só, proteção integrada)

Atenção: Injetor tem diodo flyback OBRIGATÓRIO (pico de +300V na abertura da bobina).
```

---

### 4.10 Driver de Bobina de Ignição (Indutiva)

```
Bobina de ignição padrão: desenergiza em ~100-200µs, centelha ao cortar corrente

  MCU GPIO ───── 150Ω ─────── Gate IGBT (HGTG20N60B3)
                               │
  +12V ─── [Bobina Primária] ─── Coletor
                               │
  TVS Diode 400V               Emissor ─── GND
  (em paralelo com a           │
   bobina primária)            │

IGBT recomendado: HGTG20N60B3 (600V, 20A, para ignição)
OU: Usar módulo ignição pronto (VB409SP da STMicro — driver de bobina completo)

Dwell = tempo que a bobina fica energizada antes do disparo.
Dwell típico: 2-4ms. A energia armazenada na bobina é a centelha.
```

---

### 4.11 CAN Bus — Transceiver TJA1050 / SN65HVD230

```
  MCU CAN_TX ──── TJA1050 TXD
  MCU CAN_RX ──── TJA1050 RXD
  +5V         ──── TJA1050 VCC
  GND         ──── TJA1050 GND

  TJA1050 ────── CAN_H ──────┬──── conector externo
                              │
                             120Ω  ← terminação (SOMENTE nas pontas do barramento)
                              │
  TJA1050 ────── CAN_L ──────┘

O barramento CAN precisa de terminação de 120Ω em CADA extremidade.
Velocidade: 125kbps (diagnóstico) ou 500kbps (padrão automotivo) ou 1Mbps (performance)
```

---

### 4.12 Diagrama de Blocos do Hardware Completo

```
                    ┌──────────────────────────────────────────────────────┐
                    │                   ECU PROGRAMÁVEL                    │
                    │                                                      │
+12V ───── VREG ──► │ +5V interno                    +3.3V interno        │
GND ──────────────► │                                                      │
                    │   ┌──────────────────────────────────────────────┐   │
CKP ──── COND ────► │   │          MCU STM32WB55 / STM32H743          │   │
CMP ── (optional)──►│   │                                              │   │
TPS ── DIVIDER ───► │   │  ADC1: TPS, MAP, CLT, IAT, O2, KNOCK       │   │ ──► INJ1-4
MAP ── DIVIDER ───► │   │  ADC2: BATT, FLEX, BOOST, OIL, FUEL, EGT   │   │ ──► IGN1-4
CLT ── NTC────────► │   │  TIM: CKP capture, injetor scheduler        │   │ ──► FUEL PUMP
IAT ── NTC────────► │   │  SPI: CAN (MCP2515), SD Card, CJ125        │   │ ──► FAN
O2  ── CJ125─────► │   │  I2C: EEPROM, IMU                           │   │ ──► IAC
BATT── DIVIDER────► │   │  UART: Debug                                │   │ ──► WASTEGATE
FLEX── COUNTER────► │   │  IPCC: BLE 5.0 (interno)                   │   │
KNOCK─ FILTER─────► │   │                                              │   │
                    │   │  FLASH onboard: perfil + tabelas            │   │ ──► CAN_H
                    │   └──────────────────────────────────────────────┘   │ ──► CAN_L
                    │                                                      │
                    │                                            USB ◄───  │ ──► DEBUG UART
                    └──────────────────────────────────────────────────────┘
                                           │
                                      BLE 5.0
                                           │
                                    Smartphone App
```

---

## 5. CDI PARA MOTOR CARBURADO

### 5.1 O que é CDI

CDI (Capacitor Discharge Ignition) é o sistema de ignição usado em motos com carburador. Em vez de energizar uma bobina (indutiva), ele **carrega um capacitor a alta tensão** (~300-400V) e descarrega instantaneamente no momento do disparo. Vantagens:
- Centelha muito mais rápida (ideal para altas rotações)
- Sem degradação do dwell com RPM alto
- Mais fácil de usar em motores carburados (não precisa de injeção)

### 5.2 Como Funciona o CDI Programável

```
  ETAPA 1 — CARGA (sempre ativo):
  
  +12V ─── [Conversor DC-DC Boost] ──────────────────► +350V
                                                         │
                                                    [Capacitor]
                                                    1µF / 400V
                                                         │
                                                        GND

  ETAPA 2 — DISPARO (no ângulo calculado pelo ECU):
  
                    +350V
                      │
               [Capacitor carregado]
                      │
  MCU GPIO ─────────[SCR Gate]──────────── Trigger
                      │                        │
                   [SCR/Thyristor BT151]        │
                      │                   Optocoupler PC817
                      │                   (isolação galvânica!)
              [Primário da bobina CDI]
                      │
                     GND

  Quando SCR conduz:
  Capacitor 350V descarrega em ~1µs no primário
  Secundário gera 30,000-50,000V → CENTELHA na vela
```

### 5.3 Circuito do Conversor Boost (12V → 350V)

```
  +12V ─── L1 (680µH) ─────────────── D1 (FR207) ─── C_out (1µF/400V)
                │                                           │
               Q1 (IRF640)                                 GND
                │
               GND

  MCU TIM PWM ─── Gate Q1 (100kHz, duty ~70%)
  O duty cycle controla a tensão de saída.
  Controlador: MC34063 ou circuito com STM32 PWM + feedback de tensão.
```

### 5.4 Lógica do CDI Programável

Para motor **carburado** (sem injeção), o ECU CDI faz apenas:
1. Lê o CKP (posição do virabrequim = RPM)
2. Consulta tabela: **RPM → Avanço em graus BTDC**
3. Calcula o delay: `delay_us = graus_avanço × 60_000_000 / (RPM × 360)`
4. Dispara o SCR no momento exato

```
Tabela de Avanço CDI típica (Honda CG 150):

RPM    | Avanço (°BTDC)
-------|---------------
 500   |    5°
1000   |    8°
1500   |   12°
2000   |   16°
2500   |   20°
3000   |   24°
3500   |   27°
4000   |   29°
5000   |   32°
6000   |   33°
7000   |   34°
8000   |   34°
9000   |   33° (começa a recuar para proteção)
10000  |   32°
```

---

## 6. ESCOPO 1 — ECU VIA BLE (SEM TELA)

### Objetivo
Módulo compacto, configurado 100% via smartphone. Sem display local.  
Ideal para competição (leve, sem partes frágeis) e instalação discreta.

### Hardware

| Componente | Especificação | Motivo |
|------------|---------------|--------|
| MCU | STM32WB55CGU6 (QFN48) | BLE 5.0 nativo, 64MHz, 1MB flash |
| Regulador 5V | LM2596 (switching 3A) | Eficiência 80%+ em ambiente automotivo |
| Regulador 3.3V | AMS1117-3.3 (LDO 1A) | Para MCU e sensores analógicos |
| CAN | MCP2515 + TJA1050 | CAN 2.0B via SPI |
| O2 Wideband | CJ125 (IC Bosch) | Controle de sonda LSU 4.9 via SPI |
| Injetores | VNQ860SP (4 canais) | Driver 4× injetores, proteção integrada |
| Ignição | BUF634T + HGTG20N60B3 | Driver de bobina indutiva, 1-4 cilindros |
| CDI | BT151 SCR + PC817 optocoupler | Disparo CDI para moto carburada |
| Boost CDI | MC34063 | Conversor 12V→350V para CDI |
| ADC externo | MCP3208 (opcional) | 8 canais extra se MCU não tiver canais suficientes |
| Flash ext | W25Q64 (8MB SPI) | Datalogging onboard |
| EEPROM | AT24C256 (I2C) | Backup de configuração |
| Proteção | TVS SMAJ18A em todos inputs | Proteção contra +100V no barramento automotivo |
| Watchdog | TPS3813 (externo) | Reset hardware se MCU travar |
| Conector | Deutsch DT/DTM automotivo | Resistência a vibração e umidade |

### Entradas (Inputs)

| # | Sinal | Tipo | Pino MCU |
|---|-------|------|----------|
| 1 | CKP | Digital / Timer Capture | TIM2_CH1 (PA0) |
| 2 | CMP | Digital / Timer Capture | TIM2_CH2 (PA1) |
| 3 | TPS | ADC 12-bit | ADC1_IN1 (PC0) |
| 4 | MAP | ADC 12-bit | ADC1_IN2 (PC1) |
| 5 | CLT | ADC 12-bit | ADC1_IN3 (PC2) |
| 6 | IAT | ADC 12-bit | ADC1_IN4 (PC3) |
| 7 | O2/Lambda | SPI (CJ125) | SPI1 |
| 8 | Knock | ADC 12-bit (rápido) | ADC2_IN1 |
| 9 | BATT | ADC 12-bit | ADC1_IN5 |
| 10 | Flex Fuel | Timer Capture (freq) | TIM3_CH1 |
| 11 | VSS | Timer Capture (freq) | TIM3_CH2 |
| 12 | Boost | ADC 12-bit | ADC1_IN6 |
| 13 | Óleo | ADC 12-bit | ADC1_IN7 |
| 14 | Combustível | ADC 12-bit | ADC1_IN8 |
| 15 | Partida | GPIO (digital) | PB0 |

### Saídas (Outputs)

| # | Sinal | Tipo | Corrente | Pino MCU |
|---|-------|------|----------|----------|
| 1 | INJ1 | PWM / Timer CC | 4A (via VNQ860SP) | TIM1_CH1 (PA8) |
| 2 | INJ2 | PWM / Timer CC | 4A | TIM1_CH2 (PA9) |
| 3 | INJ3 | PWM / Timer CC | 4A | TIM1_CH3 (PA10) |
| 4 | INJ4 | PWM / Timer CC | 4A | TIM1_CH4 (PA11) |
| 5 | IGN1 | GPIO / Timer CC | 10mA → IGBT | TIM8_CH1 (PC6) |
| 6 | IGN2 | GPIO / Timer CC | 10mA → IGBT | TIM8_CH2 (PC7) |
| 7 | IGN3 | GPIO / Timer CC | 10mA → IGBT | TIM8_CH3 (PC8) |
| 8 | IGN4 | GPIO / Timer CC | 10mA → IGBT | TIM8_CH4 (PC9) |
| 9 | CDI1 | GPIO → SCR gate | 50mA (via optoc.) | PB10 |
| 10 | CDI2 | GPIO → SCR gate | 50mA | PB11 |
| 11 | FUEL PUMP | GPIO → MOSFET relay | 10A | PB12 |
| 12 | FAN | GPIO → MOSFET relay | 10A | PB13 |
| 13 | IAC | 4× GPIO (stepper) | 500mA (A4988) | PB4-PB7 |
| 14 | WASTEGATE | PWM | 2A | TIM16_CH1 |
| 15 | CHECK ENGINE | GPIO | 500mA LED | PB14 |
| 16 | CAN_TX | FDCAN/MCP2515 | — | SPI1 |
| 17 | CAN_RX | FDCAN/MCP2515 | — | SPI1 |

### Diagrama de Blocos — Escopo 1

```
┌─────────────────────────────────────────────────────────────────────┐
│                    ECU BLE — ESCOPO 1 (sem tela)                   │
│                                                                     │
│  ┌─────────┐     ┌────────────────────────────────────────────┐    │
│  │ LM2596  │     │           STM32WB55CGU6                    │    │
│  │ 12V→5V  │     │                                            │    │
│  └────┬────┘     │  ADC1 (DMA): TPS,MAP,CLT,IAT,BATT,KNOCK  │    │
│       │          │  ADC2 (DMA): BOOST,OIL,FUEL               │    │
│  ┌────┴────┐     │  TIM1: INJ1-4 (Output Compare)            │    │
│  │AMS1117  │     │  TIM2: CKP/CMP (Input Capture)            │    │
│  │ 5V→3.3V │     │  TIM3: FLEX,VSS (Input Capture freq)      │    │
│  └─────────┘     │  TIM8: IGN1-4 (Output Compare)            │    │
│                  │  TIM16: WASTEGATE PWM                      │    │
│  ┌─────────┐     │  SPI1: MCP2515(CAN) + CJ125(O2)           │    │
│  │MCP2515  ├────►│  SPI2: W25Q64 (datalogging flash)         │    │
│  │CAN 2.0B │     │  I2C1: AT24C256 EEPROM + IMU              │    │
│  └─────────┘     │  UART1: Debug / Serial tuning             │    │
│                  │  IPCC: BLE 5.0 Stack                      │    │◄──► App
│  ┌─────────┐     │  IWDG: Watchdog 500ms                     │    │
│  │ CJ125   ├────►│                                            │    │
│  │ O2 Ctrl │     └────────────────────────────────────────────┘    │
│  └─────────┘                                                        │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                  OUTPUTS (saídas de potência)                 │  │
│  │                                                               │  │
│  │  VNQ860SP ──► INJ1-4    HGTG20N60B3 ──► IGN1-4             │  │
│  │  BT151/SCR ──► CDI1-2   A4988 ──────► IAC Stepper           │  │
│  │  MOSFET ───► FUEL PUMP  MOSFET ─────► FAN                   │  │
│  └──────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### Dimensões estimadas da PCB
- 100mm × 80mm (compacto, cabe em caixa IP67)
- Espessura: 1.6mm, 4 camadas (GND plane + PWR plane)
- Conector: Deutsch DT 34 pinos (automotivo, resistente a vibração)

---

## 7. ESCOPO 2 — ECU COM DISPLAY TOUCHSCREEN

### Objetivo
Produto completo como o FuelTech FT700. Tela touch local para configuração sem smartphone.  
Requer MCU mais potente (display = framebuffer em RAM = muito processamento gráfico).

### Arquitetura de dois MCUs

O produto final usa **dois microcontroladores**:

```
┌─────────────────────────┐         ┌──────────────────────────┐
│   MCU PRINCIPAL         │         │   MCU BLE                │
│   STM32H743VIT6         │◄──SPI──►│   STM32WB55              │
│   Cortex-M7 @ 480MHz    │         │   (mesmo chip da placa   │
│   - ECU engine control  │         │    de desenvolvimento)   │
│   - Display TFT         │         │   - BLE 5.0 stack        │
│   - CAN FD              │         │   - Dados para app       │
│   - ADC 16-bit (alta    │         │   - Comandos do app      │
│     precisão)           │         └──────────────────────────┘
│   - 2MB flash           │
│   - 1MB RAM             │
│   - SD Card (logging)   │
└─────────────────────────┘
         │
         │ SPI/Parallel
         ▼
  ┌────────────────┐
  │ Display 3.5"  │
  │ ILI9488       │
  │ 320×480       │
  │ Touch XPT2046 │
  └────────────────┘
```

### Hardware adicional (além do Escopo 1)

| Componente | Especificação | Motivo |
|------------|---------------|--------|
| MCU principal | STM32H743VIT6 (LQFP100) | Cortex-M7 @ 480MHz, 2MB flash, 1MB RAM |
| Display | ILI9488 3.5" TFT 320×480 + XPT2046 | Touchscreen resistivo SPI |
| SD Card | SPI mode | Datalogging de corridas (até 32GB) |
| RTC | DS3231 (I2C, TCXO interno) | Precisão de tempo para log |
| USB | STM32H7 USB FS OTG | Configuração e update de firmware via PC |
| Buzzer | ativo 5V | Alertas de temperatura, RPM, falhas |
| LED RGB | WS2812B | Status visual (motor ligado, falha, turbo, etc.) |
| MCU BLE | STM32WB55 (módulo UART/SPI) | Reutiliza o mesmo firmware do Escopo 1 |

### Interface do Display — O Que Mostrar

**Tela 1 — Dashboard Principal:**
```
┌──────────────────────────────────────┐
│                                      │
│   RPM:    ██████░░░░  5.000 rpm     │
│   TPS:    ████░░░░░░   42.3 %       │
│   MAP:    ████████░░  100.3 kPa     │
│                                      │
│   CLT:  85°C    IAT: 32°C           │
│   AFR: 14.7    BATT: 13.8V          │
│                                      │
│   BOOST: 0.5 bar  VSS: 120 km/h    │
│                                      │
│  [MAPS] [SENSORS] [FAULTS] [CONFIG] │
└──────────────────────────────────────┘
```

**Tela 2 — Editor de Mapa VE (tabela 16×16):**
```
┌──────────────────────────────────────┐
│   MAPA VE — COMBUSTÍVEL              │
│                                      │
│   RPM\CARGA  20  40  60  80  100%   │
│   1000       62  70  78  82   84    │
│   2000       68  76  84  88  [90]   │ ← célula selecionada
│   3000       72  80  88  93   95    │
│   4000       74  82  90  96   98    │
│   5000       72  80  88  94   97    │
│   6000       69  77  85  90   93    │
│                                      │
│  [↑+1] [↓-1] [+5] [-5] [SALVAR]    │
└──────────────────────────────────────┘
```

---

## 8. RECOMENDAÇÃO DE DISPLAY

### Comparativo de Displays Touch para ECU

| Display | Tamanho | Resolução | Interface | Touch | Visib. Sol | Preço | Nota |
|---------|---------|-----------|-----------|-------|------------|-------|------|
| ILI9341 2.8" | 2.8" | 240×320 | SPI | Resistivo XPT2046 | Ruim | R$15-25 | Pequeno demais |
| **ILI9488 3.5"** | **3.5"** | **320×480** | **SPI** | **Resistivo XPT2046** | **Médio** | **R$25-45** | **✅ Escopo Dev** |
| ST7796 4.0" IPS | 4.0" | 320×480 | SPI | Resistivo | **Bom (IPS)** | R$40-60 | ✅ Boa escolha |
| SSD1963 5.0" | 5.0" | 800×480 | Paralelo 16bit | Capacitivo | Bom | R$70-100 | ✅ Produto final |
| WT32-SC01 Plus | 3.5" | 480×480 | ESP32-S3 interno | Capacitivo | Médio | R$100-150 | ❌ Ecossistema diferente |
| NHD-4.3 | 4.3" | 480×272 | Paralelo | Resistivo | Bom | R$120+ | Caro, industrial |

### Recomendação Final

**Para desenvolvimento (agora):**
> **ILI9488 3.5" SPI + XPT2046** — Interface SPI, suportado por muitas libs STM32, R$25-45 (LCSC/AliExpress).

**Para produto final:**
> **ST7796 4.0" IPS + GT911 capacitivo** — IPS é FUNDAMENTAL em ambiente automotivo (ângulo de visão, brilho no sol). Capacitivo é mais confiável que resistivo. ~R$60-80.

**Controlador gráfico sugerido:** LVGL (Light and Versatile Graphics Library) — open source, C puro, otimizado para MCUs embarcados.

---

## 9. MICROCONTROLADOR PARA PRODUTO FINAL

### Escopo 1 — BLE Only

**Desenvolvimento:** STM32WB5MM-DK (já disponível)  
**Produto final:** **STM32WB55CGU6** (QFN48, menor, mesmas funções)

| Spec | Valor |
|------|-------|
| Core | Cortex-M4 @ 64MHz |
| Flash | 1MB |
| RAM | 256KB |
| ADC | 12-bit, 5× canais |
| Timers | 10 timers (captura, PWM) |
| BLE | 5.0 nativo (coprocessador interno) |
| CAN | Não nativo → MCP2515 via SPI |
| SPI | 3× |
| I2C | 3× |
| Preço | R$18-30 (LCSC) |

### Escopo 2 — Com Display

**MCU Principal: STM32H743VIT6** (ou STM32H7B3LIH6Q)  
**MCU BLE: STM32WB55 como coprocessador** (comunica via SPI/UART com H7)

| Spec | STM32H743VIT6 |
|------|---------------|
| Core | Cortex-M7 @ **480MHz** |
| Flash | **2MB** (dual bank — atualiza sem parar) |
| RAM | **1MB** (I-TCM 64K + D-TCM 128K + AXI 512K + SRAM 256K) |
| ADC | **16-bit** (3× ADC, 20 canais) |
| Timers | 20 timers, HRTIM (resolução 217ps!) |
| CAN | **FDCAN 1+2** nativo (sem chip externo) |
| LCD | LTDC para display paralelo |
| SPI | 6× |
| USB | OTG FS + OTG HS |
| Ethernet | MAC integrado (futuro: diagnóstico via rede) |
| Preço | R$35-55 (LCSC) |

---

## 10. MAPA DE MEMÓRIA FLASH

### STM32WB55 (1MB Flash)

```
Endereço         Tamanho  Conteúdo
─────────────────────────────────────────────────────────
0x08000000       256KB    Firmware da aplicação (ECU)
0x08040000       512KB    Wireless stack (BLE — ST fornece)
0x080C0000        32KB    Reservado (Safe boot)
0x080C8000        60KB    Tabelas e dados de calibração
─────────────────────────────────────────────────────────
0x080D7000         4KB    Página de backup de config (A)
0x080D8000         4KB    Página ativa de config (B)
─────────────────────────────────────────────────────────
                          Estratégia dual-bank: sempre
                          lê da página ativa, escreve
                          na backup, depois inverte.
```

### Layout da Página de Configuração (4KB = 0x1000)

```c
/* Offset  Tamanho  Campo */
0x000      4B       Magic:        0xECU04321 (valida que a página é válida)
0x004      4B       Version:      (major << 16 | minor << 8 | patch)
0x008      4B       CRC32:        checksum de tudo de 0x00C até 0xFF8
0x00C      500B     EcuProfile_t: configuração do veículo (todos os parâmetros)
0x200      256B     VE Table:     16×16 uint8 (eficiência volumétrica)
0x300      256B     IGN Table:    16×16 int8  (avanço de ignição)
0x400      256B     AFR Table:    16×16 uint8 (AFR alvo)
0x500       64B     CDI Table:    32 pontos uint16 (RPM→avanço CDI)
0x540       64B     IAC Table:    posição IAC por temperatura (10 pontos)
0x580       64B     Boost Table:  16 pontos de controle de boost
0x5C0      256B     Fault Log:    últimas 32 ocorrências de falha (8B cada)
0x6C0      320B     Calibrations: cal TPS, MAP, O2, Flex
0x800      2KB      DataLog Header: ponteiro de ring buffer
0xFF0       4B      Magic end:    0xABCD1234
0xFF4       4B      Page ID:      qual página é a ativa
0xFF8       4B      Write count:  contador de writes (desgaste)
0xFFC       4B      Reserved
```

---

## 11. ARQUITETURA DE SOFTWARE EM BAIXO NÍVEL

### Por Que Registradores (Não HAL)

- `HAL_TIM_IC_Start_DMA()` tem overhead de ~50-200 ciclos de clock
- A cada dente do CKP em 10.000 RPM, temos 0.1ms = 6.400 ciclos a 64MHz
- 200 ciclos de overhead = **3% do tempo disponível desperdiçado em overhead**
- Código de registrador direto: 5-10 ciclos de overhead
- Em tempo crítico (ISR do CKP), **cada ciclo conta**

### Estrutura de ISRs e Prioridades

```
NVIC Priority (MENOR número = MAIOR prioridade):

  Prioridade 0: TIM2_IRQHandler  → CKP tooth detection (CRÍTICO — nunca bloqueia)
  Prioridade 0: TIM1_IRQHandler  → Injector close scheduler (CRÍTICO)
  Prioridade 1: TIM8_IRQHandler  → Ignition fire (CRÍTICO)
  Prioridade 2: DMA2_IRQHandler  → ADC DMA complete (sensores)
  Prioridade 3: SPI1_IRQHandler  → CAN / O2 wideband (importante)
  Prioridade 4: BLE IPCC IRQ     → BLE events (pode aguardar)
  Prioridade 5: USART1_IRQHandler → Debug serial (pode aguardar)
  Prioridade 6: Main loop         → Cálculos de tabela, idle PID, BLE telemetria

  REGRA: ISRs de prioridade alta NUNCA chamam funções bloqueantes.
  REGRA: main loop pode ser interrompido por qualquer ISR acima dele.
```

### Fluxo de Execução em Registradores

```c
/* Exemplo: Configurar TIM2 para captura do CKP em registradores diretos */

void TIM2_CKP_Init(void)
{
    /* 1. Habilita clock do TIM2 */
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

    /* 2. Configura PA0 como AF1 (TIM2_CH1) */
    GPIOA->MODER  &= ~GPIO_MODER_MODE0;          /* limpa modo */
    GPIOA->MODER  |=  GPIO_MODER_MODE0_1;         /* Alternate Function */
    GPIOA->AFR[0] |=  (1U << GPIO_AFRL_AFSEL0_Pos); /* AF1 = TIM2 */
    GPIOA->OSPEEDR|=  GPIO_OSPEEDR_OSPEED0;        /* High speed */

    /* 3. Configura TIM2 como timer de 1MHz (1µs/tick) */
    TIM2->PSC = (SystemCoreClock / 1000000UL) - 1; /* 64MHz / 64 = 1MHz */
    TIM2->ARR = 0xFFFFFFFF;                         /* 32-bit, máximo */

    /* 4. Canal 1: Input Capture, borda de subida, sem filtro */
    TIM2->CCMR1 &= ~TIM_CCMR1_CC1S;               /* limpa */
    TIM2->CCMR1 |=  TIM_CCMR1_CC1S_0;             /* CC1 = entrada TI1 */
    TIM2->CCMR1 &= ~TIM_CCMR1_IC1F;               /* sem filtro */
    TIM2->CCER  &= ~TIM_CCER_CC1P;                /* borda de subida */
    TIM2->CCER  |=  TIM_CCER_CC1E;                /* habilita captura */

    /* 5. Habilita interrupção de captura */
    TIM2->DIER |= TIM_DIER_CC1IE;
    NVIC_SetPriority(TIM2_IRQn, 0);               /* Máxima prioridade */
    NVIC_EnableIRQ(TIM2_IRQn);

    /* 6. Inicia o timer */
    TIM2->CR1 |= TIM_CR1_CEN;
}

/* ISR — chamada a CADA dente do disco CKP */
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_CC1IF) {
        TIM2->SR = ~TIM_SR_CC1IF;          /* limpa flag (write-1-to-clear) */

        uint32_t captured = TIM2->CCR1;    /* lê em 1 ciclo de clock */
        static uint32_t last = 0;

        uint32_t period_us = captured - last; /* período em µs */
        last = captured;

        OnCrankTooth(period_us);           /* processa (< 50 ciclos) */
    }
}
```

### Scheduler de 100µs (TIM3)

```c
/* TIM3 gera IRQ a cada 100µs para fechar injetores no tempo exato */

void TIM3_Scheduler_Init(void)
{
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
    TIM3->PSC = (SystemCoreClock / 1000000UL) - 1; /* 1MHz */
    TIM3->ARR = 100 - 1;                            /* 100µs */
    TIM3->DIER |= TIM_DIER_UIE;
    NVIC_SetPriority(TIM3_IRQn, 1);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1 |= TIM_CR1_CEN;
}

void TIM3_IRQHandler(void)
{
    TIM3->SR = ~TIM_SR_UIF;

    static uint32_t tick = 0;
    tick++;

    /* Fecha injetores no momento exato */
    for (uint8_t c = 0; c < 4; c++) {
        if (s_inj[c].active && tick >= s_inj[c].close_tick) {
            /* Fecha injetor c via registrador direto */
            GPIOA->BRR = (1U << (8 + c)); /* PA8-PA11 = INJ1-4 */
            s_inj[c].active = 0;
        }
    }
}
```

### ADC com DMA (sem bloquear a CPU)

```c
/* Configura ADC1 com DMA — lê 8 canais sem intervenção da CPU */

static uint16_t s_adc_raw[8]; /* Buffer DMA */

void ADC1_DMA_Init(void)
{
    /* Habilita clocks */
    RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    /* Configura ADC: 12-bit, contínuo, DMA */
    ADC1->CR  = 0;
    ADC1->CR |= ADC_CR_ADVREGEN;            /* Habilita regulador interno */
    /* ... delay 10µs para estabilizar ... */

    ADC1->CFGR  = ADC_CFGR_CONT            /* Modo contínuo */
               | ADC_CFGR_DMACFG          /* DMA circular */
               | ADC_CFGR_DMAEN;          /* Habilita DMA */
    ADC1->CFGR |= (2U << ADC_CFGR_RES_Pos); /* 12-bit */

    /* Sequência de conversão: TPS, MAP, CLT, IAT, BATT, KNOCK, BOOST, OIL */
    ADC1->SQR1 = (7U << ADC_SQR1_L_Pos); /* 8 conversões (L = N-1) */
    ADC1->SQR1 |= (1U << ADC_SQR1_SQ1_Pos); /* SQ1 = canal 1 (TPS) */
    /* ... configurar SQ2-SQ8 ... */

    /* DMA1 Canal 1: ADC1 → s_adc_raw */
    DMA1_Channel1->CPAR  = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR  = (uint32_t)s_adc_raw;
    DMA1_Channel1->CNDTR = 8;
    DMA1_Channel1->CCR   = DMA_CCR_MSIZE_0  /* 16-bit mem */
                         | DMA_CCR_PSIZE_0  /* 16-bit periph */
                         | DMA_CCR_MINC     /* incrementa mem */
                         | DMA_CCR_CIRC     /* circular */
                         | DMA_CCR_TCIE     /* IRQ ao completar */
                         | DMA_CCR_EN;      /* habilita */

    /* Liga ADC */
    ADC1->CR |= ADC_CR_ADEN;
    ADC1->CR |= ADC_CR_ADSTART;
}
```

---

## 12. RESTRIÇÕES DE TEMPO CRÍTICO

### Janela de Tempo Disponível

```
RPM      | Período de 1 rotação | Tempo por dente (60-2) | Budget ISR (máx 5%)
---------|---------------------|------------------------|--------------------
  500    |   120.0 ms          |   2.069 ms             |   103 µs
 1000    |    60.0 ms          |   1.034 ms             |    51 µs
 2000    |    30.0 ms          |     517 µs             |    25 µs
 4000    |    15.0 ms          |     259 µs             |    12 µs
 6000    |    10.0 ms          |     172 µs             |     8 µs
 8000    |     7.5 ms          |     129 µs             |     6 µs
10000    |     6.0 ms          |     103 µs             |     5 µs
12000    |     5.0 ms          |      86 µs             |     4 µs

A ISR do CKP deve executar em < 2µs (≈ 128 ciclos @ 64MHz).
O scheduler de 100µs: < 20µs para não atrasar fechamento do injetor.
```

### Regras de Ouro do Software ECU

1. **ISR do CKP nunca bloqueia.** Sem malloc, sem printf, sem HAL.
2. **Injetor fecha pelo hardware** (Output Compare), não por software polling.
3. **ADC sempre por DMA** — nunca espera conversão no polling.
4. **BLE nunca interfere no controle do motor** — executa no main loop ou em RTOS task de baixa prioridade.
5. **Flash write nunca durante combustão** — só quando motor parado ou pelo comando BLE.
6. **Watchdog alimentado no main loop** — se motor bloquear, watchdog reinicia em 500ms.
7. **Divisão inteira, sem float em ISR** — float salva/restaura coprocessador FPU (= latência).
8. **Lookup table, não trigonometria em runtime** — seno/cosseno são pré-calculados.

---

*Este documento é o escopo técnico completo. Nenhuma linha de código foi escrita ainda. Próximo passo: aprovar escopo e iniciar implementação por módulo.*

---

**Arquivos relacionados:**
- `PROTOCOLO_BLE_ECU.md` — Definição completa do protocolo de comunicação BLE
