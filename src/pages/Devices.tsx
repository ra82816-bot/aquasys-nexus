import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { MqttFooter } from "@/components/dashboard/MqttFooter";
import { DevicesList } from "@/components/devices/DevicesList";
import { AddDeviceDialog } from "@/components/devices/AddDeviceDialog";
import { Button } from "@/components/ui/button";
import { Plus } from "lucide-react";
import { useToast } from "@/hooks/use-toast";

interface Device {
  id: string;
  device_uuid: string;
  device_type: 'sensor' | 'actuator';
  firmware_version: string | null;
  first_seen_at: string | null;
  last_seen_at: string | null;
  paired_at: string;
}

const Devices = () => {
  const navigate = useNavigate();
  const { toast } = useToast();
  const [devices, setDevices] = useState<Device[]>([]);
  const [loading, setLoading] = useState(true);
  const [isAddDialogOpen, setIsAddDialogOpen] = useState(false);

  const fetchDevices = async () => {
    try {
      const { data: { user } } = await supabase.auth.getUser();
      if (!user) {
        navigate("/auth");
        return;
      }

      // Fetch devices owned by user with join
      const { data, error } = await supabase
        .from('device_owners')
        .select(`
          paired_at,
          devices (
            id,
            device_uuid,
            device_type,
            firmware_version,
            first_seen_at,
            last_seen_at
          )
        `)
        .eq('user_id', user.id);

      if (error) {
        console.error('Error fetching devices:', error);
        toast({
          title: "Erro",
          description: "Não foi possível carregar os dispositivos",
          variant: "destructive",
        });
        return;
      }

      // Transform data
      const transformedDevices = data?.map(item => ({
        ...item.devices,
        paired_at: item.paired_at,
      })) as Device[] || [];

      setDevices(transformedDevices);
    } catch (error) {
      console.error('Error:', error);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchDevices();
  }, []);

  const handleLogout = async () => {
    await supabase.auth.signOut();
    navigate("/auth");
  };

  const handleDeviceRegistered = () => {
    setIsAddDialogOpen(false);
    fetchDevices();
    toast({
      title: "Sucesso",
      description: "Dispositivo registrado com sucesso!",
    });
  };

  const handleRemoveDevice = async (deviceId: string) => {
    try {
      const { error } = await supabase
        .from('device_owners')
        .delete()
        .eq('device_id', deviceId);

      if (error) throw error;

      toast({
        title: "Dispositivo removido",
        description: "O dispositivo foi desvinculado da sua conta",
      });
      fetchDevices();
    } catch (error) {
      console.error('Error removing device:', error);
      toast({
        title: "Erro",
        description: "Não foi possível remover o dispositivo",
        variant: "destructive",
      });
    }
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-background via-background to-primary/5 flex flex-col">
      <AppHeader onLogout={handleLogout} onNavigate={navigate} />
      
      <main className="flex-1 container mx-auto px-4 py-6 pb-20">
        <div className="flex items-center justify-between mb-6">
          <div>
            <h2 className="text-2xl font-bold text-foreground">Dispositivos</h2>
            <p className="text-muted-foreground">Gerencie seus módulos sensor e atuador</p>
          </div>
          <Button onClick={() => setIsAddDialogOpen(true)} className="gap-2">
            <Plus className="h-4 w-4" />
            Adicionar
          </Button>
        </div>

        <DevicesList 
          devices={devices} 
          loading={loading}
          onRemove={handleRemoveDevice}
        />
      </main>

      <MqttFooter />

      <AddDeviceDialog 
        open={isAddDialogOpen}
        onOpenChange={setIsAddDialogOpen}
        onSuccess={handleDeviceRegistered}
      />
    </div>
  );
};

export default Devices;
