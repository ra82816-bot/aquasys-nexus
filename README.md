# AquaSys Nexus - Smart Hydroponic Monitoring System

Sistema completo de monitoramento e automação para hidroponia com ESP32, sensores IoT, controle de relés e inteligência artificial.

## Project info

**URL**: https://lovable.dev/projects/7e63af5c-2336-45ef-8229-2ff1e2827bff

## How can I edit this code?

There are several ways of editing your application.

**Use Lovable**

Simply visit the [Lovable Project](https://lovable.dev/projects/7e63af5c-2336-45ef-8229-2ff1e2827bff) and start prompting.

Changes made via Lovable will be committed automatically to this repo.

**Use your preferred IDE**

If you want to work locally using your own IDE, you can clone this repo and push changes. Pushed changes will also be reflected in Lovable.

The only requirement is having Node.js & npm installed - [install with nvm](https://github.com/nvm-sh/nvm#installing-and-updating)

Follow these steps:

```sh
# Step 1: Clone the repository using the project's Git URL.
git clone <YOUR_GIT_URL>

# Step 2: Navigate to the project directory.
cd <YOUR_PROJECT_NAME>

# Step 3: Install the necessary dependencies.
npm i

# Step 4: Start the development server with auto-reloading and an instant preview.
npm run dev
```

**Edit a file directly in GitHub**

- Navigate to the desired file(s).
- Click the "Edit" button (pencil icon) at the top right of the file view.
- Make your changes and commit the changes.

**Use GitHub Codespaces**

- Navigate to the main page of your repository.
- Click on the "Code" button (green button) near the top right.
- Select the "Codespaces" tab.
- Click on "New codespace" to launch a new Codespace environment.
- Edit files directly within the Codespace and commit and push your changes once you're done.

## What technologies are used for this project?

This project is built with:

- Vite
- TypeScript
- React
- shadcn-ui
- Tailwind CSS

## How can I deploy this project?

Simply open [Lovable](https://lovable.dev/projects/7e63af5c-2336-45ef-8229-2ff1e2827bff) and click on Share -> Publish.

## Can I connect a custom domain to my Lovable project?

Yes, you can!

To connect a domain, navigate to Project > Settings > Domains and click Connect Domain.

Read more here: [Setting up a custom domain](https://docs.lovable.dev/features/custom-domain#custom-domain)

---

## 🔧 Troubleshooting ESP32

### Problemas SSL/TLS (-9984)

Se você encontrar erros de certificado SSL no ESP32:

```
(-9984) X509 - Certificate verification failed
```

**Soluções:**

1. **Debug rápido**: Habilite `SSL_INSECURE_MODE = true` no firmware (apenas para diagnóstico)
2. **Solução definitiva**: Verifique sincronização NTP antes da autenticação
3. **Documentação completa**: Consulte [TROUBLESHOOTING-SSL.md](./TROUBLESHOOTING-SSL.md)

### Outros Recursos

- [BLE Setup Guide](./BLE-SETUP.md) - Configuração Bluetooth Low Energy
- [MQTT Frontend Setup](./MQTT-FRONTEND-SETUP.md) - Integração MQTT no app
- [Device Configuration](./GUIA-CONFIGURACAO-DISPOSITIVO.md) - Guia de configuração de dispositivos
