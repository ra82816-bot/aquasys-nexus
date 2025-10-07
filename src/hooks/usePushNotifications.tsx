import { useEffect } from 'react';
import { PushNotifications } from '@capacitor/push-notifications';
import { useToast } from '@/hooks/use-toast';

export const usePushNotifications = () => {
  const { toast } = useToast();

  useEffect(() => {
    const registerNotifications = async () => {
      try {
        // Request permission
        let permStatus = await PushNotifications.checkPermissions();

        if (permStatus.receive === 'prompt') {
          permStatus = await PushNotifications.requestPermissions();
        }

        if (permStatus.receive !== 'granted') {
          console.log('Push notification permission not granted');
          return;
        }

        // Register with Apple / Google to receive push notifications
        await PushNotifications.register();

        // On success, we should be able to receive notifications
        PushNotifications.addListener('registration', (token) => {
          console.log('Push registration success, token: ' + token.value);
          // Here you would send the token to your backend
        });

        // Some issue with our setup and push will not work
        PushNotifications.addListener('registrationError', (error) => {
          console.error('Error on registration: ' + JSON.stringify(error));
        });

        // Show us the notification payload if the app is open on our device
        PushNotifications.addListener('pushNotificationReceived', (notification) => {
          toast({
            title: notification.title || 'Notificação',
            description: notification.body || '',
          });
        });

        // Method called when tapping on a notification
        PushNotifications.addListener('pushNotificationActionPerformed', (notification) => {
          console.log('Push notification action performed', notification);
          // Handle navigation based on notification data
        });
      } catch (error) {
        console.log('Push notifications not available:', error);
      }
    };

    registerNotifications();

    return () => {
      PushNotifications.removeAllListeners();
    };
  }, [toast]);
};
