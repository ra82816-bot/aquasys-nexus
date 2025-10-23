import { serve } from "https://deno.land/std@0.168.0/http/server.ts";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

serve(async (req) => {
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    const { analytics, period } = await req.json();
    const LOVABLE_API_KEY = Deno.env.get("LOVABLE_API_KEY");

    if (!LOVABLE_API_KEY) {
      throw new Error("LOVABLE_API_KEY não configurada");
    }

    // Build context for AI
    const context = `
Análise de dados de cultivo hidropônico para o período: ${period}

Estatísticas dos sensores:
- pH: Média ${analytics.ph.average}, Min ${analytics.ph.min}, Máx ${analytics.ph.max}, Desvio ${analytics.ph.stdDev}, Tendência: ${analytics.ph.trend}, Estabilidade: ${analytics.ph.stability}
- EC: Média ${analytics.ec.average}, Min ${analytics.ec.min}, Máx ${analytics.ec.max}, Desvio ${analytics.ec.stdDev}, Tendência: ${analytics.ec.trend}, Estabilidade: ${analytics.ec.stability}
- Temp. Ar: Média ${analytics.air_temp.average}, Min ${analytics.air_temp.min}, Máx ${analytics.air_temp.max}, Tendência: ${analytics.air_temp.trend}
- Temp. Água: Média ${analytics.water_temp.average}, Min ${analytics.water_temp.min}, Máx ${analytics.water_temp.max}, Tendência: ${analytics.water_temp.trend}
- Umidade: Média ${analytics.humidity.average}, Min ${analytics.humidity.min}, Máx ${analytics.humidity.max}, Tendência: ${analytics.humidity.trend}

Alertas detectados:
${analytics.alerts.map((a: any) => `- [${a.type}] ${a.sensor}: ${a.message}`).join('\n')}

Faixas ideais de referência:
- pH: 5.5 - 6.5
- EC: 800 - 1500 μS/cm
- Temperatura: 18 - 26°C
- Umidade: 50 - 70%
`;

    const systemPrompt = `Você é um especialista em cultivo hidropônico. Analise os dados fornecidos e gere insights práticos e acionáveis.

Forneça sua resposta em formato JSON com a seguinte estrutura:
{
  "summary": "Um resumo conciso em 2-3 frases sobre o estado geral do cultivo",
  "recommendations": ["Lista de 3-5 recomendações práticas e específicas"],
  "predictions": ["Lista de 2-3 previsões baseadas nas tendências observadas"],
  "concerns": ["Lista de pontos de atenção que requerem monitoramento"]
}

Seja específico, técnico e prático. Use linguagem clara e objetiva.`;

    const response = await fetch("https://ai.gateway.lovable.dev/v1/chat/completions", {
      method: "POST",
      headers: {
        Authorization: `Bearer ${LOVABLE_API_KEY}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        model: "google/gemini-2.5-flash",
        messages: [
          { role: "system", content: systemPrompt },
          { role: "user", content: context }
        ],
        tools: [
          {
            type: "function",
            function: {
              name: "generate_insights",
              description: "Generate cultivation insights",
              parameters: {
                type: "object",
                properties: {
                  summary: { type: "string" },
                  recommendations: { type: "array", items: { type: "string" } },
                  predictions: { type: "array", items: { type: "string" } },
                  concerns: { type: "array", items: { type: "string" } }
                },
                required: ["summary", "recommendations", "predictions", "concerns"],
                additionalProperties: false
              }
            }
          }
        ],
        tool_choice: { type: "function", function: { name: "generate_insights" } }
      }),
    });

    if (!response.ok) {
      const errorText = await response.text();
      console.error("Erro na API de IA:", response.status, errorText);
      throw new Error(`Erro na API: ${response.status}`);
    }

    const data = await response.json();
    const toolCall = data.choices?.[0]?.message?.tool_calls?.[0];
    
    if (!toolCall) {
      throw new Error("Resposta da IA inválida");
    }

    const insights = JSON.parse(toolCall.function.arguments);

    return new Response(
      JSON.stringify({ insights }),
      {
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
        status: 200,
      }
    );
  } catch (error) {
    console.error("Erro ao gerar insights:", error);
    return new Response(
      JSON.stringify({ error: error instanceof Error ? error.message : 'Erro desconhecido' }),
      {
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
        status: 500,
      }
    );
  }
});
