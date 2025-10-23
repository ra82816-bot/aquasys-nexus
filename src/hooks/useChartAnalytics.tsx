import { useMemo } from 'react';

export interface SensorReading {
  timestamp: string;
  ph: number;
  ec: number;
  air_temp: number;
  water_temp: number;
  humidity: number;
}

export interface AnalyticsResult {
  average: number;
  min: number;
  max: number;
  stdDev: number;
  anomalies: Array<{ timestamp: string; value: number; deviation: number }>;
  trend: 'increasing' | 'decreasing' | 'stable';
  stability: 'stable' | 'moderate' | 'unstable';
}

export interface SensorAnalytics {
  ph: AnalyticsResult;
  ec: AnalyticsResult;
  air_temp: AnalyticsResult;
  water_temp: AnalyticsResult;
  humidity: AnalyticsResult;
  alerts: Array<{
    type: 'warning' | 'critical';
    sensor: string;
    message: string;
    timestamp: string;
  }>;
}

export const useChartAnalytics = (data: SensorReading[], idealRanges?: {
  ph?: { min: number; max: number };
  ec?: { min: number; max: number };
  temp?: { min: number; max: number };
  humidity?: { min: number; max: number };
}) => {
  return useMemo(() => {
    if (!data || data.length === 0) return null;

    const analyzeSensor = (
      values: number[],
      sensorName: string,
      idealMin?: number,
      idealMax?: number
    ): AnalyticsResult => {
      const average = values.reduce((a, b) => a + b, 0) / values.length;
      const min = Math.min(...values);
      const max = Math.max(...values);
      
      // Standard deviation
      const squaredDiffs = values.map(v => Math.pow(v - average, 2));
      const variance = squaredDiffs.reduce((a, b) => a + b, 0) / values.length;
      const stdDev = Math.sqrt(variance);

      // Detect anomalies (values > 2 std deviations from mean)
      const anomalies = data
        .map((reading, i) => ({
          timestamp: reading.timestamp,
          value: values[i],
          deviation: Math.abs(values[i] - average) / stdDev,
        }))
        .filter(a => a.deviation > 2);

      // Calculate trend using linear regression
      const n = values.length;
      const xValues = Array.from({ length: n }, (_, i) => i);
      const sumX = xValues.reduce((a, b) => a + b, 0);
      const sumY = values.reduce((a, b) => a + b, 0);
      const sumXY = xValues.reduce((sum, x, i) => sum + x * values[i], 0);
      const sumX2 = xValues.reduce((sum, x) => sum + x * x, 0);
      const slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
      
      const trend = Math.abs(slope) < 0.001 ? 'stable' : slope > 0 ? 'increasing' : 'decreasing';
      
      // Stability based on coefficient of variation
      const coefficientOfVariation = (stdDev / average) * 100;
      const stability = coefficientOfVariation < 5 ? 'stable' : coefficientOfVariation < 10 ? 'moderate' : 'unstable';

      return {
        average: Number(average.toFixed(2)),
        min: Number(min.toFixed(2)),
        max: Number(max.toFixed(2)),
        stdDev: Number(stdDev.toFixed(2)),
        anomalies,
        trend,
        stability,
      };
    };

    const phValues = data.map(r => r.ph);
    const ecValues = data.map(r => r.ec);
    const airTempValues = data.map(r => r.air_temp);
    const waterTempValues = data.map(r => r.water_temp);
    const humidityValues = data.map(r => r.humidity);

    const analytics: SensorAnalytics = {
      ph: analyzeSensor(phValues, 'pH', idealRanges?.ph?.min, idealRanges?.ph?.max),
      ec: analyzeSensor(ecValues, 'EC', idealRanges?.ec?.min, idealRanges?.ec?.max),
      air_temp: analyzeSensor(airTempValues, 'Temperatura do Ar', idealRanges?.temp?.min, idealRanges?.temp?.max),
      water_temp: analyzeSensor(waterTempValues, 'Temperatura da Água', idealRanges?.temp?.min, idealRanges?.temp?.max),
      humidity: analyzeSensor(humidityValues, 'Umidade', idealRanges?.humidity?.min, idealRanges?.humidity?.max),
      alerts: [],
    };

    // Generate alerts based on ideal ranges
    if (idealRanges?.ph) {
      const outOfRange = data.filter(r => r.ph < idealRanges.ph!.min || r.ph > idealRanges.ph!.max);
      if (outOfRange.length > 0) {
        analytics.alerts.push({
          type: outOfRange.length > data.length * 0.3 ? 'critical' : 'warning',
          sensor: 'pH',
          message: `${outOfRange.length} leituras fora da faixa ideal (${idealRanges.ph.min}-${idealRanges.ph.max})`,
          timestamp: outOfRange[outOfRange.length - 1].timestamp,
        });
      }
    }

    if (idealRanges?.ec) {
      const outOfRange = data.filter(r => r.ec < idealRanges.ec!.min || r.ec > idealRanges.ec!.max);
      if (outOfRange.length > 0) {
        analytics.alerts.push({
          type: outOfRange.length > data.length * 0.3 ? 'critical' : 'warning',
          sensor: 'EC',
          message: `${outOfRange.length} leituras fora da faixa ideal (${idealRanges.ec.min}-${idealRanges.ec.max})`,
          timestamp: outOfRange[outOfRange.length - 1].timestamp,
        });
      }
    }

    // Check for anomalies
    Object.entries(analytics).forEach(([key, value]) => {
      if (key !== 'alerts' && (value as AnalyticsResult).anomalies.length > 0) {
        const sensorName = key === 'ph' ? 'pH' : key === 'ec' ? 'EC' : key === 'air_temp' ? 'Temp. Ar' : key === 'water_temp' ? 'Temp. Água' : 'Umidade';
        analytics.alerts.push({
          type: 'warning',
          sensor: sensorName,
          message: `${(value as AnalyticsResult).anomalies.length} anomalias detectadas`,
          timestamp: data[data.length - 1].timestamp,
        });
      }
    });

    return analytics;
  }, [data, idealRanges]);
};
