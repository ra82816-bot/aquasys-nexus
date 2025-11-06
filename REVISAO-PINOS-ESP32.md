# 📋 Revisão Completa de Pinos - ESP32 AquaSys Sensor Module

## 🎯 Resumo da Configuração de Pinos

Esta documentação detalha todos os pinos utilizados no módulo sensor ESP32 do sistema AquaSys.

---

## 📊 SENSORES ANALÓGICOS

### 🔬 Sensor de pH
- **Pino:** GPIO34 (ADC1_CH6)
- **Tipo:** Entrada Analógica
- **Faixa:** 0-3.3V (resolução 12-bit: 0-4095)
- **Função:** Leitura de pH da solução nutritiva
- **Observação:** ADC1 pode ser usado simultaneamente com WiFi

### 💧 Sensor de EC/TDS (Condutividade Elétrica)
- **Pino:** GPIO35 (ADC1_CH7)
- **Tipo:** Entrada Analógica
- **Faixa:** 0-3.3V (resolução 12-bit: 0-4095)
- **Função:** Medição de condutividade elétrica/TDS
- **Observação:** Compensação automática de temperatura aplicada

---

## 🌡️ SENSORES DIGITAIS

### 🌤️ DHT22 - Temperatura e Umidade do Ar
- **Pino:** GPIO15
- **Tipo:** One-Wire Digital
- **Protocolo:** Proprietário DHT
- **Função:** 
  - Temperatura do ar: -40°C a +80°C (±0.5°C)
  - Umidade relativa: 0-100% (±2%)
- **Biblioteca:** `DHT.h` por Adafruit

### 💦 DS18B20 - Temperatura da Água
- **Pino:** GPIO2
- **Tipo:** One-Wire Digital
- **Protocolo:** 1-Wire (Dallas)
- **Função:** Temperatura da água: -55°C a +125°C (±0.5°C)
- **Biblioteca:** `DallasTemperature.h`
- **Observação:** Sensor impermeável para submersão

---

## 🖥️ DISPLAY OLED 128x64

### 📺 Interface I²C
- **Pino SDA:** GPIO21 (padrão I²C ESP32)
- **Pino SCL:** GPIO22 (padrão I²C ESP32)
- **Endereço I²C:** 0x3C (padrão)
- **Resolução:** 128x64 pixels
- **Tipo:** OLED SSD1306
- **Função:** Exibir leituras, menus e status do sistema
- **Biblioteca:** `Adafruit_SSD1306.h` e `Adafruit_GFX.h`

---

## 🎮 BOTÕES DE CONTROLE

### ⬆️ Botão UP
- **Pino:** GPIO32
- **Tipo:** Entrada Digital com Pull-up interno
- **Função:** 
  - Navegar para cima nos menus
  - Selecionar página anterior
  - Ajustar valores

### ⬇️ Botão DOWN
- **Pino:** GPIO33
- **Tipo:** Entrada Digital com Pull-up interno
- **Função:** 
  - Navegar para baixo nos menus
  - Selecionar próxima página
  - Ajustar valores

### ✅ Botão SELECT
- **Pino:** GPIO25
- **Tipo:** Entrada Digital com Pull-up interno
- **Função:** 
  - Confirmar seleção
  - Entrar em submenu
  - Salvar calibração
  - Iniciar scan WiFi

### ◀️ Botão BACK
- **Pino:** GPIO26
- **Tipo:** Entrada Digital com Pull-up interno
- **Função:** 
  - Voltar ao menu anterior
  - Cancelar operação
  - Sair de calibração

**⚠️ Debounce:** 200ms implementado em software para todos os botões

---

## 📡 COMUNICAÇÃO SEM FIO

### 📶 WiFi (Integrado ao ESP32)
- **Chip:** ESP32 WiFi integrado
- **Modos:** 
  - Station (STA): Conecta a redes WiFi existentes
  - Access Point (AP): Cria rede própria para configuração
- **Frequência:** 2.4 GHz (802.11 b/g/n)
- **Uso:**
  - Conexão MQTT para envio de dados
  - Portal captivo para configuração
  - Sincronização NTP

### 🔵 Bluetooth Low Energy - BLE (Integrado ao ESP32)
- **Chip:** ESP32 BLE integrado
- **Versão:** Bluetooth 4.2
- **Função:**
  - Transmissão de dados de sensores em tempo real
  - Configuração local via app mobile
  - Pareamento de dispositivos
- **UUIDs de Serviço:**
  - Service: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
  - pH: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
  - EC: `beb5483e-36e1-4688-b7f5-ea07361b26a9`
  - Temp Ar: `beb5483e-36e1-4688-b7f5-ea07361b26aa`
  - Umidade: `beb5483e-36e1-4688-b7f5-ea07361b26ab`
  - Temp Água: `beb5483e-36e1-4688-b7f5-ea07361b26ac`

---

## ⚡ ALIMENTAÇÃO

### 🔌 Tensão de Operação
- **Tensão:** 5V via USB ou regulador externo
- **Consumo Típico:** 
  - Normal: ~150-250mA
  - WiFi TX: até 500mA (picos)
  - Deep Sleep: ~10µA (não implementado)

### 🔋 Regulador Interno
- **ESP32:** Regulador interno 3.3V
- **Sensores:** Alimentados por 3.3V ou 5V conforme especificação

---

## 🔧 PINOS RESERVADOS / NÃO UTILIZADOS

