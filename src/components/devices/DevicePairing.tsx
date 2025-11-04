import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { useToast } from "@/hooks/use-toast";
import { supabase } from "@/integrations/supabase/client";
import { Loader2, Smartphone, Wifi } from "lucide-react";

export const DevicePairing = () => {
  const [deviceUuid, setDeviceUuid] = useState("");
  const [deviceType, setDeviceType] = useState<"sensor" | "actuator">("sensor");
  const [firmwareVersion, setFirmwareVersion] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const { toast } = useToast();

  const handlePair = async () => {
    if (!deviceUuid.trim() || !firmwareVersion.trim()) {
      toast({
        title: "Campos obrigatórios",
        description: "Preencha todos os campos para continuar",
        variant: "destructive",
      });
      return;
    }

    setIsLoading(true);

    try {
      const { data, error } = await supabase.functions.invoke("device-pair", {
        body: {
          device_uuid: deviceUuid.trim(),
          device_type: deviceType,
          firmware_version: firmwareVersion.trim(),
        },
      });

      if (error) throw error;

      if (data.error) {
        if (data.code === "DEVICE_ALREADY_PAIRED") {
          toast({
            title: "Dispositivo já vinculado",
            description: "Este dispositivo já está vinculado a outra conta",
            variant: "destructive",
          });
        } else {
          throw new Error(data.error);
        }
        return;
      }

      // Verificar se já estava vinculado a este usuário
      if (data.already_paired) {
        toast({
          title: "Dispositivo já vinculado",
          description: "Este dispositivo já está na sua conta",
        });
        
        // Recarregar página para atualizar lista
        setTimeout(() => window.location.reload(), 1500);
        return;
      }

      toast({
        title: "Sucesso!",
        description: data.message || "Dispositivo vinculado com sucesso",
      });

      // Limpar formulário
      setDeviceUuid("");
      setFirmwareVersion("");
      
      // Recarregar página para atualizar lista de dispositivos
      setTimeout(() => window.location.reload(), 1500);

    } catch (error: any) {
      console.error("Erro ao vincular dispositivo:", error);
      toast({
        title: "Erro ao vincular",
        description: error.message || "Ocorreu um erro ao vincular o dispositivo",
        variant: "destructive",
      });
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Wifi className="h-5 w-5" />
          Vincular Novo Dispositivo
        </CardTitle>
        <CardDescription>
          Escaneie o QR Code ou digite manualmente o UUID do dispositivo
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="space-y-2">
          <Label htmlFor="device-uuid">UUID do Dispositivo</Label>
          <Input
            id="device-uuid"
            placeholder="ACT-XXXXXXXXXXXX ou SEN-XXXXXXXXXXXX"
            value={deviceUuid}
            onChange={(e) => setDeviceUuid(e.target.value.toUpperCase())}
            disabled={isLoading}
          />
          <p className="text-sm text-muted-foreground">
            Formato: ACT-XXXXXXXXXXXX (Atuador) ou SEN-XXXXXXXXXXXX (Sensor)
          </p>
        </div>

        <div className="space-y-2">
          <Label htmlFor="device-type">Tipo de Dispositivo</Label>
          <div className="flex gap-2">
            <Button
              type="button"
              variant={deviceType === "sensor" ? "default" : "outline"}
              onClick={() => setDeviceType("sensor")}
              disabled={isLoading}
              className="flex-1"
            >
              <Smartphone className="mr-2 h-4 w-4" />
              Módulo de Sensores
            </Button>
            <Button
              type="button"
              variant={deviceType === "actuator" ? "default" : "outline"}
              onClick={() => setDeviceType("actuator")}
              disabled={isLoading}
              className="flex-1"
            >
              <Wifi className="mr-2 h-4 w-4" />
              Módulo de Atuadores
            </Button>
          </div>
        </div>

        <div className="space-y-2">
          <Label htmlFor="firmware-version">Versão do Firmware</Label>
          <Input
            id="firmware-version"
            placeholder="3.0"
            value={firmwareVersion}
            onChange={(e) => setFirmwareVersion(e.target.value)}
            disabled={isLoading}
          />
        </div>

        <Button 
          onClick={handlePair} 
          disabled={isLoading}
          className="w-full"
        >
          {isLoading ? (
            <>
              <Loader2 className="mr-2 h-4 w-4 animate-spin" />
              Vinculando...
            </>
          ) : (
            "Vincular Dispositivo"
          )}
        </Button>

        <div className="bg-muted p-4 rounded-lg space-y-2">
          <p className="text-sm font-medium">Como encontrar o UUID:</p>
          <ol className="text-sm text-muted-foreground space-y-1 list-decimal list-inside">
            <li>Ligue o módulo ESP32</li>
            <li>O UUID aparecerá no Serial Monitor (formato: ACT-XXXX ou SEN-XXXX)</li>
            <li>Ou acesse o modo AP e visualize na página web</li>
            <li>Digite o UUID acima para vincular à sua conta</li>
          </ol>
          <div className="mt-3 pt-3 border-t">
            <p className="text-sm font-medium">Exemplo:</p>
            <code className="text-xs bg-background px-2 py-1 rounded">ACT-6CC84005C7C0</code>
          </div>
        </div>
      </CardContent>
    </Card>
  );
};