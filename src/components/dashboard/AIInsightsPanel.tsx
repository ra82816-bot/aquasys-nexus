import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Sparkles, Loader2, AlertCircle, Lightbulb, TrendingUp } from "lucide-react";
import { useChartInsights, ChartInsight } from "@/hooks/useChartInsights";
import { SensorAnalytics } from "@/hooks/useChartAnalytics";

interface AIInsightsPanelProps {
  analytics: SensorAnalytics | null;
  period: string;
}

export const AIInsightsPanel = ({ analytics, period }: AIInsightsPanelProps) => {
  const { insights, loading, error, generateInsights } = useChartInsights();

  const handleGenerate = () => {
    if (analytics) {
      generateInsights(analytics, period);
    }
  };

  return (
    <Card className="bg-gradient-to-br from-purple-500/10 to-blue-500/10 border-purple-500/20">
      <CardHeader>
        <CardTitle className="text-base flex items-center gap-2">
          <Sparkles className="h-5 w-5 text-purple-500" />
          Insights de IA
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        {!insights && !loading && (
          <div className="text-center py-6 space-y-4">
            <p className="text-sm text-muted-foreground">
              Gere insights inteligentes sobre seus dados de cultivo usando IA
            </p>
            <Button
              onClick={handleGenerate}
              disabled={!analytics}
              className="gap-2"
            >
              <Sparkles className="h-4 w-4" />
              Gerar Insights
            </Button>
          </div>
        )}

        {loading && (
          <div className="flex flex-col items-center justify-center py-8 space-y-3">
            <Loader2 className="h-8 w-8 animate-spin text-purple-500" />
            <p className="text-sm text-muted-foreground">Analisando dados...</p>
          </div>
        )}

        {error && (
          <div className="flex items-center gap-2 text-destructive text-sm">
            <AlertCircle className="h-4 w-4" />
            <span>{error}</span>
          </div>
        )}

        {insights && (
          <div className="space-y-4">
            {/* Summary */}
            <div className="p-4 bg-card rounded-lg border">
              <h4 className="font-semibold text-sm mb-2 flex items-center gap-2">
                <Sparkles className="h-4 w-4 text-purple-500" />
                Resumo
              </h4>
              <p className="text-sm text-muted-foreground leading-relaxed">
                {insights.summary}
              </p>
            </div>

            {/* Recommendations */}
            {insights.recommendations.length > 0 && (
              <div className="p-4 bg-card rounded-lg border">
                <h4 className="font-semibold text-sm mb-3 flex items-center gap-2">
                  <Lightbulb className="h-4 w-4 text-amber-500" />
                  Recomendações
                </h4>
                <ul className="space-y-2">
                  {insights.recommendations.map((rec, idx) => (
                    <li key={idx} className="text-sm flex items-start gap-2">
                      <Badge variant="secondary" className="mt-0.5">
                        {idx + 1}
                      </Badge>
                      <span className="text-muted-foreground">{rec}</span>
                    </li>
                  ))}
                </ul>
              </div>
            )}

            {/* Predictions */}
            {insights.predictions.length > 0 && (
              <div className="p-4 bg-card rounded-lg border">
                <h4 className="font-semibold text-sm mb-3 flex items-center gap-2">
                  <TrendingUp className="h-4 w-4 text-green-500" />
                  Previsões
                </h4>
                <ul className="space-y-2">
                  {insights.predictions.map((pred, idx) => (
                    <li key={idx} className="text-sm flex items-start gap-2">
                      <span className="text-green-500">▸</span>
                      <span className="text-muted-foreground">{pred}</span>
                    </li>
                  ))}
                </ul>
              </div>
            )}

            {/* Concerns */}
            {insights.concerns.length > 0 && (
              <div className="p-4 bg-destructive/10 rounded-lg border border-destructive/20">
                <h4 className="font-semibold text-sm mb-3 flex items-center gap-2 text-destructive">
                  <AlertCircle className="h-4 w-4" />
                  Pontos de Atenção
                </h4>
                <ul className="space-y-2">
                  {insights.concerns.map((concern, idx) => (
                    <li key={idx} className="text-sm flex items-start gap-2">
                      <span className="text-destructive">⚠</span>
                      <span className="text-muted-foreground">{concern}</span>
                    </li>
                  ))}
                </ul>
              </div>
            )}

            <Button
              onClick={handleGenerate}
              variant="outline"
              size="sm"
              className="w-full gap-2"
            >
              <Sparkles className="h-4 w-4" />
              Gerar Novos Insights
            </Button>
          </div>
        )}
      </CardContent>
    </Card>
  );
};
