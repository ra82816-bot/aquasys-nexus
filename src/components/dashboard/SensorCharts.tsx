import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from "recharts";
import { Loader2, Calendar, RefreshCw } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Popover, PopoverContent, PopoverTrigger } from "@/components/ui/popover";
import { Calendar as CalendarComponent } from "@/components/ui/calendar";
import { format, subDays } from "date-fns";
import { ptBR } from "date-fns/locale";
import { cn } from "@/lib/utils";

export const SensorCharts = () => {
  const [data, setData] = useState<any[]>([]);
  const [loading, setLoading] = useState(true);
  const [startDate, setStartDate] = useState<Date>(subDays(new Date(), 7));
  const [endDate, setEndDate] = useState<Date>(new Date());

  useEffect(() => {
    fetchHistoricalData();

    // Subscribe to realtime updates
    const channel = supabase
      .channel("readings-realtime")
      .on(
        "postgres_changes",
        { event: "INSERT", schema: "public", table: "readings" },
        () => {
          console.log("Nova leitura recebida, atualizando gráficos...");
          fetchHistoricalData();
        }
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  }, [startDate, endDate]);

  const calculateMovingAverage = (data: number[], windowSize: number = 5) => {
    const result: number[] = [];
    for (let i = 0; i < data.length; i++) {
      const start = Math.max(0, i - Math.floor(windowSize / 2));
      const end = Math.min(data.length, i + Math.ceil(windowSize / 2));
      const window = data.slice(start, end);
      const avg = window.reduce((a, b) => a + b, 0) / window.length;
      result.push(Number(avg.toFixed(2)));
    }
    return result;
  };

  const fetchHistoricalData = async () => {
    setLoading(true);
    try {
      const startTimestamp = startDate.toISOString();
      const endTimestamp = endDate.toISOString();

      console.log('Buscando dados de', startTimestamp, 'até', endTimestamp);

      const { data: readings, error } = await supabase
        .from("readings")
        .select("*")
        .gte("timestamp", startTimestamp)
        .lte("timestamp", endTimestamp)
        .order("timestamp", { ascending: true });

      if (error) throw error;

      if (!readings || readings.length === 0) {
        setData([]);
        return;
      }

      const phValues = readings.map(r => r.ph);
      const ecValues = readings.map(r => r.ec);
      const airTempValues = readings.map(r => r.air_temp);
      const waterTempValues = readings.map(r => r.water_temp);
      const humidityValues = readings.map(r => r.humidity);

      const phMA = calculateMovingAverage(phValues);
      const ecMA = calculateMovingAverage(ecValues);
      const airTempMA = calculateMovingAverage(airTempValues);
      const waterTempMA = calculateMovingAverage(waterTempValues);
      const humidityMA = calculateMovingAverage(humidityValues);

      const formattedData = readings.map((reading, index) => ({
        time: new Date(reading.timestamp).toLocaleString('pt-BR', {
          day: '2-digit',
          month: '2-digit',
          hour: '2-digit',
          minute: '2-digit'
        }),
        pH: reading.ph,
        'pH Média': phMA[index],
        EC: reading.ec,
        'EC Média': ecMA[index],
        'Temp. Ar': reading.air_temp,
        'Temp. Ar Média': airTempMA[index],
        'Temp. Água': reading.water_temp,
        'Temp. Água Média': waterTempMA[index],
        'Umidade': reading.humidity,
        'Umidade Média': humidityMA[index]
      }));

      console.log(`${formattedData.length} leituras carregadas`);
      setData(formattedData);
    } catch (error) {
      console.error("Erro ao buscar dados históricos:", error);
    } finally {
      setLoading(false);
    }
  };

  const handleRefresh = () => {
    fetchHistoricalData();
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center py-12">
        <Loader2 className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  return (
    <div className="space-y-6">
      <Card className="bg-card/50 backdrop-blur-sm">
        <CardHeader>
          <CardTitle className="text-base sm:text-xl">Filtrar Período</CardTitle>
          <div className="flex flex-col sm:flex-row gap-3 mt-4">
            <div className="flex flex-col sm:flex-row items-start sm:items-center gap-2 flex-1">
              <Popover>
                <PopoverTrigger asChild>
                  <Button variant="outline" className={cn("justify-start text-left font-normal w-full sm:w-auto")}>
                    <Calendar className="mr-2 h-4 w-4" />
                    <span className="hidden sm:inline">De: </span>
                    {format(startDate, "dd/MM/yy", { locale: ptBR })}
                  </Button>
                </PopoverTrigger>
                <PopoverContent className="w-auto p-0" align="start">
                  <CalendarComponent
                    mode="single"
                    selected={startDate}
                    onSelect={(date) => date && setStartDate(date)}
                    disabled={(date) => date > endDate}
                    initialFocus
                  />
                </PopoverContent>
              </Popover>
              
              <span className="text-sm text-muted-foreground self-center">até</span>
              
              <Popover>
                <PopoverTrigger asChild>
                  <Button variant="outline" className={cn("justify-start text-left font-normal w-full sm:w-auto")}>
                    <Calendar className="mr-2 h-4 w-4" />
                    <span className="hidden sm:inline">Até: </span>
                    {format(endDate, "dd/MM/yy", { locale: ptBR })}
                  </Button>
                </PopoverTrigger>
                <PopoverContent className="w-auto p-0" align="start">
                  <CalendarComponent
                    mode="single"
                    selected={endDate}
                    onSelect={(date) => date && setEndDate(date)}
                    disabled={(date) => date < startDate || date > new Date()}
                    initialFocus
                  />
                </PopoverContent>
              </Popover>
            </div>
            
            <Button 
              onClick={handleRefresh}
              variant="default"
              size="sm"
              className="gap-2 w-full sm:w-auto"
            >
              <RefreshCw className="h-4 w-4" />
              Atualizar
            </Button>
          </div>
        </CardHeader>
      </Card>

      {data.length === 0 ? (
        <div className="text-center py-12 text-muted-foreground">
          Nenhum dado disponível para o período selecionado
        </div>
      ) : (
        <div className="space-y-4 sm:space-y-6">
          <Card className="bg-card/50 backdrop-blur-sm">
            <CardHeader>
              <CardTitle className="text-base sm:text-xl">pH ao longo do tempo</CardTitle>
            </CardHeader>
            <CardContent className="p-2 sm:p-6">
              <ResponsiveContainer width="100%" height={250}>
                <LineChart data={data}>
                  <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" />
                  <XAxis 
                    dataKey="time" 
                    stroke="hsl(var(--muted-foreground))"
                    tick={{ fill: 'hsl(var(--muted-foreground))', fontSize: 10 }}
                    angle={-45}
                    textAnchor="end"
                    height={60}
                  />
                  <YAxis 
                    domain={[0, 14]} 
                    stroke="hsl(var(--muted-foreground))"
                    tick={{ fill: 'hsl(var(--muted-foreground))' }}
                  />
                  <Tooltip 
                    contentStyle={{ 
                      backgroundColor: 'hsl(var(--card))',
                      border: '1px solid hsl(var(--border))',
                      borderRadius: '8px'
                    }}
                  />
                  <Legend />
                  <Line 
                    type="monotone" 
                    dataKey="pH" 
                    stroke="#06b6d4" 
                    strokeWidth={2}
                    dot={false}
                    name="Leitura Instantânea"
                  />
                  <Line 
                    type="monotone" 
                    dataKey="pH Média" 
                    stroke="#f59e0b" 
                    strokeWidth={2}
                    dot={false}
                    name="Média Móvel"
                  />
                </LineChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>

          <Card className="bg-card/50 backdrop-blur-sm">
            <CardHeader>
              <CardTitle className="text-base sm:text-xl">Condutividade Elétrica (EC)</CardTitle>
            </CardHeader>
            <CardContent className="p-2 sm:p-6">
              <ResponsiveContainer width="100%" height={250}>
                <LineChart data={data}>
                  <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" />
                  <XAxis 
                    dataKey="time" 
                    stroke="hsl(var(--muted-foreground))"
                    tick={{ fill: 'hsl(var(--muted-foreground))', fontSize: 10 }}
                    angle={-45}
                    textAnchor="end"
                    height={60}
                  />
                  <YAxis 
                    stroke="hsl(var(--muted-foreground))"
                    tick={{ fill: 'hsl(var(--muted-foreground))', fontSize: 11 }}
                  />
                  <Tooltip 
                    contentStyle={{ 
                      backgroundColor: 'hsl(var(--card))',
                      border: '1px solid hsl(var(--border))',
                      borderRadius: '8px'
                    }}
                  />
                  <Legend />
                  <Line 
                    type="monotone" 
                    dataKey="EC" 
                    stroke="#06b6d4" 
                    strokeWidth={2}
                    dot={false}
                    name="Leitura Instantânea"
                  />
                  <Line 
                    type="monotone" 
                    dataKey="EC Média" 
                    stroke="#f59e0b" 
                    strokeWidth={2}
                    dot={false}
                    name="Média Móvel"
                  />
                </LineChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>

          <Card className="bg-card/50 backdrop-blur-sm">
            <CardHeader>
              <CardTitle className="text-base sm:text-xl">Temperaturas</CardTitle>
            </CardHeader>
            <CardContent className="p-2 sm:p-6">
              <ResponsiveContainer width="100%" height={250}>
                <LineChart data={data}>
                  <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" />
                  <XAxis 
                    dataKey="time" 
                    stroke="hsl(var(--muted-foreground))"
                    tick={{ fill: 'hsl(var(--muted-foreground))', fontSize: 10 }}
                    angle={-45}
                    textAnchor="end"
                    height={60}
                  />
                  <YAxis 
                    stroke="hsl(var(--muted-foreground))"
                    tick={{ fill: 'hsl(var(--muted-foreground))', fontSize: 11 }}
                  />
                  <Tooltip 
                    contentStyle={{ 
                      backgroundColor: 'hsl(var(--card))',
                      border: '1px solid hsl(var(--border))',
                      borderRadius: '8px'
                    }}
                  />
                  <Legend />
                  <Line 
                    type="monotone" 
                    dataKey="Temp. Ar" 
                    stroke="#06b6d4" 
                    strokeWidth={2}
                    dot={false}
                    name="Temp. Ar"
                  />
                  <Line 
                    type="monotone" 
                    dataKey="Temp. Ar Média" 
                    stroke="#f59e0b" 
                    strokeWidth={2}
                    dot={false}
                    name="Temp. Ar Média"
                  />
                  <Line 
                    type="monotone" 
                    dataKey="Temp. Água" 
                    stroke="#14b8a6" 
                    strokeWidth={2}
                    dot={false}
                    name="Temp. Água"
                  />
                  <Line 
                    type="monotone" 
                    dataKey="Temp. Água Média" 
                    stroke="#fb923c" 
                    strokeWidth={2}
                    dot={false}
                    name="Temp. Água Média"
                  />
                </LineChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>

          <Card className="bg-card/50 backdrop-blur-sm">
            <CardHeader>
              <CardTitle className="text-base sm:text-xl">Umidade</CardTitle>
            </CardHeader>
            <CardContent className="p-2 sm:p-6">
              <ResponsiveContainer width="100%" height={250}>
                <LineChart data={data}>
                  <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" />
                  <XAxis 
                    dataKey="time" 
                    stroke="hsl(var(--muted-foreground))"
                    tick={{ fill: 'hsl(var(--muted-foreground))', fontSize: 10 }}
                    angle={-45}
                    textAnchor="end"
                    height={60}
                  />
                  <YAxis 
                    domain={[0, 100]} 
                    stroke="hsl(var(--muted-foreground))"
                    tick={{ fill: 'hsl(var(--muted-foreground))', fontSize: 11 }}
                  />
                  <Tooltip 
                    contentStyle={{ 
                      backgroundColor: 'hsl(var(--card))',
                      border: '1px solid hsl(var(--border))',
                      borderRadius: '8px'
                    }}
                  />
                  <Legend />
                  <Line 
                    type="monotone" 
                    dataKey="Umidade" 
                    stroke="#06b6d4" 
                    strokeWidth={2}
                    dot={false}
                    name="Leitura Instantânea"
                  />
                  <Line 
                    type="monotone" 
                    dataKey="Umidade Média" 
                    stroke="#f59e0b" 
                    strokeWidth={2}
                    dot={false}
                    name="Média Móvel"
                  />
                </LineChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>
        </div>
      )}
    </div>
  );
};
