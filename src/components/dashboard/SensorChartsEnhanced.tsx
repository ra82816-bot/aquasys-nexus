import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, ReferenceLine } from "recharts";
import { Loader2, BarChart3, AlertTriangle } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import { subDays } from "date-fns";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { ChartFilters } from "./ChartFilters";
import { ChartAnalysis } from "./ChartAnalysis";
import { ReportGenerator } from "./ReportGenerator";
import { AIInsightsPanel } from "./AIInsightsPanel";
import { useChartAnalytics, SensorReading } from "@/hooks/useChartAnalytics";

export const SensorChartsEnhanced = () => {
  const [data, setData] = useState<SensorReading[]>([]);
  const [loading, setLoading] = useState(true);
  const [startDate, setStartDate] = useState<Date>(subDays(new Date(), 7));
  const [endDate, setEndDate] = useState<Date>(new Date());
  const [lastDataTimestamp, setLastDataTimestamp] = useState<string | null>(null);

  // Ideal ranges for hydroponic cultivation (example values)
  const idealRanges = {
    ph: { min: 5.5, max: 6.5 },
    ec: { min: 800, max: 1500 },
    temp: { min: 18, max: 26 },
    humidity: { min: 50, max: 70 },
  };

  const analytics = useChartAnalytics(data, idealRanges);

  useEffect(() => {
    fetchHistoricalData();

    const channel = supabase
      .channel("readings-realtime-enhanced")
      .on(
        "postgres_changes",
        { event: "INSERT", schema: "public", table: "readings" },
        () => {
          console.log("Nova leitura recebida, atualizando...");
          fetchHistoricalData();
        }
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  }, [startDate, endDate]);

  const fetchHistoricalData = async () => {
    setLoading(true);
    try {
      const startTimestamp = startDate.toISOString();
      const endTimestamp = endDate.toISOString();

      console.log('📊 Buscando dados de', startTimestamp, 'até', endTimestamp);

      const { data: readings, error } = await supabase
        .from("readings")
        .select("*")
        .gte("timestamp", startTimestamp)
        .lte("timestamp", endTimestamp)
        .order("timestamp", { ascending: true });

      if (error) throw error;

      if (!readings || readings.length === 0) {
        console.log('⚠️ Nenhum dado encontrado para o período');
        setData([]);
        setLastDataTimestamp(null);
        return;
      }

      console.log(`✅ ${readings.length} leituras carregadas`);
      const sortedReadings = readings as SensorReading[];
      setData(sortedReadings);
      
      // Get the most recent reading timestamp
      if (sortedReadings.length > 0) {
        setLastDataTimestamp(sortedReadings[sortedReadings.length - 1].timestamp);
      }
    } catch (error) {
      console.error("❌ Erro ao buscar dados:", error);
    } finally {
      setLoading(false);
    }
  };

  const handleQuickFilter = (days: number) => {
    setStartDate(subDays(new Date(), days));
    setEndDate(new Date());
  };

  const calculateMovingAverage = (values: number[], windowSize: number = 5) => {
    const result: number[] = [];
    for (let i = 0; i < values.length; i++) {
      const start = Math.max(0, i - Math.floor(windowSize / 2));
      const end = Math.min(values.length, i + Math.ceil(windowSize / 2));
      const window = values.slice(start, end);
      const avg = window.reduce((a, b) => a + b, 0) / window.length;
      result.push(Number(avg.toFixed(2)));
    }
    return result;
  };

  const formattedChartData = data.map((reading, index) => {
    const phMA = calculateMovingAverage(data.map(r => r.ph))[index];
    const ecMA = calculateMovingAverage(data.map(r => r.ec))[index];
    
    return {
      time: new Date(reading.timestamp).toLocaleString('pt-BR', {
        day: '2-digit',
        month: '2-digit',
        hour: '2-digit',
        minute: '2-digit'
      }),
      pH: reading.ph,
      'pH Média': phMA,
      EC: reading.ec,
      'EC Média': ecMA,
      'Temp. Ar': reading.air_temp,
      'Temp. Água': reading.water_temp,
      'Umidade': reading.humidity,
    };
  });

  if (loading) {
    return (
      <div className="flex items-center justify-center py-12">
        <Loader2 className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  // Check if data is outdated (more than 12 hours old)
  const isDataOutdated = lastDataTimestamp && 
    (new Date().getTime() - new Date(lastDataTimestamp).getTime()) > 12 * 60 * 60 * 1000;

  return (
    <div className="space-y-6">
      <Card className="bg-gradient-to-r from-primary/10 to-blue-500/10 border-primary/20">
        <CardHeader>
          <CardTitle className="text-xl flex items-center justify-between">
            <span className="flex items-center gap-2">
              <BarChart3 className="h-6 w-6" />
              Análise Avançada de Dados
            </span>
            {lastDataTimestamp && (
              <span className="text-xs text-muted-foreground font-normal">
                Última leitura: {new Date(lastDataTimestamp).toLocaleString('pt-BR')}
              </span>
            )}
          </CardTitle>
        </CardHeader>
      </Card>

      {isDataOutdated && (
        <Alert variant="destructive" className="border-orange-500 bg-orange-500/10">
          <AlertTriangle className="h-4 w-4" />
          <AlertTitle>Dados Desatualizados</AlertTitle>
          <AlertDescription>
            A última leitura dos sensores foi há mais de 12 horas. Verifique se o ESP32 está conectado e enviando dados.
          </AlertDescription>
        </Alert>
      )}

      <Tabs defaultValue="charts" className="space-y-4">
        <TabsList className="grid w-full grid-cols-4">
          <TabsTrigger value="charts">Gráficos</TabsTrigger>
          <TabsTrigger value="analysis">Análise</TabsTrigger>
          <TabsTrigger value="insights">IA</TabsTrigger>
          <TabsTrigger value="reports">Relatórios</TabsTrigger>
        </TabsList>

        <TabsContent value="charts" className="space-y-4">
          <ChartFilters
            startDate={startDate}
            endDate={endDate}
            onStartDateChange={setStartDate}
            onEndDateChange={setEndDate}
            onRefresh={fetchHistoricalData}
            onQuickFilter={handleQuickFilter}
          />

          {formattedChartData.length === 0 ? (
            <Card className="border-orange-500/50 bg-orange-500/5">
              <CardContent className="py-12">
                <div className="text-center space-y-4">
                  <AlertTriangle className="h-12 w-12 mx-auto text-orange-500" />
                  <div>
                    <h3 className="font-semibold text-lg mb-2">Nenhum dado disponível</h3>
                    <p className="text-sm text-muted-foreground">
                      Não há leituras dos sensores para o período selecionado ({startDate.toLocaleDateString('pt-BR')} - {endDate.toLocaleDateString('pt-BR')}).
                    </p>
                    <p className="text-sm text-muted-foreground mt-2">
                      Verifique se o ESP32 está conectado e publicando dados no tópico MQTT correto.
                    </p>
                  </div>
                </div>
              </CardContent>
            </Card>
          ) : (
            <div className="space-y-4">
              {/* pH Chart */}
              <Card className="bg-card/50 backdrop-blur-sm">
                <CardHeader>
                  <CardTitle className="text-base">pH ao longo do tempo</CardTitle>
                </CardHeader>
                <CardContent>
                  <ResponsiveContainer width="100%" height={300}>
                    <LineChart data={formattedChartData}>
                      <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" />
                      <XAxis 
                        dataKey="time" 
                        stroke="hsl(var(--muted-foreground))"
                        tick={{ fontSize: 10 }}
                        angle={-45}
                        textAnchor="end"
                        height={80}
                      />
                      <YAxis domain={[0, 14]} stroke="hsl(var(--muted-foreground))" />
                      <Tooltip
                        contentStyle={{
                          backgroundColor: 'hsl(var(--card))',
                          border: '1px solid hsl(var(--border))',
                          borderRadius: '8px'
                        }}
                      />
                      <Legend />
                      <ReferenceLine y={idealRanges.ph.min} stroke="#10b981" strokeDasharray="3 3" label="Mín ideal" />
                      <ReferenceLine y={idealRanges.ph.max} stroke="#ef4444" strokeDasharray="3 3" label="Máx ideal" />
                      <Line type="monotone" dataKey="pH" stroke="#06b6d4" strokeWidth={2} dot={false} />
                      <Line type="monotone" dataKey="pH Média" stroke="#f59e0b" strokeWidth={2} dot={false} />
                    </LineChart>
                  </ResponsiveContainer>
                </CardContent>
              </Card>

              {/* EC Chart */}
              <Card className="bg-card/50 backdrop-blur-sm">
                <CardHeader>
                  <CardTitle className="text-base">Condutividade Elétrica (EC)</CardTitle>
                </CardHeader>
                <CardContent>
                  <ResponsiveContainer width="100%" height={300}>
                    <LineChart data={formattedChartData}>
                      <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" />
                      <XAxis 
                        dataKey="time" 
                        stroke="hsl(var(--muted-foreground))"
                        tick={{ fontSize: 10 }}
                        angle={-45}
                        textAnchor="end"
                        height={80}
                      />
                      <YAxis stroke="hsl(var(--muted-foreground))" />
                      <Tooltip
                        contentStyle={{
                          backgroundColor: 'hsl(var(--card))',
                          border: '1px solid hsl(var(--border))',
                          borderRadius: '8px'
                        }}
                      />
                      <Legend />
                      <ReferenceLine y={idealRanges.ec.min} stroke="#10b981" strokeDasharray="3 3" label="Mín" />
                      <ReferenceLine y={idealRanges.ec.max} stroke="#ef4444" strokeDasharray="3 3" label="Máx" />
                      <Line type="monotone" dataKey="EC" stroke="#06b6d4" strokeWidth={2} dot={false} />
                      <Line type="monotone" dataKey="EC Média" stroke="#f59e0b" strokeWidth={2} dot={false} />
                    </LineChart>
                  </ResponsiveContainer>
                </CardContent>
              </Card>

              {/* Temperature Chart */}
              <Card className="bg-card/50 backdrop-blur-sm">
                <CardHeader>
                  <CardTitle className="text-base">Temperaturas</CardTitle>
                </CardHeader>
                <CardContent>
                  <ResponsiveContainer width="100%" height={300}>
                    <LineChart data={formattedChartData}>
                      <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" />
                      <XAxis 
                        dataKey="time" 
                        stroke="hsl(var(--muted-foreground))"
                        tick={{ fontSize: 10 }}
                        angle={-45}
                        textAnchor="end"
                        height={80}
                      />
                      <YAxis stroke="hsl(var(--muted-foreground))" />
                      <Tooltip
                        contentStyle={{
                          backgroundColor: 'hsl(var(--card))',
                          border: '1px solid hsl(var(--border))',
                          borderRadius: '8px'
                        }}
                      />
                      <Legend />
                      <Line type="monotone" dataKey="Temp. Ar" stroke="#06b6d4" strokeWidth={2} dot={false} />
                      <Line type="monotone" dataKey="Temp. Água" stroke="#14b8a6" strokeWidth={2} dot={false} />
                    </LineChart>
                  </ResponsiveContainer>
                </CardContent>
              </Card>

              {/* Humidity Chart */}
              <Card className="bg-card/50 backdrop-blur-sm">
                <CardHeader>
                  <CardTitle className="text-base">Umidade</CardTitle>
                </CardHeader>
                <CardContent>
                  <ResponsiveContainer width="100%" height={300}>
                    <LineChart data={formattedChartData}>
                      <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" />
                      <XAxis 
                        dataKey="time" 
                        stroke="hsl(var(--muted-foreground))"
                        tick={{ fontSize: 10 }}
                        angle={-45}
                        textAnchor="end"
                        height={80}
                      />
                      <YAxis domain={[0, 100]} stroke="hsl(var(--muted-foreground))" />
                      <Tooltip
                        contentStyle={{
                          backgroundColor: 'hsl(var(--card))',
                          border: '1px solid hsl(var(--border))',
                          borderRadius: '8px'
                        }}
                      />
                      <Legend />
                      <ReferenceLine y={idealRanges.humidity.min} stroke="#10b981" strokeDasharray="3 3" label="Mín" />
                      <ReferenceLine y={idealRanges.humidity.max} stroke="#ef4444" strokeDasharray="3 3" label="Máx" />
                      <Line type="monotone" dataKey="Umidade" stroke="#06b6d4" strokeWidth={2} dot={false} />
                    </LineChart>
                  </ResponsiveContainer>
                </CardContent>
              </Card>
            </div>
          )}
        </TabsContent>

        <TabsContent value="analysis">
          <ChartAnalysis analytics={analytics} />
        </TabsContent>

        <TabsContent value="insights">
          <AIInsightsPanel 
            analytics={analytics}
            period={`${startDate.toLocaleDateString('pt-BR')} - ${endDate.toLocaleDateString('pt-BR')}`}
          />
        </TabsContent>

        <TabsContent value="reports" className="space-y-4">
          <ReportGenerator
            data={data}
            analytics={analytics}
            startDate={startDate}
            endDate={endDate}
          />
        </TabsContent>
      </Tabs>
    </div>
  );
};
