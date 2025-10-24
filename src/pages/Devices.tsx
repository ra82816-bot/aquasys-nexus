import { useNavigate } from "react-router-dom";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { DevicePairing } from "@/components/devices/DevicePairing";
import { DeviceList } from "@/components/devices/DeviceList";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";

const Devices = () => {
  const navigate = useNavigate();
  const { toast } = useToast();

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
            Vincule e monitore seus módulos ESP32
          </p>
        </div>

        <div className="grid gap-8 lg:grid-cols-3">
          <div className="lg:col-span-1">
            <DevicePairing />
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