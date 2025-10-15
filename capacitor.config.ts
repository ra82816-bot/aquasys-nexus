import { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'com.lovable.aquasysnexus',
  appName: 'aquasys-nexus',
  webDir: 'dist',
  plugins: {
    BluetoothLe: {
      displayStrings: {
        scanning: "Escaneando dispositivos Bluetooth...",
        cancel: "Cancelar",
        availableDevices: "Dispositivos disponíveis",
        noDeviceFound: "Nenhum dispositivo encontrado"
      }
    }
  },
  android: {
    permissions: [
      "android.permission.BLUETOOTH",
      "android.permission.BLUETOOTH_ADMIN",
      "android.permission.BLUETOOTH_CONNECT",
      "android.permission.BLUETOOTH_SCAN",
      "android.permission.ACCESS_FINE_LOCATION",
      "android.permission.ACCESS_COARSE_LOCATION"
    ]
  },
  ios: {
    privacyDescriptions: {
      NSBluetoothAlwaysUsageDescription: "Este app precisa de acesso ao Bluetooth para se conectar aos módulos ESP32",
      NSBluetoothPeripheralUsageDescription: "Este app precisa de acesso ao Bluetooth para se conectar aos módulos ESP32",
      NSLocationWhenInUseUsageDescription: "Este app precisa de acesso à localização para escanear dispositivos Bluetooth próximos"
    }
  }
};

export default config;
