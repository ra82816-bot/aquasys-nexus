import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { FileText, ExternalLink, CheckCircle, Clock, XCircle, Loader2 } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { format } from "date-fns";
import { ptBR } from "date-fns/locale";

interface KnowledgeItem {
  id: string;
  title: string;
  content_type: string;
  summary: string | null;
  source_url: string | null;
  topics: string[] | null;
  processing_status: string;
  verified: boolean;
  quality_score: number | null;
  created_at: string;
}

export const KnowledgeList = () => {
  const [knowledge, setKnowledge] = useState<KnowledgeItem[]>([]);
  const [loading, setLoading] = useState(true);
  const { toast } = useToast();

  const fetchKnowledge = async () => {
    try {
      const { data, error } = await supabase
        .from('knowledge_base')
        .select('*')
        .order('created_at', { ascending: false });

      if (error) throw error;
      setKnowledge(data || []);
    } catch (error) {
      console.error('Error fetching knowledge:', error);
      toast({
        title: "Erro",
        description: "Não foi possível carregar a base de conhecimento",
        variant: "destructive",
      });
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchKnowledge();

    // Subscribe to realtime changes
    const channel = supabase
      .channel('knowledge-changes')
      .on(
        'postgres_changes',
        {
          event: '*',
          schema: 'public',
          table: 'knowledge_base'
        },
        () => {
          fetchKnowledge();
        }
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  }, []);

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'completed':
        return <CheckCircle className="h-4 w-4 text-green-500" />;
      case 'processing':
        return <Loader2 className="h-4 w-4 text-blue-500 animate-spin" />;
      case 'failed':
        return <XCircle className="h-4 w-4 text-red-500" />;
      default:
        return <Clock className="h-4 w-4 text-yellow-500" />;
    }
  };

  const getStatusLabel = (status: string) => {
    switch (status) {
      case 'completed':
        return 'Processado';
      case 'processing':
        return 'Processando';
      case 'failed':
        return 'Falha';
      default:
        return 'Pendente';
    }
  };

  const getContentTypeLabel = (type: string) => {
    const labels: Record<string, string> = {
      'scientific_paper': 'Artigo Científico',
      'guide': 'Guia de Cultivo',
      'manual': 'Manual Técnico',
      'research': 'Trabalho de Pesquisa',
      'forum_post': 'Post do Fórum',
      'article': 'Artigo Web',
      'pdf_document': 'Documento PDF',
      'video_transcript': 'Transcrição de Vídeo',
      'case_study': 'Estudo de Caso',
      'user_experience': 'Experiência de Usuário',
    };
    return labels[type] || type;
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center py-12">
        <Loader2 className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  if (knowledge.length === 0) {
    return (
      <Card>
        <CardContent className="flex flex-col items-center justify-center py-12">
          <FileText className="h-12 w-12 text-muted-foreground mb-4" />
          <h3 className="text-lg font-semibold mb-2">Nenhum conteúdo adicionado</h3>
          <p className="text-muted-foreground text-center">
            Comece adicionando artigos ou fazendo upload de documentos para construir sua base de conhecimento
          </p>
        </CardContent>
      </Card>
    );
  }

  return (
    <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-3">
      {knowledge.map((item) => (
        <Card key={item.id} className="hover:shadow-lg transition-shadow">
          <CardHeader>
            <div className="flex items-start justify-between gap-2">
              <div className="flex-1 min-w-0">
                <CardTitle className="text-base line-clamp-2">{item.title}</CardTitle>
                <CardDescription className="text-xs mt-1">
                  {format(new Date(item.created_at), "dd 'de' MMMM, yyyy", { locale: ptBR })}
                </CardDescription>
              </div>
              <div className="flex items-center gap-2 flex-shrink-0">
                {getStatusIcon(item.processing_status)}
                {item.verified && (
                  <CheckCircle className="h-4 w-4 text-primary" />
                )}
              </div>
            </div>
          </CardHeader>
          <CardContent className="space-y-3">
            {item.summary && (
              <p className="text-sm text-muted-foreground line-clamp-3">
                {item.summary}
              </p>
            )}

            <div className="flex flex-wrap gap-2">
              <Badge variant="outline" className="text-xs">
                {getContentTypeLabel(item.content_type)}
              </Badge>
              <Badge variant="secondary" className="text-xs">
                {getStatusLabel(item.processing_status)}
              </Badge>
              {item.quality_score && (
                <Badge variant="outline" className="text-xs">
                  Qualidade: {(item.quality_score * 100).toFixed(0)}%
                </Badge>
              )}
            </div>

            {item.topics && item.topics.length > 0 && (
              <div className="flex flex-wrap gap-1">
                {item.topics.slice(0, 3).map((topic, index) => (
                  <Badge key={index} variant="secondary" className="text-xs">
                    {topic}
                  </Badge>
                ))}
                {item.topics.length > 3 && (
                  <Badge variant="secondary" className="text-xs">
                    +{item.topics.length - 3}
                  </Badge>
                )}
              </div>
            )}

            {item.source_url && (
              <Button
                variant="ghost"
                size="sm"
                className="w-full gap-2"
                onClick={() => window.open(item.source_url!, '_blank')}
              >
                <ExternalLink className="h-3 w-3" />
                Ver fonte
              </Button>
            )}
          </CardContent>
        </Card>
      ))}
    </div>
  );
};
