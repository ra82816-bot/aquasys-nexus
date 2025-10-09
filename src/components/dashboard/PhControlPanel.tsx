import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Alert, AlertDescription } from "@/components/ui/alert";
import { Droplet, TrendingUp, TrendingDown, Clock, AlertTriangle } from "lucide-react";
import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, ReferenceLine } from "recharts";

interface PhReading {
  ph: number;
  timestamp: string;
}

interface PhLog {
  timestamp: string;
  action: string;
  ph_average: number;
}

export const PhControlPanel = () => {
  const [phHistory, setPhHistory] = useState<PhReading[]>([]);
  const [phAverage, setPhAverage] = useState<number | null>(null);
  const [lastCorrection, setLastCorrection] = useState<PhLog | null>(null);
  const [phRange, setPhRange] = useState({ low: 5.8, high: 6.5 });

  useEffect(() => {
    fetchPhData();
    const interval = setInterval(fetchPhData, 60000); // Atualiza a cada minuto
    return () => clearInterval(interval);
  }, []);

  const fetchPhData = async () => {
    try {
      // Buscar últimas 24 leituras de pH (1 por hora)
      const { data: readings, error: readingsError } = await supabase
        .from('readings')
        .select('ph, timestamp')
        .order('timestamp', { ascending: false })
        .limit(24);

      if (readingsError) throw readingsError;

      if (readings && readings.length > 0) {
        setPhHistory(readings.reverse());
        
        // Calcular média das últimas 24h
        const sum = readings.reduce((acc, r) => acc + r.ph, 0);
        const avg = sum / readings.length;
        setPhAverage(avg);
      }

      // Buscar última correção de pH nos logs
      const { data: logs, error: logsError } = await supabase
        .from('event_logs')
        .select('*')
        .or('message.ilike.%pH Up%,message.ilike.%pH Down%,message.ilike.%Correção pH%')
        .order('timestamp', { ascending: false })
        .limit(1)
        .maybeSingle();

      if (!logsError && logs) {
        setLastCorrection({
          timestamp: logs.timestamp,
          action: logs.message,
          ph_average: phAverage || 0
        });
      }

      // Buscar configuração de limites de pH
      const { data: config, error: configError } = await supabase
        .from('relay_configs')
        .select('ph_threshold_low, ph_threshold_high')
        .or('mode.eq.ph_up,mode.eq.ph_down')
        .limit(1)
        .maybeSingle();

      if (!configError && config) {
        setPhRange({
          low: config.ph_threshold_low || 5.8,
          high: config.ph_threshold_high || 6.5
        });
      }
    } catch (error) {
      console.error('Erro ao buscar dados de pH:', error);
    }
  };

  const getPhStatus = () => {
    if (!phAverage) return { status: 'unknown', color: 'bg-muted', text: 'Aguardando dados' };
    
    if (phAverage < phRange.low) {
      return { status: 'low', color: 'bg-red-500/10 text-red-500 border-red-500/20', text: 'pH Baixo', icon: TrendingDown };
    } else if (phAverage > phRange.high) {
      return { status: 'high', color: 'bg-orange-500/10 text-orange-500 border-orange-500/20', text: 'pH Alto', icon: TrendingUp };
    }
    return { status: 'ok', color: 'bg-green-500/10 text-green-500 border-green-500/20', text: 'pH Ideal', icon: Droplet };
  };

  const phStatus = getPhStatus();
  const StatusIcon = phStatus.icon || Droplet;

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center justify-between">
          <CardTitle className="flex items-center gap-2">
            <Droplet className="h-5 w-5 text-primary" />
            Controle Automático de pH
          </CardTitle>
          <Badge variant="outline" className={`border ${phStatus.color}`}>
            <StatusIcon className="h-3 w-3 mr-1" />
            {phStatus.text}
          </Badge>
        </div>
      </CardHeader>
      <CardContent className="space-y-4">
        {/* Estatísticas de pH */}
        <div className="grid grid-cols-2 md:grid-cols-3 gap-3">
          <div className="bg-card border rounded-lg p-3">
            <p className="text-xs text-muted-foreground">pH Atual</p>
            <p className="text-2xl font-bold">
              {phHistory.length > 0 ? phHistory[phHistory.length - 1].ph.toFixed(2) : '--'}
            </p>
          </div>
          <div className="bg-card border rounded-lg p-3">
            <p className="text-xs text-muted-foreground">Média 24h</p>
            <p className="text-2xl font-bold">
              {phAverage ? phAverage.toFixed(2) : '--'}
            </p>
          </div>
          <div className="bg-card border rounded-lg p-3 col-span-2 md:col-span-1">
            <p className="text-xs text-muted-foreground">Faixa Ideal</p>
            <p className="text-lg font-semibold">
              {phRange.low.toFixed(1)} - {phRange.high.toFixed(1)}
            </p>
          </div>
        </div>

        {/* Gráfico de pH */}
        {phHistory.length > 0 && (
          <div className="h-48">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={phHistory}>
                <CartesianGrid strokeDasharray="3 3" opacity={0.3} />
                <XAxis 
                  dataKey="timestamp" 
                  tickFormatter={(value) => new Date(value).toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' })}
                  tick={{ fontSize: 10 }}
                />
                <YAxis 
                  domain={[5, 7]} 
                  ticks={[5.0, 5.5, 6.0, 6.5, 7.0]}
                  tick={{ fontSize: 10 }}
                />
                <Tooltip 
                  labelFormatter={(value) => new Date(value).toLocaleString('pt-BR')}
                  formatter={(value: number) => [value.toFixed(2), 'pH']}
                />
                <ReferenceLine y={phRange.low} stroke="#ef4444" strokeDasharray="3 3" label="Mín" />
                <ReferenceLine y={phRange.high} stroke="#ef4444" strokeDasharray="3 3" label="Máx" />
                <Line 
                  type="monotone" 
                  dataKey="ph" 
                  stroke="hsl(var(--primary))" 
                  strokeWidth={2}
                  dot={{ r: 3 }}
                  activeDot={{ r: 5 }}
                />
              </LineChart>
            </ResponsiveContainer>
          </div>
        )}

        {/* Alertas e status */}
        {phAverage && (phAverage < phRange.low || phAverage > phRange.high) && (
          <Alert className="border-yellow-500/50 bg-yellow-500/10">
            <AlertTriangle className="h-4 w-4 text-yellow-500" />
            <AlertDescription className="text-yellow-600">
              {phAverage < phRange.low ? (
                <>pH abaixo do ideal. Sistema acionará correção automática às 6h (pH Up)</>
              ) : (
                <>pH acima do ideal. Sistema acionará correção automática às 6h (pH Down)</>
              )}
            </AlertDescription>
          </Alert>
        )}

        {/* Última correção */}
        {lastCorrection && (
          <div className="bg-muted/50 rounded-lg p-3 text-sm">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <Clock className="h-4 w-4" />
              <span className="font-medium">Última Correção</span>
            </div>
            <p className="text-xs text-muted-foreground mb-1">
              {new Date(lastCorrection.timestamp).toLocaleString('pt-BR')}
            </p>
            <p className="text-sm">{lastCorrection.action}</p>
          </div>
        )}

        {/* Informações do sistema */}
        <div className="text-xs text-muted-foreground space-y-1 bg-muted/30 rounded-lg p-3">
          <p className="font-medium mb-2">ℹ️ Funcionamento Automático:</p>
          <p>• Verificação diária às 6h da manhã</p>
          <p>• Correção baseada na média das últimas 24 horas</p>
          <p>• Pulso de correção limitado a 5 segundos</p>
          <p>• Apenas uma correção por dia</p>
        </div>
      </CardContent>
    </Card>
  );
};
