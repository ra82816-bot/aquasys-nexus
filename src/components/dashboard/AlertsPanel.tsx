import { useEffect, useState } from "react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Alert, AlertDescription } from "@/components/ui/alert";
import { AlertCircle, CheckCircle, TrendingUp, TrendingDown } from "lucide-react";
import { supabase } from "@/integrations/supabase/client";

export const AlertsPanel = () => {
  const [alerts, setAlerts] = useState<any[]>([]);

  useEffect(() => {
    const fetchLatestReadings = async () => {
      const { data } = await supabase
        .from("readings")
        .select("*")
        .order("timestamp", { ascending: false })
        .limit(10);

      if (data && data.length > 0) {
        const latest = data[0];
        const newAlerts: any[] = [];

        // Temperature alerts
        if (latest.air_temp > 28) {
          newAlerts.push({
            type: "warning",
            message: `Temperatura acima do ideal: ${latest.air_temp.toFixed(1)}°C`,
            icon: TrendingUp,
          });
        } else if (latest.air_temp < 20) {
          newAlerts.push({
            type: "warning",
            message: `Temperatura abaixo do ideal: ${latest.air_temp.toFixed(1)}°C`,
            icon: TrendingDown,
          });
        }

        // Humidity alerts
        if (latest.humidity > 70) {
          newAlerts.push({
            type: "warning",
            message: `Umidade alta: ${latest.humidity.toFixed(1)}%`,
            icon: AlertCircle,
          });
        } else if (latest.humidity < 50) {
          newAlerts.push({
            type: "warning",
            message: `Umidade baixa: ${latest.humidity.toFixed(1)}%`,
            icon: AlertCircle,
          });
        }

        // pH alerts
        if (latest.ph > 6.5 || latest.ph < 5.5) {
          newAlerts.push({
            type: "danger",
            message: `pH fora do ideal: ${latest.ph.toFixed(2)}`,
            icon: AlertCircle,
          });
        }

        // EC alerts
        if (latest.ec > 2.0 || latest.ec < 1.2) {
          newAlerts.push({
            type: "warning",
            message: `EC fora do ideal: ${latest.ec.toFixed(2)} mS/cm`,
            icon: AlertCircle,
          });
        }

        // Success message if all is good
        if (newAlerts.length === 0) {
          newAlerts.push({
            type: "success",
            message: "Todos os parâmetros estão dentro do ideal",
            icon: CheckCircle,
          });
        }

        setAlerts(newAlerts);
      }
    };

    fetchLatestReadings();

    const channel = supabase
      .channel("alerts-readings")
      .on(
        "postgres_changes",
        { event: "INSERT", schema: "public", table: "readings" },
        () => fetchLatestReadings()
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  }, []);

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <AlertCircle className="h-5 w-5 text-primary" />
          Alertas e Recomendações
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        {alerts.map((alert, index) => (
          <Alert 
            key={index}
            variant={alert.type === "danger" ? "destructive" : "default"}
            className={
              alert.type === "success" 
                ? "border-success text-success" 
                : alert.type === "warning"
                ? "border-warning text-warning"
                : ""
            }
          >
            <alert.icon className="h-4 w-4" />
            <AlertDescription>{alert.message}</AlertDescription>
          </Alert>
        ))}
      </CardContent>
    </Card>
  );
};
