import { useState, useCallback } from 'react';
import { supabase } from '@/integrations/supabase/client';
import { SensorAnalytics } from './useChartAnalytics';

export interface ChartInsight {
  summary: string;
  recommendations: string[];
  predictions: string[];
  concerns: string[];
}

export const useChartInsights = () => {
  const [insights, setInsights] = useState<ChartInsight | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const generateInsights = useCallback(async (analytics: SensorAnalytics, period: string) => {
    setLoading(true);
    setError(null);
    
    try {
      const { data, error: functionError } = await supabase.functions.invoke('chart-insights', {
        body: { analytics, period }
      });

      if (functionError) throw functionError;
      
      setInsights(data.insights);
    } catch (err) {
      console.error('Erro ao gerar insights:', err);
      setError(err instanceof Error ? err.message : 'Erro desconhecido');
    } finally {
      setLoading(false);
    }
  }, []);

  return { insights, loading, error, generateInsights };
};
