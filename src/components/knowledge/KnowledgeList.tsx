import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { FileText, ExternalLink, CheckCircle, Clock, XCircle, Loader2, Trash2 } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { format } from "date-fns";
import { ptBR } from "date-fns/locale";

interface KnowledgeListProps {
  onStorageUpdate: () => void;
}

interface KnowledgeItem {
  id: string;
  title: string;
  description: string | null;
  category: string | null;
  source_type: string | null;
  is_trusted: boolean;
  processing_status: string;
  file_size: number | null;
  file_path: string | null;
  created_at: string;
}

export const KnowledgeList = ({ onStorageUpdate }: KnowledgeListProps) => {
  const [knowledge, setKnowledge] = useState<KnowledgeItem[]>([]);
  const [loading, setLoading] = useState(true);
  const { toast } = useToast();

  const fetchKnowledge = async () => {
    try {
      const { data, error } = await supabase
        .from('knowledge')
        .select('*')
        .order('created_at', { ascending: false });

      if (error) throw error;
      setKnowledge(data || []);
    } catch (error) {
      console.error('Error fetching knowledge:', error);
      toast({
        title: "Erro",
        description: "Não foi possível carregar os materiais",
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
          table: 'knowledge'
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
      case 'indexed':
        return <CheckCircle className="h-4 w-4 text-green-500" />;
      case 'processing':
        return <Loader2 className="h-4 w-4 text-blue-500 animate-spin" />;
      case 'error':
        return <XCircle className="h-4 w-4 text-red-500" />;
      default:
        return <Clock className="h-4 w-4 text-yellow-500" />;
    }
  };

  const getStatusLabel = (status: string) => {
    switch (status) {
      case 'indexed':
        return 'Indexado';
      case 'processing':
        return 'Processando';
      case 'error':
        return 'Erro';
      default:
        return 'Pendente';
    }
  };

  const getSourceTypeLabel = (type: string | null) => {
    if (!type) return 'Desconhecido';
    const labels: Record<string, string> = {
      'pdf': 'PDF',
      'txt': 'Texto',
      'csv': 'Planilha CSV',
      'docx': 'Word',
      'xlsx': 'Excel',
      'link': 'Link/Artigo'
    };
    return labels[type] || type;
  };

  const handleDelete = async (id: string, filePath: string | null) => {
    try {
      // Deletar arquivo do storage se existir
      if (filePath) {
        await supabase.storage
          .from('knowledge-materials')
          .remove([filePath]);
      }

      // Deletar registro do banco
      const { error } = await supabase
        .from('knowledge')
        .delete()
        .eq('id', id);

      if (error) throw error;

      toast({
        title: "Material removido",
        description: "O material foi removido com sucesso"
      });

      onStorageUpdate();
    } catch (error) {
      console.error('Error deleting:', error);
      toast({
        title: "Erro",
        description: "Não foi possível remover o material",
        variant: "destructive"
      });
    }
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
          <h3 className="text-lg font-semibold mb-2">Nenhum material adicionado</h3>
          <p className="text-muted-foreground text-center">
            Comece enviando documentos ou links para treinar a IA com seus próprios materiais
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
                {item.is_trusted && (
                  <CheckCircle className="h-4 w-4 text-primary" />
                )}
              </div>
            </div>
          </CardHeader>
          <CardContent className="space-y-3">
            {item.description && (
              <p className="text-sm text-muted-foreground line-clamp-3">
                {item.description}
              </p>
            )}

            <div className="flex flex-wrap gap-2">
              {item.source_type && (
                <Badge variant="outline" className="text-xs">
                  {getSourceTypeLabel(item.source_type)}
                </Badge>
              )}
              <Badge variant="secondary" className="text-xs">
                {getStatusLabel(item.processing_status)}
              </Badge>
              {item.file_size && (
                <Badge variant="outline" className="text-xs">
                  {(item.file_size / 1048576).toFixed(2)} MB
                </Badge>
              )}
            </div>

            {item.category && (
              <Badge variant="secondary" className="text-xs">
                {item.category}
              </Badge>
            )}

            <div className="flex gap-2 pt-2">
              <Button
                variant="ghost"
                size="sm"
                className="flex-1 gap-2 text-destructive hover:text-destructive"
                onClick={() => handleDelete(item.id, item.file_path || null)}
              >
                <Trash2 className="h-3 w-3" />
                Remover
              </Button>
            </div>
          </CardContent>
        </Card>
      ))}
    </div>
  );
};
