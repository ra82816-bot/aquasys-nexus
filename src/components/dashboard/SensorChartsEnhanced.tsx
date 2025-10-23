import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, ReferenceLine } from "recharts";
import { Loader2, BarChart3 } from "lucide-react";
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
        return;
      }

      console.log(`✅ ${readings.length} leituras carregadas`);
      setData(readings as SensorReading[]);
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

  return (
    <div className="space-y-6">
      <Card className="bg-gradient-to-r from-primary/10 to-blue-500/10 border-primary/20">
        <CardHeader>
          <CardTitle className="text-xl flex items-center gap-2">
            <BarChart3 className="h-6 w-6" />
            Análise Avançada de Dados
          </CardTitle>
        </CardHeader>
      </Card>

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
            <div className="text-center py-12 text-muted-foreground">
              Nenhum dado disponível para o período selecionado
            </div>
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
