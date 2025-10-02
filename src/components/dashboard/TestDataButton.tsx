import { useState } from "react";
import { Button } from "@/components/ui/button";
import { useToast } from "@/hooks/use-toast";
import { Beaker } from "lucide-react";

export const TestDataButton = () => {
  const [loading, setLoading] = useState(false);
  const { toast } = useToast();

  const generateTestData = async () => {
    setLoading(true);
    try {
      const response = await fetch(
        `${import.meta.env.VITE_SUPABASE_URL}/functions/v1/insert-test-reading`,
        {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
        }
      );

      if (!response.ok) throw new Error('Falha ao gerar dados de teste');

      toast({
        title: "Dados de teste gerados!",
        description: "Uma nova leitura foi inserida no sistema.",
      });
    } catch (error) {
      toast({
        title: "Erro",
        description: error instanceof Error ? error.message : "Erro desconhecido",
        variant: "destructive",
      });
    } finally {
      setLoading(false);
    }
  };

  return (
    <Button
      onClick={generateTestData}
      disabled={loading}
      variant="outline"
      size="sm"
      className="gap-2"
    >
      <Beaker className="h-4 w-4" />
      {loading ? "Gerando..." : "Gerar Dados de Teste"}
    </Button>
  );
};