import { useEffect, useState } from 'react';
import { Network } from '@capacitor/network';
import { App } from '@capacitor/app';
import { StatusBar, Style } from '@capacitor/status-bar';
import { SplashScreen } from '@capacitor/splash-screen';

export const useCapacitor = () => {
  const [isOnline, setIsOnline] = useState(true);
  const [appState, setAppState] = useState<'active' | 'background'>('active');

  useEffect(() => {
    // Hide splash screen
    SplashScreen.hide();

    // Configure status bar
    const configureStatusBar = async () => {
      try {
        await StatusBar.setStyle({ style: Style.Light });
        await StatusBar.setBackgroundColor({ color: '#1a5d3a' });
      } catch (error) {
        console.log('Status bar not available:', error);
      }
    };

    configureStatusBar();

    // Monitor network status
    let networkListener: any;
    let appStateListener: any;

    const setupListeners = async () => {
      networkListener = await Network.addListener('networkStatusChange', (status) => {
        setIsOnline(status.connected);
      });

      appStateListener = await App.addListener('appStateChange', ({ isActive }) => {
        setAppState(isActive ? 'active' : 'background');
      });

      // Get initial network status
      const status = await Network.getStatus();
      setIsOnline(status.connected);
    };

    setupListeners();

    return () => {
      if (networkListener) networkListener.remove();
      if (appStateListener) appStateListener.remove();
    };
  }, []);

  return {
    isOnline,
    appState,
  };
};
