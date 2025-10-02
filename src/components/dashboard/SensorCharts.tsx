import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from "recharts";
import { Loader2 } from "lucide-react";

export const SensorCharts = () => {
  const [data, setData] = useState<any[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetchHistoricalData();
  }, []);

  const fetchHistoricalData = async () => {
    setLoading(true);
    try {
      // Buscar últimas 100 leituras
      const { data: readings, error } = await supabase
        .from("readings")
        .select("*")
        .order("timestamp", { ascending: false })
        .limit(100);

      if (error) throw error;

      // Reverter para ordem cronológica e formatar dados
      const formattedData = readings?.reverse().map((reading) => ({
        timestamp: new Date(reading.timestamp).toLocaleTimeString('pt-BR', {
          hour: '2-digit',
          minute: '2-digit'
        }),
        pH: reading.ph,
        EC: reading.ec,
        'Temp. Ar': reading.air_temp,
        'Umidade': reading.humidity,
        'Temp. Água': reading.water_temp,
      })) || [];

      setData(formattedData);
    } catch (error) {
      console.error("Erro ao buscar dados históricos:", error);
    } finally {
      setLoading(false);
    }
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center py-12">
        <Loader2 className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  if (data.length === 0) {
    return (
      <div className="text-center py-12 text-muted-foreground">
        Nenhum dado disponível ainda
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Gráfico de pH */}
      <Card>
        <CardHeader>
          <CardTitle>pH ao longo do tempo</CardTitle>
        </CardHeader>
        <CardContent>
          <ResponsiveContainer width="100%" height={300}>
            <LineChart data={data}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis dataKey="timestamp" />
              <YAxis domain={[0, 14]} />
              <Tooltip />
              <Legend />
              <Line type="monotone" dataKey="pH" stroke="#3b82f6" strokeWidth={2} dot={false} />
            </LineChart>
          </ResponsiveContainer>
        </CardContent>
      </Card>

      {/* Gráfico de EC */}
      <Card>
        <CardHeader>
          <CardTitle>Condutividade Elétrica (EC)</CardTitle>
        </CardHeader>
        <CardContent>
          <ResponsiveContainer width="100%" height={300}>
            <LineChart data={data}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis dataKey="timestamp" />
              <YAxis />
              <Tooltip />
              <Legend />
              <Line type="monotone" dataKey="EC" stroke="#eab308" strokeWidth={2} dot={false} />
            </LineChart>
          </ResponsiveContainer>
        </CardContent>
      </Card>

      {/* Gráfico de Temperaturas */}
      <Card>
        <CardHeader>
          <CardTitle>Temperaturas</CardTitle>
        </CardHeader>
        <CardContent>
          <ResponsiveContainer width="100%" height={300}>
            <LineChart data={data}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis dataKey="timestamp" />
              <YAxis />
              <Tooltip />
              <Legend />
              <Line type="monotone" dataKey="Temp. Ar" stroke="#f97316" strokeWidth={2} dot={false} />
              <Line type="monotone" dataKey="Temp. Água" stroke="#14b8a6" strokeWidth={2} dot={false} />
            </LineChart>
          </ResponsiveContainer>
        </CardContent>
      </Card>

      {/* Gráfico de Umidade */}
      <Card>
        <CardHeader>
          <CardTitle>Umidade</CardTitle>
        </CardHeader>
        <CardContent>
          <ResponsiveContainer width="100%" height={300}>
            <LineChart data={data}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis dataKey="timestamp" />
              <YAxis domain={[0, 100]} />
              <Tooltip />
              <Legend />
              <Line type="monotone" dataKey="Umidade" stroke="#06b6d4" strokeWidth={2} dot={false} />
            </LineChart>
          </ResponsiveContainer>
        </CardContent>
      </Card>
    </div>
  );
};
