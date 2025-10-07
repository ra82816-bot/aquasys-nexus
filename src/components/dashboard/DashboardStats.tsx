import { Card, CardContent } from "@/components/ui/card";
import { Thermometer, Droplets, Leaf, AlertTriangle } from "lucide-react";

interface DashboardStatsProps {
  latestReading: any;
}

export const DashboardStats = ({ latestReading }: DashboardStatsProps) => {
  const getStatusColor = (value: number, min: number, max: number) => {
    if (value < min || value > max) return "text-destructive";
    if (value <= min + 2 || value >= max - 2) return "text-warning";
    return "text-success";
  };

  const stats = [
    {
      label: "Temperatura",
      value: latestReading?.air_temp?.toFixed(1) || "--",
      unit: "°C",
      icon: Thermometer,
      color: getStatusColor(latestReading?.air_temp || 0, 20, 28),
      ideal: "20-28°C"
    },
    {
      label: "Umidade",
      value: latestReading?.humidity?.toFixed(1) || "--",
      unit: "%",
      icon: Droplets,
      color: getStatusColor(latestReading?.humidity || 0, 50, 70),
      ideal: "50-70%"
    },
    {
      label: "pH",
      value: latestReading?.ph?.toFixed(2) || "--",
      unit: "",
      icon: Leaf,
      color: getStatusColor(latestReading?.ph || 0, 5.5, 6.5),
      ideal: "5.5-6.5"
    },
    {
      label: "EC",
      value: latestReading?.ec?.toFixed(2) || "--",
      unit: "mS/cm",
      icon: AlertTriangle,
      color: getStatusColor(latestReading?.ec || 0, 1.2, 2.0),
      ideal: "1.2-2.0"
    }
  ];

  return (
    <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
      {stats.map((stat) => (
        <Card key={stat.label} className="overflow-hidden">
          <CardContent className="p-4">
            <div className="flex items-start justify-between mb-2">
              <div className="p-2 rounded-lg bg-primary/10">
                <stat.icon className="h-5 w-5 text-primary" />
              </div>
              <div className="text-right">
                <div className={`text-2xl font-bold ${stat.color}`}>
                  {stat.value}
                  <span className="text-sm ml-1">{stat.unit}</span>
                </div>
              </div>
            </div>
            <div className="space-y-1">
              <p className="text-sm font-medium text-foreground">{stat.label}</p>
              <p className="text-xs text-muted-foreground">Ideal: {stat.ideal}</p>
            </div>
          </CardContent>
        </Card>
      ))}
    </div>
  );
};
