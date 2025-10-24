import { useNavigate } from "react-router-dom";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { DevicePairing } from "@/components/devices/DevicePairing";
import { DeviceList } from "@/components/devices/DeviceList";
import { DeviceCalibration } from "@/components/devices/DeviceCalibration";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";
import { useEffect, useState } from "react";

interface Device {
  id: string;
  device_uuid: string;
  device_type: string;
}

const Devices = () => {
  const navigate = useNavigate();
  const { toast } = useToast();
  const [devices, setDevices] = useState<Device[]>([]);

  useEffect(() => {
    loadDevices();
  }, []);

  const loadDevices = async () => {
    try {
      const { data } = await supabase
        .from("devices")
        .select("id, device_uuid, device_type");
      
      setDevices(data || []);
    } catch (error) {
      console.error("Erro ao carregar dispositivos:", error);
    }
  };

  const handleLogout = async () => {
    await supabase.auth.signOut();
    navigate("/auth");
  };

  const handleNavigate = (path: string) => {
    navigate(path);
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-background via-background to-primary/5">
      <AppHeader onLogout={handleLogout} onNavigate={handleNavigate} />
      
      <main className="container mx-auto px-4 py-8 space-y-8">
        <div className="space-y-2">
          <h1 className="text-3xl font-bold">Gerenciar Dispositivos</h1>
          <p className="text-muted-foreground">
            Vincule, monitore e calibre seus módulos ESP32
          </p>
        </div>

        <div className="grid gap-8 lg:grid-cols-3">
          <div className="lg:col-span-1 space-y-8">
            <DevicePairing />
            <DeviceCalibration devices={devices} />
          </div>
          
          <div className="lg:col-span-2">
            <DeviceList />
          </div>
        </div>
      </main>
    </div>
  );
};

export default Devices;