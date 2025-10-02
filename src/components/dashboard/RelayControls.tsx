import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Settings } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { RelayConfigDialog } from "./RelayConfigDialog";

interface RelayConfig {
  relay_index: number;
  mode: string;
  [key: string]: any;
}

export const RelayControls = () => {
  const [relayConfigs, setRelayConfigs] = useState<RelayConfig[]>([]);
  const [selectedRelay, setSelectedRelay] = useState<number | null>(null);
  const { toast } = useToast();

  useEffect(() => {
    fetchRelayConfigs();
  }, []);

  const fetchRelayConfigs = async () => {
    const { data, error } = await supabase
      .from("relay_configs")
      .select("*")
      .order("relay_index");

    if (error) {
      toast({
        title: "Erro ao carregar configurações",
        description: error.message,
        variant: "destructive"
      });
      return;
    }

    setRelayConfigs(data || []);
  };

  const getModeLabel = (mode: string) => {
    const modes: Record<string, string> = {
      unused: "Não Usado",
      led: "Iluminação",
      cycle: "Ciclo",
      ph_up: "pH Up",
      ph_down: "pH Down",
      temperature: "Temperatura",
      humidity: "Umidade",
      ec: "Condutividade"
    };
    return modes[mode] || mode;
  };

  return (
    <section>
      <h2 className="text-2xl font-semibold mb-4 text-foreground flex items-center gap-2">
        <Settings className="h-6 w-6 text-primary" />
        Configuração dos Relés
      </h2>
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        {relayConfigs.map((config) => (
          <Card
            key={config.relay_index}
            className="border-primary/20 hover:border-primary/40 transition-all cursor-pointer"
            onClick={() => setSelectedRelay(config.relay_index)}
          >
            <CardHeader className="pb-3">
              <CardTitle className="text-sm">Relé {config.relay_index + 1}</CardTitle>
              <CardDescription>{getModeLabel(config.mode)}</CardDescription>
            </CardHeader>
            <CardContent>
              <Button variant="outline" size="sm" className="w-full gap-2">
                <Settings className="h-4 w-4" />
                Configurar
              </Button>
            </CardContent>
          </Card>
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