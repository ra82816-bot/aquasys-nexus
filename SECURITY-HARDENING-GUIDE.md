# Guia de Hardening de Segurança - AquaSys Nexus

## ⚠️ AÇÃO OBRIGATÓRIA ANTES DE PRODUÇÃO

Este documento descreve as etapas **OBRIGATÓRIAS** para remover segredos hardcoded dos firmwares antes do deployment em produção.

---

## PRIORIDADE II.2: Remover Segredos Embutidos

### ❌ Problema Atual

Os seguintes segredos estão **hardcoded** no código-fonte e são visíveis no repositório:

**Firmware Sensor (v4.3.4):**
- `SUPABASE_ANON_KEY` (linha 108)
- Credenciais MQTT fallback (linhas 1413-1414): `"hydrosmart" / "Hydro@2024!"`
- `AP_PASSWORD` (linha 94): `"aquasys2024"`

**Firmware Atuador (v4.3.0):**
- `SUPABASE_ANON_KEY` (linha 64)
- `AP_PASSWORD` (linha 52): `"aquasys2024"`

---

## ✅ Solução: Build-Time Injection

### Opção 1: PlatformIO (Recomendado)

Criar arquivo `platformio.ini`:

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino

; Injetar secrets via build flags
build_flags = 
  -D SUPABASE_URL=\"${sysenv.SUPABASE_URL}\"
  -D SUPABASE_ANON_KEY=\"${sysenv.SUPABASE_ANON_KEY}\"
  -D AP_PASSWORD=\"${sysenv.AP_PASSWORD}\"
  -D FALLBACK_MQTT_USER=\"${sysenv.MQTT_USER}\"
  -D FALLBACK_MQTT_PASS=\"${sysenv.MQTT_PASS}\"
```

No código, **REMOVER** as linhas com valores hardcoded e deixar apenas:
```cpp
// Definidos via build flags
#ifndef SUPABASE_ANON_KEY
#error "SUPABASE_ANON_KEY not defined. Set via build flags."
#endif
```

### Opção 2: Arduino IDE - Arquivo Secrets (Não Versionado)

1. Criar `secrets.h` (adicionar ao `.gitignore`):
```cpp
#define SUPABASE_ANON_KEY "sua_chave_aqui"
#define AP_PASSWORD "senha_forte_aqui"
#define FALLBACK_MQTT_USER "user"
#define FALLBACK_MQTT_PASS "pass"
```

2. No firmware, incluir:
```cpp
#include "secrets.h"  // NÃO VERSIONAR ESTE ARQUIVO
```

---

## 📋 Checklist de Segurança

- [ ] Remover `SUPABASE_ANON_KEY` hardcoded dos firmwares
- [ ] Remover credenciais MQTT fallback hardcoded
- [ ] Usar senhas fortes e únicas para `AP_PASSWORD` (min. 16 caracteres)
- [ ] Rotacionar todas as chaves após remoção do código versionado
- [ ] Validar que `git log` não contém histórico de segredos
- [ ] Configurar CI/CD para injetar secrets no build
- [ ] Documentar processo de provisionamento de dispositivos

---

## 🔒 Melhorias de Segurança Implementadas

✅ **Correções Funcionais (Prioridade I):**
- Fallback BLE corrigido (conversão ASCII→float)
- MQTT Last Will and Testament (LWT) implementado

✅ **Correções de Segurança (Prioridade II):**
- Certificados CA Root configurados (HiveMQ + Supabase)
- Validação SSL/TLS ativada (removido `setInsecure()`)

✅ **Correções de Robustez (Prioridade III):**
- Buffers persistentes para strings C (corrigido `c_str()` temporário)
- QoS 1 padronizado para comandos críticos

---

## 🚀 Próximos Passos

1. Implementar estratégia de secrets (PlatformIO ou secrets.h)
2. Remover todos os valores hardcoded
3. Rotacionar chaves comprometidas
4. Testar deployment com secrets injetados
5. Documentar processo de provisionamento seguro
