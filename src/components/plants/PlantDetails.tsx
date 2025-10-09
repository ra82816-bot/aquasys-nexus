import { useEffect, useState } from "react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { 
  Sprout, Calendar, Droplet, Zap, Thermometer, Wind, 
  TrendingUp, FileText, Brain, Camera, Edit, ArrowLeft 
} from "lucide-react";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Legend } from "recharts";
import { Alert, AlertDescription } from "@/components/ui/alert";

interface PlantDetailsProps {
  plantId: string;
  onBack: () => void;
  onEdit: () => void;
}

export const PlantDetails = ({ plantId, onBack, onEdit }: PlantDetailsProps) => {
  const [plant, setPlant] = useState<any>(null);
  const [observations, setObservations] = useState<any[]>([]);
  const [sensorHistory, setSensorHistory] = useState<any[]>([]);
  const [aiInsights, setAiInsights] = useState<any[]>([]);
  const [loading, setLoading] = useState(true);
  const { toast } = useToast();

  useEffect(() => {
    loadPlantData();
  }, [plantId]);

  const loadPlantData = async () => {
    try {
      setLoading(true);

      // Carregar dados da planta
      const { data: plantData, error: plantError } = await supabase
        .from('plants')
        .select(`
          *,
          species_profile:plant_species_profiles(*)
        `)
        .eq('id', plantId)
        .single();

      if (plantError) throw plantError;
      setPlant(plantData);

      // Carregar observações
      const { data: obsData, error: obsError } = await supabase
        .from('plant_observations')
        .select('*')
        .eq('plant_id', plantId)
        .order('observation_date', { ascending: false });

      if (!obsError) setObservations(obsData || []);

      // Carregar histórico de sensores (últimos 30 dias)
      const thirtyDaysAgo = new Date();
      thirtyDaysAgo.setDate(thirtyDaysAgo.getDate() - 30);

      const { data: sensorData, error: sensorError } = await supabase
        .from('readings')
        .select('*')
        .gte('timestamp', thirtyDaysAgo.toISOString())
        .order('timestamp', { ascending: true });

      if (!sensorError) setSensorHistory(sensorData || []);

      // Carregar insights de IA
      const { data: insightsData, error: insightsError } = await supabase
        .from('ai_analysis_history')
        .select('*')
        .eq('plant_id', plantId)
        .order('created_at', { ascending: false })
        .limit(5);

      if (!insightsError) setAiInsights(insightsData || []);

    } catch (error: any) {
      console.error('Erro ao carregar dados da planta:', error);
      toast({
        title: "Erro",
        description: "Não foi possível carregar os dados da planta",
        variant: "destructive"
      });
    } finally {
      setLoading(false);
    }
  };

  const getStatusColor = (status: string) => {
    const colors: Record<string, string> = {
      germinating: 'bg-yellow-500/10 text-yellow-600 border-yellow-500/20',
      vegetative: 'bg-green-500/10 text-green-600 border-green-500/20',
      flowering: 'bg-purple-500/10 text-purple-600 border-purple-500/20',
      harvested: 'bg-blue-500/10 text-blue-600 border-blue-500/20',
      discarded: 'bg-red-500/10 text-red-600 border-red-500/20'
    };
    return colors[status] || 'bg-muted';
  };

  const getStatusLabel = (status: string) => {
    const labels: Record<string, string> = {
      germinating: 'Germinando',
      vegetative: 'Vegetativo',
      flowering: 'Floração',
      harvested: 'Colhida',
      discarded: 'Descartada'
    };
    return labels[status] || status;
  };

  const getDaysInStage = () => {
    if (!plant?.stage_started_at) return 0;
    const start = new Date(plant.stage_started_at);
    const now = new Date();
    return Math.floor((now.getTime() - start.getTime()) / (1000 * 60 * 60 * 24));
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center p-8">
        <div className="text-center">
          <div className="h-12 w-12 border-4 border-primary border-t-transparent rounded-full animate-spin mx-auto mb-4" />
          <p className="text-muted-foreground">Carregando dados...</p>
        </div>
      </div>
    );
  }

  if (!plant) {
    return (
      <Alert className="border-red-500/50 bg-red-500/10">
        <AlertDescription>Planta não encontrada</AlertDescription>
      </Alert>
    );
  }

  return (
    <div className="space-y-4">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-3">
          <Button variant="ghost" size="icon" onClick={onBack}>
            <ArrowLeft className="h-5 w-5" />
          </Button>
          <div>
            <h2 className="text-2xl font-bold flex items-center gap-2">
              <Sprout className="h-6 w-6 text-primary" />
              {plant.nickname}
            </h2>
            <p className="text-sm text-muted-foreground">{plant.genetics || plant.species}</p>
          </div>
        </div>
        <div className="flex items-center gap-2">
          <Badge variant="outline" className={`border ${getStatusColor(plant.status)}`}>
            {getStatusLabel(plant.status)}
          </Badge>
          <Button onClick={onEdit} variant="outline" size="sm">
            <Edit className="h-4 w-4 mr-2" />
            Editar
          </Button>
        </div>
      </div>

      {/* Informações gerais */}
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Informações Gerais</CardTitle>
        </CardHeader>
        <CardContent>
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
            <div>
              <p className="text-xs text-muted-foreground mb-1">Dias nesta fase</p>
              <p className="text-2xl font-bold">{getDaysInStage()}</p>
            </div>
            <div>
              <p className="text-xs text-muted-foreground mb-1">Origem</p>
              <p className="text-lg font-semibold capitalize">{plant.origin === 'seed' ? 'Semente' : 'Clone'}</p>
            </div>
            <div>
              <p className="text-xs text-muted-foreground mb-1">pH Médio</p>
              <p className="text-2xl font-bold">{plant.avg_ph?.toFixed(2) || '--'}</p>
            </div>
            <div>
              <p className="text-xs text-muted-foreground mb-1">Temp. Média</p>
              <p className="text-2xl font-bold">{plant.avg_temperature?.toFixed(1) || '--'}°C</p>
            </div>
          </div>

          {plant.germination_date && (
            <div className="mt-4 flex items-center gap-2 text-sm">
              <Calendar className="h-4 w-4 text-muted-foreground" />
              <span className="text-muted-foreground">
                Germinação: {new Date(plant.germination_date).toLocaleDateString('pt-BR')}
              </span>
            </div>
          )}
        </CardContent>
      </Card>

      <Tabs defaultValue="environment" className="w-full">
        <TabsList className="grid w-full grid-cols-4">
          <TabsTrigger value="environment">Ambiente</TabsTrigger>
          <TabsTrigger value="cultivation">Cultivo</TabsTrigger>
          <TabsTrigger value="ai">IA</TabsTrigger>
          <TabsTrigger value="timeline">Linha do Tempo</TabsTrigger>
        </TabsList>

        <TabsContent value="environment" className="space-y-4">
          <Card>
            <CardHeader>
              <CardTitle>Histórico Ambiental (30 dias)</CardTitle>
            </CardHeader>
            <CardContent>
              {sensorHistory.length > 0 ? (
                <div className="h-64">
                  <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={sensorHistory}>
                      <CartesianGrid strokeDasharray="3 3" opacity={0.3} />
                      <XAxis 
                        dataKey="timestamp" 
                        tickFormatter={(value) => new Date(value).toLocaleDateString('pt-BR', { day: '2-digit', month: '2-digit' })}
                        tick={{ fontSize: 10 }}
                      />
                      <YAxis yAxisId="left" tick={{ fontSize: 10 }} />
                      <YAxis yAxisId="right" orientation="right" tick={{ fontSize: 10 }} />
                      <Tooltip 
                        labelFormatter={(value) => new Date(value).toLocaleString('pt-BR')}
                      />
                      <Legend />
                      <Line yAxisId="left" type="monotone" dataKey="ph" stroke="#8b5cf6" strokeWidth={2} name="pH" />
                      <Line yAxisId="right" type="monotone" dataKey="ec" stroke="#f59e0b" strokeWidth={2} name="EC" />
                      <Line yAxisId="right" type="monotone" dataKey="air_temp" stroke="#ef4444" strokeWidth={2} name="Temp (°C)" />
                    </LineChart>
                  </ResponsiveContainer>
                </div>
              ) : (
                <p className="text-muted-foreground text-center py-8">Nenhum dado disponível</p>
              )}
            </CardContent>
          </Card>
        </TabsContent>

        <TabsContent value="cultivation" className="space-y-4">
          <div className="grid md:grid-cols-2 gap-4">
            <Card>
              <CardHeader>
                <CardTitle className="text-base">Sistema de Cultivo</CardTitle>
              </CardHeader>
              <CardContent className="space-y-3">
                <div>
                  <p className="text-xs text-muted-foreground">Substrato</p>
                  <p className="font-medium">{plant.substrate_type || 'Não especificado'}</p>
                </div>
                <div>
                  <p className="text-xs text-muted-foreground">Nutentes</p>
                  <p className="font-medium">{plant.nutrients_type || 'Não especificado'}</p>
                </div>
                <div>
                  <p className="text-xs text-muted-foreground">Ciclo de Luz</p>
                  <p className="font-medium">{plant.light_cycle || 'Não especificado'}</p>
                </div>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <CardTitle className="text-base">Métricas de Qualidade</CardTitle>
              </CardHeader>
              <CardContent className="space-y-3">
                {plant.quality_resin && (
                  <div className="flex items-center justify-between">
                    <span className="text-sm">Resina</span>
                    <div className="flex gap-1">
                      {[...Array(5)].map((_, i) => (
                        <div 
                          key={i} 
                          className={`h-2 w-6 rounded ${i < plant.quality_resin ? 'bg-primary' : 'bg-muted'}`}
                        />
                      ))}
                    </div>
                  </div>
                )}
                {plant.quality_density && (
                  <div className="flex items-center justify-between">
                    <span className="text-sm">Densidade</span>
                    <div className="flex gap-1">
                      {[...Array(5)].map((_, i) => (
                        <div 
                          key={i} 
                          className={`h-2 w-6 rounded ${i < plant.quality_density ? 'bg-primary' : 'bg-muted'}`}
                        />
                      ))}
                    </div>
                  </div>
                )}
                {plant.yield_grams && (
                  <div>
                    <p className="text-xs text-muted-foreground">Colheita</p>
                    <p className="text-2xl font-bold">{plant.yield_grams}g</p>
                  </div>
                )}
              </CardContent>
            </Card>
          </div>

          {plant.general_notes && (
            <Card>
              <CardHeader>
                <CardTitle className="text-base flex items-center gap-2">
                  <FileText className="h-4 w-4" />
                  Observações
                </CardTitle>
              </CardHeader>
              <CardContent>
                <p className="text-sm whitespace-pre-wrap">{plant.general_notes}</p>
              </CardContent>
            </Card>
          )}
        </TabsContent>

        <TabsContent value="ai" className="space-y-4">
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Brain className="h-5 w-5 text-primary" />
                Recomendações da IA
              </CardTitle>
            </CardHeader>
            <CardContent className="space-y-4">
              {aiInsights.length > 0 ? (
                aiInsights.map((insight) => (
                  <Card key={insight.id} className="border-primary/20">
                    <CardHeader>
                      <div className="flex items-center justify-between">
                        <p className="text-sm text-muted-foreground">
                          {new Date(insight.created_at).toLocaleDateString('pt-BR')}
                        </p>
                        {insight.confidence_score && (
                          <Badge variant="outline">
                            Confiança: {(insight.confidence_score * 100).toFixed(0)}%
                          </Badge>
                        )}
                      </div>
                    </CardHeader>
                    <CardContent>
                      {insight.insights_generated && (
                        <div className="space-y-2">
                          {Array.isArray(insight.insights_generated) ? (
                            insight.insights_generated.map((ins: any, idx: number) => (
                              <div key={idx} className="text-sm">
                                <p className="font-medium">{ins.title || ins.type}</p>
                                <p className="text-muted-foreground">{ins.description}</p>
                              </div>
                            ))
                          ) : (
                            <p className="text-sm">{JSON.stringify(insight.insights_generated)}</p>
                          )}
                        </div>
                      )}
                      {insight.recommendations && insight.recommendations.length > 0 && (
                        <div className="mt-3 space-y-1">
                          <p className="text-xs font-medium text-muted-foreground">Recomendações:</p>
                          <ul className="text-sm list-disc list-inside space-y-1">
                            {insight.recommendations.map((rec: string, idx: number) => (
                              <li key={idx}>{rec}</li>
                            ))}
                          </ul>
                        </div>
                      )}
                    </CardContent>
                  </Card>
                ))
              ) : (
                <p className="text-muted-foreground text-center py-8">
                  Nenhuma análise de IA disponível ainda
                </p>
              )}
            </CardContent>
          </Card>
        </TabsContent>

        <TabsContent value="timeline" className="space-y-4">
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Calendar className="h-5 w-5 text-primary" />
                Linha do Tempo
              </CardTitle>
            </CardHeader>
            <CardContent>
              <div className="space-y-4">
                {/* Evento de criação */}
                <div className="flex gap-3">
                  <div className="flex flex-col items-center">
                    <div className="h-3 w-3 rounded-full bg-primary" />
                    <div className="w-0.5 h-full bg-border" />
                  </div>
                  <div className="flex-1 pb-4">
                    <p className="text-sm font-medium">Planta registrada</p>
                    <p className="text-xs text-muted-foreground">
                      {new Date(plant.created_at).toLocaleString('pt-BR')}
                    </p>
                  </div>
                </div>

                {/* Observações */}
                {observations.map((obs, index) => (
                  <div key={obs.id} className="flex gap-3">
                    <div className="flex flex-col items-center">
                      <div className="h-3 w-3 rounded-full bg-primary" />
                      {index < observations.length - 1 && <div className="w-0.5 h-full bg-border" />}
                    </div>
                    <div className="flex-1 pb-4">
                      <p className="text-sm font-medium">Observação registrada</p>
                      <p className="text-xs text-muted-foreground mb-2">
                        {new Date(obs.observation_date).toLocaleDateString('pt-BR')}
                      </p>
                      {obs.notes && (
                        <p className="text-sm bg-muted/50 p-2 rounded">{obs.notes}</p>
                      )}
                      {obs.height_cm && (
                        <p className="text-xs text-muted-foreground mt-1">
                          Altura: {obs.height_cm} cm
                        </p>
                      )}
                    </div>
                  </div>
                ))}

                {observations.length === 0 && (
                  <p className="text-muted-foreground text-center py-4">
                    Nenhuma observação registrada
                  </p>
                )}
              </div>
            </CardContent>
          </Card>
        </TabsContent>
      </Tabs>
    </div>
  );
};
