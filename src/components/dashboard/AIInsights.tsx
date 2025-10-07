import { useEffect, useState } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { supabase } from '@/integrations/supabase/client';
import { useToast } from '@/hooks/use-toast';
import { Brain, TrendingUp, AlertTriangle, Lightbulb, RefreshCw, Loader2 } from 'lucide-react';

interface AIInsight {
  id: string;
  created_at: string;
  insight_type: string;
  title: string;
  description: string;
  severity: 'info' | 'warning' | 'critical';
  recommendations: string[];
}

export function AIInsights() {
  const [insights, setInsights] = useState<AIInsight[]>([]);
  const [loading, setLoading] = useState(true);
  const [analyzing, setAnalyzing] = useState(false);
  const { toast } = useToast();

  const fetchInsights = async () => {
    try {
      const { data, error } = await supabase
        .from('ai_insights')
        .select('*')
        .eq('is_active', true)
        .order('created_at', { ascending: false });

      if (error) throw error;
      setInsights((data || []) as AIInsight[]);
    } catch (error) {
      console.error('Erro ao buscar insights:', error);
      toast({
        title: 'Erro ao carregar insights',
        description: 'Não foi possível carregar as análises da IA',
        variant: 'destructive',
      });
    } finally {
      setLoading(false);
    }
  };

  const runAnalysis = async () => {
    setAnalyzing(true);
    try {
      const { error } = await supabase.functions.invoke('ai-analytics');

      if (error) throw error;

      toast({
        title: 'Análise concluída',
        description: 'Novos insights foram gerados pela IA',
      });

      await fetchInsights();
    } catch (error) {
      console.error('Erro ao executar análise:', error);
      toast({
        title: 'Erro na análise',
        description: 'Não foi possível completar a análise. Tente novamente.',
        variant: 'destructive',
      });
    } finally {
      setAnalyzing(false);
    }
  };

  useEffect(() => {
    fetchInsights();

    // Inscrever em mudanças em tempo real
    const channel = supabase
      .channel('ai-insights-changes')
      .on(
        'postgres_changes',
        {
          event: '*',
          schema: 'public',
          table: 'ai_insights'
        },
        () => {
          fetchInsights();
        }
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  }, []);

  const getInsightIcon = (type: string) => {
    switch (type) {
      case 'anomaly':
        return <AlertTriangle className="h-5 w-5" />;
      case 'trend':
        return <TrendingUp className="h-5 w-5" />;
      case 'recommendation':
        return <Lightbulb className="h-5 w-5" />;
      default:
        return <Brain className="h-5 w-5" />;
    }
  };

  const getSeverityVariant = (severity: string): "default" | "secondary" | "destructive" => {
    switch (severity) {
      case 'critical':
        return 'destructive';
      case 'warning':
        return 'secondary';
      default:
        return 'default';
    }
  };

  const getSeverityColor = (severity: string) => {
    switch (severity) {
      case 'critical':
        return 'text-destructive';
      case 'warning':
        return 'text-yellow-600 dark:text-yellow-500';
      default:
        return 'text-primary';
    }
  };

  if (loading) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Brain className="h-5 w-5" />
            Análise por IA
          </CardTitle>
        </CardHeader>
        <CardContent>
          <div className="flex items-center justify-center py-8">
            <Loader2 className="h-8 w-8 animate-spin text-muted-foreground" />
          </div>
        </CardContent>
      </Card>
    );
  }

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center justify-between">
          <div>
            <CardTitle className="flex items-center gap-2">
              <Brain className="h-5 w-5" />
              Análise por IA
            </CardTitle>
            <CardDescription>
              Insights automáticos sobre a saúde do seu cultivo
            </CardDescription>
          </div>
          <Button 
            onClick={runAnalysis} 
            disabled={analyzing}
            size="sm"
          >
            {analyzing ? (
              <>
                <Loader2 className="h-4 w-4 mr-2 animate-spin" />
                Analisando...
              </>
            ) : (
              <>
                <RefreshCw className="h-4 w-4 mr-2" />
                Atualizar Análise
              </>
            )}
          </Button>
        </div>
      </CardHeader>
      <CardContent className="space-y-4">
        {insights.length === 0 ? (
          <div className="text-center py-8 text-muted-foreground">
            <Brain className="h-12 w-12 mx-auto mb-3 opacity-50" />
            <p>Nenhuma análise disponível ainda.</p>
            <p className="text-sm mt-2">Clique em "Atualizar Análise" para gerar insights.</p>
          </div>
        ) : (
          insights.map((insight) => (
            <Card key={insight.id} className="border-l-4" style={{
              borderLeftColor: insight.severity === 'critical' ? 'hsl(var(--destructive))' :
                              insight.severity === 'warning' ? 'hsl(var(--warning))' :
                              'hsl(var(--primary))'
            }}>
              <CardHeader className="pb-3">
                <div className="flex items-start justify-between gap-4">
                  <div className="flex items-start gap-3 flex-1">
                    <div className={getSeverityColor(insight.severity)}>
                      {getInsightIcon(insight.insight_type)}
                    </div>
                    <div className="flex-1">
                      <CardTitle className="text-base mb-1">{insight.title}</CardTitle>
                      <CardDescription className="text-sm">
                        {insight.description}
                      </CardDescription>
                    </div>
                  </div>
                  <Badge variant={getSeverityVariant(insight.severity)}>
                    {insight.severity === 'critical' ? 'Crítico' :
                     insight.severity === 'warning' ? 'Atenção' : 'Info'}
                  </Badge>
                </div>
              </CardHeader>
              {insight.recommendations && insight.recommendations.length > 0 && (
                <CardContent className="pt-0">
                  <div className="space-y-2">
                    <p className="text-sm font-medium text-muted-foreground">Recomendações:</p>
                    <ul className="space-y-1">
                      {insight.recommendations.map((rec, idx) => (
                        <li key={idx} className="text-sm flex items-start gap-2">
                          <span className="text-primary mt-1">•</span>
                          <span>{rec}</span>
                        </li>
                      ))}
                    </ul>
                  </div>
                </CardContent>
              )}
            </Card>
          ))
        )}
      </CardContent>
    </Card>
  );
}