### ⚠️ Pinos que devem ser evitados:
- **GPIO0:** Boot mode (deve estar HIGH no boot)
- **GPIO1:** TX Serial (usado para debug)
- **GPIO3:** RX Serial (usado para debug)
- **GPIO6-11:** Flash SPI (NÃO USAR - conectados à flash interna)
- **GPIO12:** Pode causar problemas no boot se em HIGH

### ✅ Pinos disponíveis para expansão futura:
- GPIO4, GPIO5, GPIO13, GPIO14, GPIO16, GPIO17, GPIO18, GPIO19, GPIO23, GPIO27

---

## 📐 DIAGRAMA DE CONEXÕES

```
ESP32 Módulo Sensor AquaSys
┌─────────────────────────────────────┐
│                                     │
│  [ADC]  GPIO34 ─────► Sensor pH    │
│  [ADC]  GPIO35 ─────► Sensor EC    │
│                                     │
│  [1W]   GPIO2  ─────► DS18B20      │
│  [DHT]  GPIO15 ─────► DHT22        │
│                                     │
│  [I2C]  GPIO21 ─────► OLED SDA     │
│  [I2C]  GPIO22 ─────► OLED SCL     │
│                                     │
│  [BTN]  GPIO32 ─────► UP           │
│  [BTN]  GPIO33 ─────► DOWN         │
│  [BTN]  GPIO25 ─────► SELECT       │
│  [BTN]  GPIO26 ─────► BACK         │
│                                     │
│  [WiFi] Interno ───► 2.4GHz        │
│  [BLE]  Interno ───► Bluetooth 4.2 │
│                                     │
│  [PWR]  5V/GND  ───► USB/Ext       │
└─────────────────────────────────────┘
```

---

## 🎨 PÁGINAS DO DISPLAY OLED

### 1️⃣ Dashboard (Página Inicial)
- pH atual
- EC (condutividade)
- Temperatura do ar
- Umidade relativa
- Temperatura da água

### 2️⃣ Conexões
- Status WiFi (conectado/desconectado)
- RSSI (força do sinal)
- Status MQTT
- Status BLE
- Botão para scan de redes

### 3️⃣ Calibração
- Menu de seleção:
  - pH 7.0
  - pH 4.0
  - EC Low (360 µS/cm)
  - EC High (4588 µS/cm)
- Tela de calibração com valor atual
- Confirmação e salvamento

### 4️⃣ Sistema
- UUID do dispositivo
- Memória livre
- Uptime (tempo ligado)
- Versão do firmware

---

## 🔄 MODO AP (Access Point)

### 🌐 Configuração do Portal Web
- **SSID:** `AquaSys-SEN-XXXXXX` (XXXXXX = últimos 6 dígitos do MAC)
- **Senha:** `aquasys2024`
- **IP do ESP32:** `192.168.4.1`
- **DNS:** Redirecionamento captive portal
- **Portal:** Interface web para configurar WiFi

### 📱 Acesso ao Portal
1. Conectar ao WiFi "AquaSys-SEN-XXXXXX"
2. Abrir navegador (deve abrir automaticamente)
3. Se não abrir, acessar: `http://192.168.4.1`
4. Escanear redes disponíveis
5. Selecionar rede e digitar senha
6. Salvar (ESP32 reinicia e conecta)

---

## 💾 ARMAZENAMENTO NVS (Non-Volatile Storage)

### 🗂️ Namespaces utilizados:
- **"wifi":** Credenciais de até 3 redes WiFi
- **"mqtt":** Credenciais MQTT do broker
- **"calib":** Valores de calibração de pH e EC
- **"crash":** Último erro crítico (debug)

---

## 🧪 VALORES DE CALIBRAÇÃO PADRÃO

### pH
- **pH 7.0:** 2.52V (neutro)
- **pH 4.0:** 3.29V (ácido)

### EC (Condutividade Elétrica)
- **EC Low:** 360 µS/cm (valor: 645 raw ADC)
- **EC High:** 4588 µS/cm (valor: 2850 raw ADC)

**⚠️ Nota:** Estes são valores de referência. Calibre com soluções certificadas!

---

## 📚 BIBLIOTECAS UTILIZADAS

| Biblioteca | Versão | Função |
|------------|--------|--------|
| Wire.h | Nativa | Comunicação I²C |
| WiFi.h | Nativa | WiFi ESP32 |
| WebServer.h | Nativa | Servidor web |
| DNSServer.h | Nativa | Captive portal |
| BLEDevice.h | Nativa | Bluetooth LE |
| Adafruit_SSD1306.h | 2.5+ | Display OLED |
| DHT.h | 1.4+ | Sensor DHT22 |
| DallasTemperature.h | 3.9+ | Sensor DS18B20 |
| ArduinoJson.h | 6.21+ | Parse/serialização JSON |
| PubSubClient.h | 2.8+ | Cliente MQTT |

---

## 🎯 INTERVALOS DE LEITURA

- **Sensores:** 30 segundos (configurável)
- **Display:** 500ms (atualização visual)
- **Heartbeat MQTT:** 60 segundos
- **Watchdog Reset:** 1 segundo
- **Check WiFi:** 10 segundos
- **Debounce Botões:** 200ms

---

## 🛡️ WATCHDOG TIMER

- **Timeout:** 60 segundos
- **Função:** Reiniciar ESP32 em caso de travamento
- **Reset:** A cada 1 segundo em operação normal
- **Panic:** Reinício forçado se não receber reset

---

## 📞 CONTATO E SUPORTE

**Firmware:** v4.3.3-WEB-IMPROVED  
**Tipo:** Módulo Sensor  
**Autor:** HydroSmart Team  
**Data:** Janeiro 2025  

---

**✅ Documento atualizado e completo!**
