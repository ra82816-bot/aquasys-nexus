# 🚀 Guia Completo de Configuração do Sistema HydroSmart

## ✅ Implementações Concluídas

### 1. **Banco de Dados Atualizado**
- ✅ Campo `firmware_version` ampliado para VARCHAR(50)
- ✅ Suporta versões completas como `4.0.7-DYNAMIC-AUTH`

### 2. **Edge Function de Autenticação Criada**
- ✅ Endpoint: `/functions/v1/device-auth`
- ✅ Retorna credenciais MQTT personalizadas por dispositivo
- ✅ Tópicos MQTT únicos por UUID

### 3. **Navegação Corrigida**
- ✅ Botão "Dispositivos" no menu agora leva para `/devices`
- ✅ Acesso direto à página de gerenciamento de dispositivos

### 4. **Firmware v4.0.7-DYNAMIC-AUTH Criado**
- ✅ Autenticação dinâmica via servidor
- ✅ Credenciais MQTT obtidas automaticamente
- ✅ Tópicos personalizados por UUID

---

## 📋 Passo a Passo para Configurar o Dispositivo

### **FASE 1: Registrar o Dispositivo no Sistema (Web)**

1. **Acesse a página de dispositivos:**
   - Clique no botão **"Dispositivos"** no menu principal
   - Ou acesse diretamente: `https://aquasys-nexus-kohl.vercel.app/devices`

2. **Preencha o formulário de vinculação:**
   ```
   UUID do Dispositivo: HYDRO-6CC8-4005-C7C0
   Tipo de Dispositivo: Módulo de Atuadores
   Versão do Firmware: 4.0.7-DYNAMIC-AUTH
   ```

3. **Clique em "Vincular Dispositivo"**
   - ✅ Deve aparecer mensagem de sucesso
   - ✅ Dispositivo será cadastrado no banco
   - ✅ Senha MQTT será gerada automaticamente

4. **Verifique no banco de dados:**
   ```sql
   SELECT device_uuid, firmware_version, mqtt_password_hash 
   FROM devices 
   WHERE device_uuid = 'HYDRO-6CC8-4005-C7C0';
   ```

---

### **FASE 2: Atualizar o Firmware do ESP32**

1. **Abra o Arduino IDE**

2. **Carregue o firmware:**
   - Arquivo: `Firmware_ESP32_Atuador_v4.0.7_DYNAMIC_AUTH.ino`

3. **Configure suas credenciais WiFi (linhas 30-31):**
   ```cpp
   const char* WIFI_SSID = "SUA_REDE_WIFI";
   const char* WIFI_PASSWORD = "SUA_SENHA_WIFI";
   ```

4. **Faça o upload para o ESP32:**
   - Placa: ESP32 Dev Module
   - Velocidade: 115200 baud
   - Porta: Selecione a porta COM do seu ESP32

5. **Abra o Serial Monitor (115200 baud)**

---

### **FASE 3: Testar a Conexão**

#### **O que esperar no Serial Monitor:**

```
[0.00s] [INFO] ═══════════════════════════════════════════
[0.10s] [INFO] AquaSys Nexus - Módulo de Atuadores
[0.10s] [INFO] Firmware: 4.0.7-DYNAMIC-AUTH
[0.10s] [INFO] ═══════════════════════════════════════════
[0.15s] [INFO] Device UUID: HYDRO-6CC8-4005-C7C0
[0.20s] [INFO] Configurando relés...
[0.25s] [INFO] 8 relés configurados e desligados
[0.30s] [INFO] Conectando WiFi...
[2.50s] [INFO] WiFi conectado!
[2.51s] [INFO] IP: 192.168.1.100
[2.52s] [INFO] RSSI: -45 dBm
[2.60s] [INFO] Autenticando dispositivo...
[2.65s] [DEBUG] Auth request: {"device_uuid":"HYDRO-6CC8-4005-C7C0","firmware_version":"4.0.7-DYNAMIC-AUTH"}
[3.20s] [DEBUG] Auth response: {"success":true,"mqtt_config":{...}}
[3.21s] [INFO] Autenticação bem-sucedida!
[3.22s] [INFO] MQTT Broker: wss://8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud:8884/mqtt
[3.23s] [INFO] Topic Status: aquasys/HYDRO-6CC8-4005-C7C0/relay/status
[3.30s] [INFO] Conectando MQTT...
[3.35s] [DEBUG] Broker: wss://8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud:8884/mqtt
[3.36s] [DEBUG] ClientID: HYDRO-6CC8-4005-C7C0_1234567890
[3.37s] [DEBUG] Username: HYDRO-6CC8-4005-C7C0
[4.50s] [INFO] MQTT conectado!
[4.51s] [INFO] Subscrito a: aquasys/HYDRO-6CC8-4005-C7C0/relay/command
[4.60s] [DEBUG] Status publicado: {"device_uuid":"HYDRO-6CC8-4005-C7C0",...}
[4.70s] [INFO] Heartbeat enviado
[4.75s] [INFO] Setup completo! Sistema operacional.
```

