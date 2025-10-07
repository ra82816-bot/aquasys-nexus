# 📱 Mobile App Setup - Hydro Smart

Este documento descreve como configurar e executar o app Hydro Smart em dispositivos Android e iOS usando Capacitor.

## ✅ Pré-requisitos

### Para desenvolvimento Android:
- **Android Studio** instalado
- **Java JDK 17+** instalado
- Android SDK configurado

### Para desenvolvimento iOS:
- **macOS** com **Xcode 14+** instalado
- **CocoaPods** instalado (`sudo gem install cocoapods`)

## 🚀 Primeiros Passos

### 1. Exportar o projeto para o GitHub

1. No Lovable, clique no botão **"Export to GitHub"**
2. Clone o repositório para sua máquina local:
   ```bash
   git clone https://github.com/SEU-USUARIO/aquasys-nexus.git
   cd aquasys-nexus
   ```

### 2. Instalar dependências

```bash
npm install
```

### 3. Adicionar plataformas nativas

#### Android:
```bash
npx cap add android
```

#### iOS (somente em macOS):
```bash
npx cap add ios
cd ios/App
pod install
cd ../..
```

### 4. Build do projeto web

```bash
npm run build
```

### 5. Sincronizar com as plataformas nativas

```bash
npx cap sync
```

## 🔧 Desenvolvimento

### Executar em Android

#### Emulador:
```bash
npx cap run android
```

#### Dispositivo físico:
1. Ative o **Modo de Desenvolvedor** no seu Android
2. Conecte via USB e ative **Depuração USB**
3. Execute:
   ```bash
   npx cap run android
   ```

### Executar em iOS

#### Simulador:
```bash
npx cap run ios
```

#### Dispositivo físico:
1. Abra o projeto no Xcode:
   ```bash
   npx cap open ios
   ```
2. Configure seu **Team** e **Bundle Identifier**
3. Conecte seu iPhone via USB
4. Selecione seu dispositivo no Xcode
5. Clique em **Run** (▶️)

## 📲 Recursos Nativos Implementados

### ✅ Push Notifications
- Suporte para notificações push em Android (FCM) e iOS (APNs)
- Configurado automaticamente via `usePushNotifications` hook

#### Configurar Firebase (Android):
1. Crie um projeto no [Firebase Console](https://console.firebase.google.com/)
2. Adicione um app Android com o package name: `app.lovable.7e63af5c233645ef82292ff1e2827bff`
3. Baixe o arquivo `google-services.json`
4. Coloque em: `android/app/google-services.json`

#### Configurar APNs (iOS):
1. No [Apple Developer Portal](https://developer.apple.com/):
   - Crie um App ID
   - Ative Push Notifications
   - Crie um certificado APNs
2. No Xcode:
   - Selecione o projeto → Signing & Capabilities
   - Adicione **Push Notifications**

### ✅ Network Status
- Detecta quando o usuário está offline
- Mostra banner de aviso automático

### ✅ App State
- Monitora quando o app vai para background/foreground
- Útil para pausar/retomar conexões MQTT

### ✅ Status Bar
- Configurado com cor verde do tema
- Estilo adaptado para iOS

### ✅ Splash Screen
- Configurado para esconder automaticamente ao carregar

## 🎨 UI Mobile-First

O app foi otimizado para dispositivos móveis:

### ✨ Melhorias no Header:
- Logo com efeito de brilho animado
- Design responsivo com tamanhos adaptados
- Menu hambúrguer em mobile
- Safe area support para iOS (notch)

### 📊 Dashboard Otimizado:
- Tabs verticais em mobile para melhor usabilidade
- Cards com espaçamento otimizado
- Touch targets ≥ 44px
- Fontes legíveis em telas pequenas

### 🎯 Interações Touch:
- Feedback visual em todos os botões
- Prevenção de seleção de texto em UI
- Gestos otimizados

## 🔄 Sincronização & Offline

O app está preparado para funcionar offline:
- Detecção automática de status de rede
- Avisos visuais quando offline
- Cache de dados (via React Query)

## 📦 Build para Produção

### Android (APK/AAB):

```bash
# Build de produção
npm run build
npx cap sync android

# Abrir no Android Studio
npx cap open android

# No Android Studio:
# Build → Generate Signed Bundle/APK
```

### iOS (IPA):

```bash
# Build de produção
npm run build
npx cap sync ios

# Abrir no Xcode
npx cap open ios

# No Xcode:
# Product → Archive → Distribute App
```

## 🔐 Variáveis de Ambiente

O arquivo `.env` já está configurado com:
- `VITE_SUPABASE_URL`
- `VITE_SUPABASE_PUBLISHABLE_KEY`
- `VITE_SUPABASE_PROJECT_ID`

Não é necessário configurar nada adicional.

## 🐛 Troubleshooting

### Android

**Erro: "SDK location not found"**
```bash
# Criar arquivo local.properties em android/
echo "sdk.dir=/Users/SEU_USUARIO/Library/Android/sdk" > android/local.properties
```

**Build falha com "Gradle sync failed"**
- Abra o projeto no Android Studio
- File → Sync Project with Gradle Files

### iOS

**Erro: "Pod install failed"**
```bash
cd ios/App
pod install --repo-update
cd ../..
```

**Erro de assinatura de código**
- Abra o projeto no Xcode
- Selecione o target → Signing & Capabilities
- Configure seu Team

## 📚 Recursos Adicionais

- [Documentação do Capacitor](https://capacitorjs.com/docs)
- [Plugins do Capacitor](https://capacitorjs.com/docs/plugins)
- [Guia de Deploy Android](https://capacitorjs.com/docs/android/deploying-to-google-play)
- [Guia de Deploy iOS](https://capacitorjs.com/docs/ios/deploying-to-app-store)

## 🎯 Próximos Passos

1. ✅ Configurar builds Android/iOS
2. ⏳ Implementar sincronização offline avançada
3. ⏳ Adicionar mais notificações push contextuais
4. ⏳ Implementar biometria para login
5. ⏳ Otimizar performance de gráficos

---

**Desenvolvido com ❤️ por André Crepaldi**
