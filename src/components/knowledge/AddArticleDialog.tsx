import { useState } from "react";
import { Button } from "@/components/ui/button";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { Textarea } from "@/components/ui/textarea";
import { Plus, Loader2 } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { supabase } from "@/integrations/supabase/client";

export const AddArticleDialog = () => {
  const [open, setOpen] = useState(false);
  const [saving, setSaving] = useState(false);
  const [title, setTitle] = useState("");
  const [content, setContent] = useState("");
  const [sourceUrl, setSourceUrl] = useState("");
  const [contentType, setContentType] = useState<"article" | "guide" | "forum_post" | "manual">("article");
  const [topics, setTopics] = useState("");
  const { toast } = useToast();

  const handleSave = async () => {
    if (!title || !content) {
      toast({
        title: "Erro",
        description: "Por favor, preencha título e conteúdo",
        variant: "destructive",
      });
      return;
    }

    setSaving(true);

    try {
      const topicsArray = topics ? topics.split(',').map(t => t.trim()) : [];

      // Insert into knowledge_base
      const { data, error } = await supabase
        .from('knowledge_base')
        .insert({
          title,
          content_type: contentType,
          original_content: content,
          source_url: sourceUrl || null,
          topics: topicsArray,
          processing_status: 'pending',
        })
        .select()
        .single();

      if (error) throw error;

      // Call processing function
      const { error: processError } = await supabase.functions.invoke('process-knowledge', {
        body: {
          knowledgeId: data.id,
          content,
          title,
          contentType,
        }
      });

      if (processError) {
        console.error('Processing error:', processError);
      }

      toast({
        title: "Sucesso!",
        description: "Artigo adicionado e será processado em breve",
      });

      setOpen(false);
      setTitle("");
      setContent("");
      setSourceUrl("");
      setTopics("");
    } catch (error) {
      console.error('Save error:', error);
      toast({
        title: "Erro",
        description: "Não foi possível salvar o artigo",
        variant: "destructive",
      });
    } finally {
      setSaving(false);
    }
  };

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button variant="outline" className="gap-2">
          <Plus className="h-4 w-4" />
          Adicionar Artigo
        </Button>
      </DialogTrigger>
      <DialogContent className="sm:max-w-[600px]">
        <DialogHeader>
          <DialogTitle>Adicionar Artigo</DialogTitle>
          <DialogDescription>
            Adicione artigos ou conteúdo web à base de conhecimento
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-4 py-4">
          <div className="space-y-2">
            <Label htmlFor="title">Título *</Label>
            <Input
              id="title"
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              placeholder="Ex: Como corrigir deficiência de nitrogênio"
              disabled={saving}
            />
          </div>

          <div className="space-y-2">
            <Label htmlFor="content">Conteúdo *</Label>
            <Textarea
              id="content"
              value={content}
              onChange={(e) => setContent(e.target.value)}
              placeholder="Cole aqui o conteúdo do artigo..."
              className="min-h-[200px]"
              disabled={saving}
            />
          </div>

          <div className="space-y-2">
            <Label htmlFor="sourceUrl">URL da Fonte (opcional)</Label>
            <Input
              id="sourceUrl"
              type="url"
              value={sourceUrl}
              onChange={(e) => setSourceUrl(e.target.value)}
              placeholder="https://exemplo.com/artigo"
              disabled={saving}
            />
          </div>

          <div className="space-y-2">
            <Label htmlFor="contentType">Tipo de Conteúdo</Label>
            <Select value={contentType} onValueChange={(value) => setContentType(value as typeof contentType)} disabled={saving}>
              <SelectTrigger id="contentType">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="article">Artigo Web</SelectItem>
                <SelectItem value="guide">Guia de Cultivo</SelectItem>
                <SelectItem value="forum_post">Post do Fórum</SelectItem>
                <SelectItem value="manual">Manual Técnico</SelectItem>
              </SelectContent>
            </Select>
          </div>

          <div className="space-y-2">
            <Label htmlFor="topics">Tópicos (separados por vírgula)</Label>
            <Textarea
              id="topics"
              value={topics}
              onChange={(e) => setTopics(e.target.value)}
              placeholder="Ex: deficiência nutricional, nitrogênio, diagnóstico"
              disabled={saving}
            />
          </div>
        </div>

        <div className="flex justify-end gap-2">
          <Button variant="outline" onClick={() => setOpen(false)} disabled={saving}>
            Cancelar
          </Button>
          <Button onClick={handleSave} disabled={saving || !title || !content}>
            {saving ? (
              <>
                <Loader2 className="mr-2 h-4 w-4 animate-spin" />
                Salvando...
              </>
            ) : (
              "Salvar"
            )}
          </Button>
        </div>
      </DialogContent>
    </Dialog>
  );
};