---

### **FASE 4: Verificar na Interface Web**

1. **Acesse o Dashboard:**
   - `https://aquasys-nexus-kohl.vercel.app/dashboard`

2. **Verifique a aba "Dispositivos":**
   - ✅ Deve mostrar: **"Dispositivos Conectados: 1"**
   - ✅ Deve exibir o UUID: `HYDRO-6CC8-4005-C7C0`
   - ✅ Status: **"Online"**

3. **Teste os relés na aba "Relés":**
   - Clique em qualquer relé para ligar/desligar
   - Observe o LED do ESP32 correspondente
   - Verifique logs no Serial Monitor

---

## 🔧 Troubleshooting

### **Erro: "Device not registered"**
**Causa:** Dispositivo não foi cadastrado no banco de dados.

**Solução:**
1. Acesse `/devices` na interface web
2. Cadastre o dispositivo manualmente
3. Reinicie o ESP32

---

### **Erro: "MQTT_CONNECT_UNAUTHORIZED" (State 5)**
**Causa:** Senha MQTT incorreta ou não gerada.

**Solução:**
1. Verifique no banco se `mqtt_password_hash` foi gerado:
   ```sql
   SELECT mqtt_password_hash FROM devices WHERE device_uuid = 'HYDRO-6CC8-4005-C7C0';
   ```
2. Se estiver NULL, delete o registro e cadastre novamente
3. Reinicie o ESP32

---

### **Erro: "Auth failed! HTTP Code: 404"**
**Causa:** Edge Function `device-auth` não está deployada.

**Solução:**
1. Aguarde o deploy automático (ocorre quando você salva arquivos)
2. Ou force o deploy manualmente
3. Teste o endpoint: `https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/device-auth`

---

### **Erro: "WiFi connection timeout"**
**Causa:** Credenciais WiFi incorretas ou sinal fraco.

**Solução:**
1. Verifique SSID e senha no código
2. Aproxime o ESP32 do roteador
3. Verifique RSSI no log (deve ser > -70 dBm)

---

## 📊 Arquitetura do Sistema

```
┌─────────────────────────────────────────────────────────────────┐
│                         ESP32 (Atuadores)                       │
│  - Gera UUID do MAC Address                                     │
│  - Conecta WiFi                                                 │
│  - Chama /device-auth para obter credenciais MQTT              │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     │ HTTPS POST (device_uuid, firmware_version)
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│            Edge Function: device-auth                           │
│  - Valida se device_uuid está cadastrado                       │
│  - Retorna: broker, username, password, client_id, topics     │
│  - Atualiza last_seen_at e firmware_version                    │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     │ Retorna credenciais únicas
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ESP32 conecta MQTT                           │
│  - Broker: HiveMQ Cloud (WSS)                                  │
│  - Username: HYDRO-6CC8-4005-C7C0                              │
│  - Password: Hash gerado no cadastro                           │
│  - Topics personalizados por UUID                              │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     │ Publica/subscreve tópicos
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                Frontend (React + MQTT.js)                       │
│  - Conecta ao mesmo broker HiveMQ                              │
│  - Subscreve aos tópicos do dispositivo                        │
│  - Exibe status em tempo real                                  │
│  - Envia comandos aos relés                                    │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🎯 Próximos Passos (Opcional)

### **1. Implementar ACLs no HiveMQ Cloud**
- Limitar cada dispositivo aos seus próprios tópicos
- Evitar que dispositivos publiquem em tópicos de outros

### **2. Adicionar Certificado TLS**
- Substituir `wifiClient.setInsecure()` por certificado real
- Aumentar segurança da conexão

### **3. Implementar OTA Updates**
- Permitir atualização de firmware remotamente
- Usar tópico `aquasys/{UUID}/ota`

### **4. Dashboard de Diagnóstico**
- Criar página `/devices/diagnostics`
- Exibir heartbeat, uptime, RSSI, memória livre

---

## ✅ Checklist de Validação

- [ ] Dispositivo cadastrado no banco com `mqtt_password_hash` gerado
- [ ] Firmware v4.0.7-DYNAMIC-AUTH carregado no ESP32
- [ ] WiFi conectado (RSSI visível no log)
- [ ] Autenticação bem-sucedida (HTTP 200)
- [ ] MQTT conectado (sem erros de State)
- [ ] Heartbeat sendo enviado a cada 30s
- [ ] Status dos relés visível no frontend
- [ ] Comandos de relé funcionando (ON/OFF)
- [ ] Logs estruturados e legíveis

---

## 📞 Suporte

**Se encontrar erros:**
1. Copie os logs completos do Serial Monitor
2. Verifique o banco de dados:
   ```sql
   SELECT * FROM devices WHERE device_uuid = 'HYDRO-6CC8-4005-C7C0';
   ```
3. Verifique logs da Edge Function no painel Lovable Cloud
4. Teste a conexão MQTT manualmente com MQTT Explorer

---

**Sistema implementado com sucesso! 🎉**

Desenvolvido por **André Crepaldi**  
HydroSmart - Agricultura de Precisão
