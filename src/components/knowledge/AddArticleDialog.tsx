import { useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Textarea } from "@/components/ui/textarea";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { Checkbox } from "@/components/ui/checkbox";
import { FileText, Loader2 } from "lucide-react";

interface AddArticleDialogProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  onSuccess: () => void;
}

export const AddArticleDialog = ({ open, onOpenChange, onSuccess }: AddArticleDialogProps) => {
  const { toast } = useToast();
  const [isLoading, setIsLoading] = useState(false);
  const [title, setTitle] = useState("");
  const [content, setContent] = useState("");
  const [sourceUrl, setSourceUrl] = useState("");
  const [category, setCategory] = useState("");
  const [isTrusted, setIsTrusted] = useState(false);

  const categories = [
    "Nutrientes",
    "Fotoperíodo",
    "Controle de pH",
    "Controle de EC",
    "Pragas e Doenças",
    "Genética",
    "Cultivo Geral",
    "Outro"
  ];

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    setIsLoading(true);

    try {
      const { data: { user } } = await supabase.auth.getUser();
      if (!user) throw new Error("User not authenticated");

      // Criar registro no banco
      const { data: knowledge, error: dbError } = await supabase
        .from('knowledge')
        .insert({
          user_id: user.id,
          title,
          description: content,
          category,
          source_type: 'link',
          is_trusted: isTrusted,
          content: content,
          processing_status: 'pending'
        })
        .select()
        .single();

      if (dbError) throw dbError;

      // Processar em background
      supabase.functions.invoke('process-knowledge-upload', {
        body: { knowledgeId: knowledge.id }
      });

      toast({
        title: "Artigo adicionado",
        description: "O conteúdo está sendo processado pela IA"
      });

      setTitle("");
      setContent("");
      setSourceUrl("");
      setCategory("");
      setIsTrusted(false);
      onSuccess();
      onOpenChange(false);

    } catch (error) {
      console.error("Error saving:", error);
      toast({
        title: "Erro ao salvar",
        description: error instanceof Error ? error.message : "Erro desconhecido",
        variant: "destructive"
      });
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-2xl max-h-[90vh] overflow-y-auto">
        <DialogHeader>
          <DialogTitle>Adicionar Link ou Artigo</DialogTitle>
          <DialogDescription>
            Cole o conteúdo de artigos, links ou textos para treinar a IA
          </DialogDescription>
        </DialogHeader>

        <form onSubmit={handleSubmit} className="space-y-4">
          <div>
            <Label htmlFor="title">Título *</Label>
            <Input
              id="title"
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              placeholder="Ex: Como corrigir deficiência de nitrogênio"
              disabled={isLoading}
              required
            />
          </div>

          <div>
            <Label htmlFor="content">Conteúdo *</Label>
            <Textarea
              id="content"
              value={content}
              onChange={(e) => setContent(e.target.value)}
              placeholder="Cole aqui o conteúdo do artigo ou texto..."
              className="min-h-[200px]"
              disabled={isLoading}
              required
            />
          </div>

          <div>
            <Label htmlFor="sourceUrl">URL da Fonte (opcional)</Label>
            <Input
              id="sourceUrl"
              type="url"
              value={sourceUrl}
              onChange={(e) => setSourceUrl(e.target.value)}
              placeholder="https://exemplo.com/artigo"
              disabled={isLoading}
            />
          </div>

          <div>
            <Label htmlFor="category">Categoria</Label>
            <Select value={category} onValueChange={setCategory} disabled={isLoading} required>
              <SelectTrigger>
                <SelectValue placeholder="Selecione uma categoria" />
              </SelectTrigger>
              <SelectContent>
                {categories.map((cat) => (
                  <SelectItem key={cat} value={cat}>
                    {cat}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </div>

          <div className="flex items-center space-x-2">
            <Checkbox
              id="trusted"
              checked={isTrusted}
              onCheckedChange={(checked) => setIsTrusted(checked as boolean)}
              disabled={isLoading}
            />
            <Label htmlFor="trusted" className="cursor-pointer">
              Marcar como fonte confiável
            </Label>
          </div>

          <div className="flex gap-2 justify-end pt-4">
            <Button type="button" variant="outline" onClick={() => onOpenChange(false)} disabled={isLoading}>
              Cancelar
            </Button>
            <Button type="submit" disabled={isLoading || !title || !content}>
              {isLoading ? (
                <>
                  <Loader2 className="mr-2 h-4 w-4 animate-spin" />
                  Salvando...
                </>
              ) : (
                <>
                  <FileText className="mr-2 h-4 w-4" />
                  Salvar
                </>
              )}
            </Button>
          </div>
        </form>
      </DialogContent>
    </Dialog>
  );
};
