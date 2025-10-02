import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Button } from "@/components/ui/button";
import { Settings, Download } from "lucide-react";
import { RelayConfigDialog } from "./RelayConfigDialog";
import { RelayCard } from "./RelayCard";
import { useToast } from "@/hooks/use-toast";

interface RelayConfig {
  relay_index: number;
  mode: string;
  [key: string]: any;
}

interface RelayStatus {
  relay1_led: boolean;
  relay2_pump: boolean;
  relay3_ph_up: boolean;
  relay4_fan: boolean;
  relay5_humidity: boolean;
  relay6_ec: boolean;
  relay7_co2: boolean;
  relay8_generic: boolean;
}

export const RelayControls = () => {
  const [relayConfigs, setRelayConfigs] = useState<RelayConfig[]>([]);
  const [selectedRelay, setSelectedRelay] = useState<number | null>(null);
  const [relayStatus, setRelayStatus] = useState<RelayStatus | null>(null);
  const { toast } = useToast();


  useEffect(() => {
    fetchRelayConfigs();
    fetchLatestRelayStatus();

    // Subscrever ao status em tempo real
    const channel = supabase
      .channel('relay-status-changes')
      .on(
        'postgres_changes',
        { event: 'INSERT', schema: 'public', table: 'relay_status' },
        (payload) => setRelayStatus(payload.new as RelayStatus)
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  }, []);

  const fetchRelayConfigs = async () => {
    const { data, error } = await supabase
      .from("relay_configs")
      .select("*")
      .order("relay_index");

    if (error) {
      console.error("Erro ao buscar configurações dos relés:", error);
      return;
    }

    setRelayConfigs(data || []);
  };

  const fetchLatestRelayStatus = async () => {
    const { data, error } = await supabase
      .from("relay_status")
      .select("*")
      .order("timestamp", { ascending: false })
      .limit(1)
      .single();

    if (error && error.code !== "PGRST116") {
      console.error("Erro ao buscar status dos relés:", error);
      return;
    }

    if (data) setRelayStatus(data);
  };

  const handleExportReadings = async () => {
    try {
      const startDate = new Date();
      startDate.setDate(startDate.getDate() - 30); // Últimos 30 dias

      const response = await supabase.functions.invoke('export-readings', {
        body: {
          start_date: startDate.toISOString(),
          end_date: new Date().toISOString()
        }
      });

      if (response.error) throw response.error;

      // Criar link para download
      const blob = new Blob([response.data], { type: 'text/csv' });
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `leituras_aquasys_${new Date().toISOString().split('T')[0]}.csv`;
      document.body.appendChild(a);
      a.click();
      window.URL.revokeObjectURL(url);
      document.body.removeChild(a);

      toast({
        title: "Exportação concluída",
        description: "Relatório baixado com sucesso"
      });
    } catch (error) {
      console.error('Erro ao exportar:', error);
      toast({
        title: "Erro",
        description: "Falha ao exportar relatório",
        variant: "destructive"
      });
    }
  };

  const getRelayStatus = (index: number): boolean => {
    if (!relayStatus) return false;
    const statusKeys: Record<number, keyof RelayStatus> = {
      0: 'relay1_led',
      1: 'relay2_pump',
      2: 'relay3_ph_up',
      3: 'relay4_fan',
      4: 'relay5_humidity',
      5: 'relay6_ec',
      6: 'relay7_co2',
      7: 'relay8_generic'
    };
    return relayStatus[statusKeys[index]] || false;
  };

  return (
    <section>
      <div className="flex items-center justify-between mb-4">
        <h2 className="text-2xl font-semibold text-foreground flex items-center gap-2">
          <Settings className="h-6 w-6 text-primary" />
          Controle dos Relés
        </h2>
        <Button onClick={handleExportReadings} variant="outline" size="sm" className="gap-2">
          <Download className="h-4 w-4" />
          Exportar Relatório
        </Button>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        {relayConfigs.map((config) => (
          <div key={config.relay_index} className="relative">
            <RelayCard
              relayIndex={config.relay_index}
              name={config.name || `Relé ${config.relay_index + 1}`}
              mode={config.mode}
              isOn={getRelayStatus(config.relay_index)}
              onNameUpdate={fetchRelayConfigs}
            />
            <Button
              variant="ghost"
              size="sm"
              onClick={() => setSelectedRelay(config.relay_index)}
              className="absolute top-2 right-2 h-8 w-8 p-0"
            >
              <Settings className="h-4 w-4" />
            </Button>
          </div>
        ))}
      </div>

      {selectedRelay !== null && (
        <RelayConfigDialog
          relayIndex={selectedRelay}
          config={relayConfigs.find(c => c.relay_index === selectedRelay) || null}
          open={selectedRelay !== null}
          onOpenChange={(open) => !open && setSelectedRelay(null)}
          onSave={() => {
            fetchRelayConfigs();
            setSelectedRelay(null);
          }}
        />
      )}
    </section>
  );
};
