import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Alert, AlertDescription } from "@/components/ui/alert";
import { TrendingUp, TrendingDown, Minus, AlertTriangle, Activity } from "lucide-react";
import { SensorAnalytics } from "@/hooks/useChartAnalytics";

interface ChartAnalysisProps {
  analytics: SensorAnalytics | null;
}

export const ChartAnalysis = ({ analytics }: ChartAnalysisProps) => {
  if (!analytics) return null;

  const TrendIcon = ({ trend }: { trend: 'increasing' | 'decreasing' | 'stable' }) => {
    if (trend === 'increasing') return <TrendingUp className="h-4 w-4 text-green-500" />;
    if (trend === 'decreasing') return <TrendingDown className="h-4 w-4 text-red-500" />;
    return <Minus className="h-4 w-4 text-yellow-500" />;
  };

  const StabilityBadge = ({ stability }: { stability: 'stable' | 'moderate' | 'unstable' }) => {
    const variants: Record<string, 'default' | 'secondary' | 'destructive'> = {
      stable: 'default',
      moderate: 'secondary',
      unstable: 'destructive'
    };
    return (
      <Badge variant={variants[stability]}>
        {stability === 'stable' ? 'Estável' : stability === 'moderate' ? 'Moderado' : 'Instável'}
      </Badge>
    );
  };

  return (
    <div className="space-y-4">
      {/* Alerts */}
      {analytics.alerts.length > 0 && (
        <Card className="border-orange-500/50 bg-orange-500/5">
          <CardHeader>
            <CardTitle className="text-base flex items-center gap-2">
              <AlertTriangle className="h-5 w-5 text-orange-500" />
              Alertas ({analytics.alerts.length})
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-2">
            {analytics.alerts.map((alert, idx) => (
              <Alert key={idx} variant={alert.type === 'critical' ? 'destructive' : 'default'}>
                <AlertDescription>
                  <strong>{alert.sensor}:</strong> {alert.message}
                </AlertDescription>
              </Alert>
            ))}
          </CardContent>
        </Card>
      )}

      {/* Statistics Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        {Object.entries(analytics).filter(([key]) => key !== 'alerts').map(([key, value]) => {
          const sensorNames: Record<string, string> = {
            ph: 'pH',
            ec: 'EC',
            air_temp: 'Temp. Ar',
            water_temp: 'Temp. Água',
            humidity: 'Umidade'
          };

          const data = value as any;

          return (
            <Card key={key} className="bg-card/50">
              <CardHeader className="pb-3">
                <CardTitle className="text-sm flex items-center justify-between">
                  <span className="flex items-center gap-2">
                    <Activity className="h-4 w-4" />
                    {sensorNames[key]}
                  </span>
                  <div className="flex items-center gap-2">
                    <TrendIcon trend={data.trend} />
                    <StabilityBadge stability={data.stability} />
                  </div>
                </CardTitle>
              </CardHeader>
              <CardContent className="space-y-2 text-xs">
                <div className="flex justify-between">
                  <span className="text-muted-foreground">Média:</span>
                  <span className="font-semibold">{data.average}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-muted-foreground">Mín / Máx:</span>
                  <span className="font-semibold">{data.min} / {data.max}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-muted-foreground">Desvio Padrão:</span>
                  <span className="font-semibold">{data.stdDev}</span>
                </div>
                {data.anomalies.length > 0 && (
                  <div className="pt-2 border-t">
                    <Badge variant="destructive" className="text-xs">
                      {data.anomalies.length} anomalia(s)
                    </Badge>
                  </div>
                )}
              </CardContent>
            </Card>
          );
        })}
      </div>
    </div>
  );
};
