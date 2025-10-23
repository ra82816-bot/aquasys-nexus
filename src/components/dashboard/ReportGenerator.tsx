import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { FileDown, FileText, Loader2 } from "lucide-react";
import { SensorAnalytics, SensorReading } from "@/hooks/useChartAnalytics";
import { toast } from "sonner";
import jsPDF from 'jspdf';

interface ReportGeneratorProps {
  data: SensorReading[];
  analytics: SensorAnalytics | null;
  startDate: Date;
  endDate: Date;
}

export const ReportGenerator = ({ data, analytics, startDate, endDate }: ReportGeneratorProps) => {
  const [generating, setGenerating] = useState(false);

  const generateCSV = () => {
    if (data.length === 0) {
      toast.error("Nenhum dado disponível para exportar");
      return;
    }

    setGenerating(true);
    try {
      // CSV Headers
      const headers = ['Timestamp', 'pH', 'EC', 'Temp. Ar (°C)', 'Temp. Água (°C)', 'Umidade (%)'];
      const csvContent = [
        headers.join(','),
        ...data.map(row => 
          [row.timestamp, row.ph, row.ec, row.air_temp, row.water_temp, row.humidity].join(',')
        )
      ].join('\n');

      // Create download link
      const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
      const link = document.createElement('a');
      const url = URL.createObjectURL(blob);
      link.setAttribute('href', url);
      link.setAttribute('download', `relatorio_sensores_${startDate.toISOString().split('T')[0]}_${endDate.toISOString().split('T')[0]}.csv`);
      link.style.visibility = 'hidden';
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);

      toast.success("Relatório CSV gerado com sucesso!");
    } catch (error) {
      console.error('Erro ao gerar CSV:', error);
      toast.error("Erro ao gerar relatório CSV");
    } finally {
      setGenerating(false);
    }
  };

  const generatePDF = () => {
    if (!analytics || data.length === 0) {
      toast.error("Nenhum dado disponível para gerar relatório");
      return;
    }

    setGenerating(true);
    try {
      const doc = new jsPDF();
      let yPosition = 20;

      // Title
      doc.setFontSize(18);
      doc.text('Relatório de Análise do Cultivo', 105, yPosition, { align: 'center' });
      yPosition += 15;

      // Period
      doc.setFontSize(12);
      doc.text(`Período: ${startDate.toLocaleDateString('pt-BR')} a ${endDate.toLocaleDateString('pt-BR')}`, 20, yPosition);
      yPosition += 10;
      doc.text(`Total de leituras: ${data.length}`, 20, yPosition);
      yPosition += 15;

      // Statistics for each sensor
      const sensors = [
        { key: 'ph', name: 'pH', unit: '' },
        { key: 'ec', name: 'Condutividade Elétrica', unit: 'μS/cm' },
        { key: 'air_temp', name: 'Temperatura do Ar', unit: '°C' },
        { key: 'water_temp', name: 'Temperatura da Água', unit: '°C' },
        { key: 'humidity', name: 'Umidade', unit: '%' }
      ];

      doc.setFontSize(14);
      doc.text('Estatísticas dos Sensores', 20, yPosition);
      yPosition += 10;

      doc.setFontSize(10);
      sensors.forEach(sensor => {
        const stats = analytics[sensor.key as keyof typeof analytics] as any;
        if (stats && typeof stats === 'object' && 'average' in stats) {
          doc.text(`${sensor.name} (${sensor.unit}):`, 20, yPosition);
          yPosition += 5;
          doc.text(`  Média: ${stats.average} | Mín: ${stats.min} | Máx: ${stats.max}`, 25, yPosition);
          yPosition += 5;
          doc.text(`  Desvio Padrão: ${stats.stdDev} | Tendência: ${stats.trend} | Estabilidade: ${stats.stability}`, 25, yPosition);
          yPosition += 5;
          if (stats.anomalies.length > 0) {
            doc.text(`  ⚠ ${stats.anomalies.length} anomalia(s) detectada(s)`, 25, yPosition);
            yPosition += 5;
          }
          yPosition += 3;

          if (yPosition > 270) {
            doc.addPage();
            yPosition = 20;
          }
        }
      });

      // Alerts section
      if (analytics.alerts.length > 0) {
        yPosition += 10;
        if (yPosition > 250) {
          doc.addPage();
          yPosition = 20;
        }

        doc.setFontSize(14);
        doc.text('Alertas e Recomendações', 20, yPosition);
        yPosition += 10;

        doc.setFontSize(10);
        analytics.alerts.forEach(alert => {
          const alertText = `${alert.type === 'critical' ? '🔴' : '⚠'} [${alert.sensor}] ${alert.message}`;
          doc.text(alertText, 20, yPosition);
          yPosition += 6;

          if (yPosition > 270) {
            doc.addPage();
            yPosition = 20;
          }
        });
      }

      // Save PDF
      doc.save(`relatorio_cultivo_${startDate.toISOString().split('T')[0]}_${endDate.toISOString().split('T')[0]}.pdf`);
      toast.success("Relatório PDF gerado com sucesso!");
    } catch (error) {
      console.error('Erro ao gerar PDF:', error);
      toast.error("Erro ao gerar relatório PDF");
    } finally {
      setGenerating(false);
    }
  };

  return (
    <Card className="bg-card/50 backdrop-blur-sm">
      <CardHeader>
        <CardTitle className="text-base flex items-center gap-2">
          <FileText className="h-5 w-5" />
          Gerar Relatório
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <Button
          onClick={generatePDF}
          disabled={generating || !analytics || data.length === 0}
          className="w-full gap-2"
          variant="default"
        >
          {generating ? (
            <Loader2 className="h-4 w-4 animate-spin" />
          ) : (
            <FileDown className="h-4 w-4" />
          )}
          Exportar PDF Completo
        </Button>

        <Button
          onClick={generateCSV}
          disabled={generating || data.length === 0}
          className="w-full gap-2"
          variant="outline"
        >
          {generating ? (
            <Loader2 className="h-4 w-4 animate-spin" />
          ) : (
            <FileDown className="h-4 w-4" />
          )}
          Exportar CSV (Dados Brutos)
        </Button>

        <p className="text-xs text-muted-foreground">
          {data.length === 0 
            ? "Sem dados para exportar" 
            : `${data.length} leituras disponíveis para exportação`
          }
        </p>
      </CardContent>
    </Card>
  );
};
