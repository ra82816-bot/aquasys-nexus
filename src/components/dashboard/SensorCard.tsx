import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Droplets, Thermometer, Wind, Zap } from "lucide-react";

interface SensorCardProps {
  latestReading: any;
}

export const SensorCard = ({ latestReading }: SensorCardProps) => {
  const sensors = [
    {
      title: "pH",
      value: latestReading?.ph,
      icon: <Droplets className="h-5 w-5" />,
      unit: "",
      color: "text-blue-600",
      bgColor: "bg-blue-50"
    },
    {
      title: "EC",
      value: latestReading?.ec,
      icon: <Zap className="h-5 w-5" />,
      unit: "µS/cm",
      color: "text-yellow-600",
      bgColor: "bg-yellow-50"
    },
    {
      title: "Temperatura do Ar",
      value: latestReading?.air_temp,
      icon: <Thermometer className="h-5 w-5" />,
      unit: "°C",
      color: "text-orange-600",
      bgColor: "bg-orange-50"
    },
    {
      title: "Umidade",
      value: latestReading?.humidity,
      icon: <Wind className="h-5 w-5" />,
      unit: "%",
      color: "text-cyan-600",
      bgColor: "bg-cyan-50"
    },
    {
      title: "Temperatura da Água",
      value: latestReading?.water_temp,
      icon: <Thermometer className="h-5 w-5" />,
      unit: "°C",
      color: "text-teal-600",
      bgColor: "bg-teal-50"
    }
  ];

  return (
    <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
      {sensors.map((sensor) => (
        <Card key={sensor.title} className="border-primary/20 hover:border-primary/40 transition-all">
          <CardHeader className="pb-3">
            <CardTitle className="text-sm font-medium flex items-center gap-2">
              <div className={`p-2 rounded-lg ${sensor.bgColor} ${sensor.color}`}>
                {sensor.icon}
              </div>
              {sensor.title}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <div className="text-3xl font-bold">
              {sensor.value !== null && sensor.value !== undefined ? (
                <>
                  {sensor.value.toFixed(1)}
                  <span className="text-lg text-muted-foreground ml-1">{sensor.unit}</span>
                </>
              ) : (
                <span className="text-muted-foreground text-lg">Aguardando dados...</span>
              )}
            </div>
          </CardContent>
        </Card>
      ))}
    </div>
  );
};
