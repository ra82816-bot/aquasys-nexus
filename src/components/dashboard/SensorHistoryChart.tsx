import { useState, useEffect } from "react";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { Button } from "@/components/ui/button";
import { Download } from "lucide-react";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from "recharts";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";
import jsPDF from "jspdf";
import html2canvas from "html2canvas";

interface SensorHistoryChartProps {
  sensorKey: string;
  title: string;
  unit: string;
  color: string;
}

type Period = "hour" | "day" | "week" | "month";

export const SensorHistoryChart = ({ sensorKey, title, unit, color }: SensorHistoryChartProps) => {
  const [period, setPeriod] = useState<Period>("day");
  const [data, setData] = useState<any[]>([]);
  const [loading, setLoading] = useState(true);
  const { toast } = useToast();

  useEffect(() => {
    fetchData();
    
    // Realtime subscription
    const channel = supabase
      .channel(`${sensorKey}-chart`)
      .on(
        "postgres_changes",
        { event: "INSERT", schema: "public", table: "readings" },
        () => fetchData()
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  }, [period, sensorKey]);

  const fetchData = async () => {
    setLoading(true);
    try {
      const now = new Date();
      let startDate = new Date();

      switch (period) {
        case "hour":
          startDate.setHours(now.getHours() - 1);
          break;
        case "day":
          startDate.setDate(now.getDate() - 1);
          break;
        case "week":
          startDate.setDate(now.getDate() - 7);
          break;
        case "month":
          startDate.setMonth(now.getMonth() - 1);
          break;
      }

      const { data: readings, error } = await supabase
        .from("readings")
        .select("timestamp, " + sensorKey)
        .gte("timestamp", startDate.toISOString())
        .order("timestamp", { ascending: true });

      if (error) throw error;

      // Calculate moving average
      const processedData = readings?.map((reading, index) => {
        const windowSize = Math.min(5, index + 1);
        const window = readings.slice(Math.max(0, index - windowSize + 1), index + 1);
        const movingAvg = window.reduce((sum, r) => sum + (r[sensorKey] || 0), 0) / windowSize;

        return {
          time: new Date(reading.timestamp).toLocaleString("pt-BR", {
            hour: "2-digit",
            minute: "2-digit",
            day: "2-digit",
            month: "2-digit",
          }),
          value: reading[sensorKey],
          movingAvg: parseFloat(movingAvg.toFixed(2)),
        };
      }) || [];

      setData(processedData);
    } catch (error) {
      console.error("Error fetching data:", error);
      toast({
        title: "Erro ao carregar dados",
        description: "Não foi possível carregar o histórico",
        variant: "destructive",
      });
    } finally {
      setLoading(false);
    }
  };

  const exportToPDF = async () => {
    try {
      const chartElement = document.getElementById(`chart-${sensorKey}`);
      if (!chartElement) return;

      const canvas = await html2canvas(chartElement, {
        scale: 2,
        backgroundColor: "#ffffff",
      });

      const imgData = canvas.toDataURL("image/png");
      const pdf = new jsPDF({
        orientation: "landscape",
        unit: "mm",
        format: "a4",
      });

      const imgWidth = 280;
      const imgHeight = (canvas.height * imgWidth) / canvas.width;

      pdf.text(`${title} - Histórico`, 15, 15);
      pdf.text(`Período: ${getPeriodLabel(period)}`, 15, 22);
      pdf.text(`Data: ${new Date().toLocaleString("pt-BR")}`, 15, 29);
      pdf.addImage(imgData, "PNG", 10, 35, imgWidth, imgHeight);

      pdf.save(`${sensorKey}-${period}-${Date.now()}.pdf`);

      toast({
        title: "PDF exportado com sucesso",
        description: `Gráfico de ${title} salvo`,
      });
    } catch (error) {
      console.error("Error exporting PDF:", error);
      toast({
        title: "Erro ao exportar PDF",
        description: "Não foi possível gerar o arquivo",
        variant: "destructive",
      });
    }
  };

  const getPeriodLabel = (p: Period) => {
    const labels = {
      hour: "Última Hora",
      day: "Último Dia",
      week: "Última Semana",
      month: "Último Mês",
    };
    return labels[p];
  };

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center justify-between">
          <div>
            <CardTitle>{title}</CardTitle>
            <CardDescription>Histórico com média móvel</CardDescription>
          </div>
          <div className="flex gap-2">
            <Select value={period} onValueChange={(v) => setPeriod(v as Period)}>
              <SelectTrigger className="w-[160px]">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="hour">Última Hora</SelectItem>
                <SelectItem value="day">Último Dia</SelectItem>
                <SelectItem value="week">Última Semana</SelectItem>
                <SelectItem value="month">Último Mês</SelectItem>
              </SelectContent>
            </Select>
            <Button onClick={exportToPDF} variant="outline" size="icon">
              <Download className="h-4 w-4" />
            </Button>
          </div>
        </div>
      </CardHeader>
      <CardContent>
        <div id={`chart-${sensorKey}`} className="bg-white p-4 rounded-lg">
          {loading ? (
            <div className="h-[300px] flex items-center justify-center text-muted-foreground">
              Carregando dados...
            </div>
          ) : data.length === 0 ? (
            <div className="h-[300px] flex items-center justify-center text-muted-foreground">
              Sem dados para este período
            </div>
          ) : (
            <ResponsiveContainer width="100%" height={300}>
              <LineChart data={data}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="time" tick={{ fontSize: 12 }} />
                <YAxis tick={{ fontSize: 12 }} />
                <Tooltip />
                <Legend />
                <Line
                  type="monotone"
                  dataKey="value"
                  stroke={color}
                  strokeWidth={2}
                  name={`${title} (${unit})`}
                  dot={{ r: 3 }}
                />
                <Line
                  type="monotone"
                  dataKey="movingAvg"
                  stroke={`${color}80`}
                  strokeWidth={2}
                  strokeDasharray="5 5"
                  name="Média Móvel"
                  dot={false}
                />
              </LineChart>
            </ResponsiveContainer>
          )}
        </div>
      </CardContent>
    </Card>
  );
};
