import { useState } from "react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Separator } from "@/components/ui/separator";
import { useMqttContext } from "@/contexts/MqttContext";
import { useToast } from "@/hooks/use-toast";
import { Beaker, Droplet, Zap, Info, AlertCircle } from "lucide-react";
import { Alert, AlertDescription } from "@/components/ui/alert";

export const SensorCalibration = () => {
  const { isConnected, publish } = useMqttContext();
  const { toast } = useToast();
  
  // Estados para valores customizáveis de EC
  const [ecLowValue, setEcLowValue] = useState("360");
  const [ecHighValue, setEcHighValue] = useState("4588");
  const [isCalibrating, setIsCalibrating] = useState(false);

  const sendCalibrationCommand = async (type: string, ecValue?: number) => {
    if (!isConnected) {
      toast({
        title: "MQTT Desconectado",
        description: "Conecte-se ao broker MQTT primeiro",
        variant: "destructive",
      });
      return;
    }

    setIsCalibrating(true);

    try {
      const command: any = { calibrate: type };
      
      if (ecValue !== undefined) {
        command.ec_value = ecValue;
      }

      await publish("aquasys/sensors/command", command);

      toast({
        title: "Comando Enviado",
        description: `Calibração ${type} iniciada no ESP32`,
      });
    } catch (error) {
      console.error("Erro ao enviar comando:", error);
      toast({
        title: "Erro",
        description: "Falha ao enviar comando de calibração",
        variant: "destructive",
      });
    } finally {
      setTimeout(() => setIsCalibrating(false), 2000);
    }
  };

  const handlePhCalibration = (phValue: "7" | "4") => {
    const type = phValue === "7" ? "ph7" : "ph4";
    sendCalibrationCommand(type);
  };

  const handleEcCalibration = (level: "low" | "high") => {
    const ecValue = level === "low" ? parseFloat(ecLowValue) : parseFloat(ecHighValue);
    
    if (isNaN(ecValue) || ecValue < 0 || ecValue > 5000) {
      toast({
        title: "Valor Inválido",
        description: "Digite um valor EC entre 0 e 5000 µS/cm",
        variant: "destructive",
      });
      return;
    }

    const type = level === "low" ? "ec_low" : "ec_high";
    sendCalibrationCommand(type, ecValue);
  };

  return (
    <Card className="border-primary/20 bg-card/50 backdrop-blur">
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Beaker className="h-5 w-5 text-primary" />
          Calibração de Sensores
        </CardTitle>
        <CardDescription>
          Configure os valores de referência e calibre os sensores
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-6">
        {/* Status MQTT */}
        <Alert variant={isConnected ? "default" : "destructive"}>
          <AlertCircle className="h-4 w-4" />
          <AlertDescription>
            {isConnected ? "✓ MQTT Conectado - Pronto para calibrar" : "✗ MQTT Desconectado - Conecte-se primeiro"}
          </AlertDescription>
        </Alert>

        {/* Instruções */}
        <Alert>
          <Info className="h-4 w-4" />
          <AlertDescription className="text-sm">
            <strong>Como calibrar:</strong>
            <ol className="list-decimal list-inside mt-2 space-y-1">
              <li>Mergulhe o sensor na solução de calibração</li>
              <li>Aguarde 30 segundos para estabilizar</li>
              <li>Clique no botão correspondente para calibrar</li>
            </ol>
          </AlertDescription>
        </Alert>

        <Separator />

        {/* Calibração de pH */}
        <div className="space-y-4">
          <div className="flex items-center gap-2">
            <Droplet className="h-5 w-5 text-blue-500" />
            <h3 className="text-lg font-semibold">Calibração de pH</h3>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div className="space-y-2">
              <Label className="text-sm font-medium">Solução pH 7.0 (Neutra)</Label>
              <Button
                onClick={() => handlePhCalibration("7")}
                disabled={!isConnected || isCalibrating}
                className="w-full"
                variant="outline"
              >
                {isCalibrating ? "Calibrando..." : "Calibrar pH 7.0"}
              </Button>
              <p className="text-xs text-muted-foreground">
                Use solução tampão pH 7.0 a 25°C
              </p>
            </div>

            <div className="space-y-2">
              <Label className="text-sm font-medium">Solução pH 4.0 (Ácida)</Label>
              <Button
                onClick={() => handlePhCalibration("4")}
                disabled={!isConnected || isCalibrating}
                className="w-full"
                variant="outline"
              >
                {isCalibrating ? "Calibrando..." : "Calibrar pH 4.0"}
              </Button>
              <p className="text-xs text-muted-foreground">
                Use solução tampão pH 4.0 a 25°C
              </p>
            </div>
          </div>
        </div>

        <Separator />

        {/* Calibração de EC */}
        <div className="space-y-4">
          <div className="flex items-center gap-2">
            <Zap className="h-5 w-5 text-yellow-500" />
            <h3 className="text-lg font-semibold">Calibração de EC / TDS</h3>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {/* EC Baixa */}
            <div className="space-y-3 p-4 border rounded-lg bg-card">
              <Label className="text-sm font-medium">EC Baixa (Low)</Label>
              
              <div className="space-y-2">
                <Label htmlFor="ec-low" className="text-xs text-muted-foreground">
                  Valor da Solução (µS/cm):
                </Label>
                <Input
                  id="ec-low"
                  type="number"
                  value={ecLowValue}
                  onChange={(e) => setEcLowValue(e.target.value)}
                  placeholder="Ex: 360"
                  min="0"
                  max="5000"
                  step="10"
                  className="text-base"
                />
              </div>

              <Button
                onClick={() => handleEcCalibration("low")}
                disabled={!isConnected || isCalibrating}
                className="w-full"
                variant="outline"
              >
                {isCalibrating ? "Calibrando..." : "Calibrar EC Baixa"}
              </Button>
              
              <p className="text-xs text-muted-foreground">
                Solução típica: 360 µS/cm a 25°C
              </p>
            </div>

            {/* EC Alta */}
            <div className="space-y-3 p-4 border rounded-lg bg-card">
              <Label className="text-sm font-medium">EC Alta (High)</Label>
              
              <div className="space-y-2">
                <Label htmlFor="ec-high" className="text-xs text-muted-foreground">
                  Valor da Solução (µS/cm):
                </Label>
                <Input
                  id="ec-high"
                  type="number"
                  value={ecHighValue}
                  onChange={(e) => setEcHighValue(e.target.value)}
                  placeholder="Ex: 4588"
                  min="0"
                  max="5000"
                  step="10"
                  className="text-base"
                />
              </div>

              <Button
                onClick={() => handleEcCalibration("high")}
                disabled={!isConnected || isCalibrating}
                className="w-full"
                variant="outline"
              >
                {isCalibrating ? "Calibrando..." : "Calibrar EC Alta"}
              </Button>
              
              <p className="text-xs text-muted-foreground">
                Solução típica: 4588 µS/cm a 25°C
              </p>
            </div>
          </div>

          <Alert>
            <Info className="h-4 w-4" />
            <AlertDescription className="text-xs">
              <strong>Dica:</strong> Você pode usar qualquer solução de referência! Basta inserir o valor exato 
              da EC da sua solução (conforme indicado na embalagem) antes de calibrar.
            </AlertDescription>
          </Alert>
        </div>

        <Separator />

        {/* Observações */}
        <div className="text-xs text-muted-foreground space-y-1">
          <p><strong>Observações importantes:</strong></p>
          <ul className="list-disc list-inside space-y-1 ml-2">
            <li>Sempre calibre pH primeiro (7.0, depois 4.0)</li>
            <li>Limpe os sensores entre calibrações</li>
            <li>Mantenha temperatura constante (~25°C)</li>
            <li>Recalibre a cada 30 dias para melhor precisão</li>
          </ul>
        </div>
      </CardContent>
    </Card>
  );
};
