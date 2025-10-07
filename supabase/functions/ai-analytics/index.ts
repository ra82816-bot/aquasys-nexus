import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2.39.3";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

serve(async (req) => {
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    const supabaseUrl = Deno.env.get('SUPABASE_URL')!;
    const supabaseKey = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
    const lovableApiKey = Deno.env.get('LOVABLE_API_KEY')!;
    
    const supabase = createClient(supabaseUrl, supabaseKey);

    // Buscar últimas 50 leituras para análise
    const { data: readings, error: readingsError } = await supabase
      .from('readings')
      .select('*')
      .order('timestamp', { ascending: false })
      .limit(50);

    if (readingsError) throw readingsError;

    if (!readings || readings.length === 0) {
      return new Response(
        JSON.stringify({ message: 'Não há dados suficientes para análise' }),
        { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Calcular estatísticas dos dados
    const stats = {
      ph: {
        current: readings[0].ph,
        avg: readings.reduce((sum, r) => sum + r.ph, 0) / readings.length,
        min: Math.min(...readings.map(r => r.ph)),
        max: Math.max(...readings.map(r => r.ph)),
      },
      ec: {
        current: readings[0].ec,
        avg: readings.reduce((sum, r) => sum + r.ec, 0) / readings.length,
        min: Math.min(...readings.map(r => r.ec)),
        max: Math.max(...readings.map(r => r.ec)),
      },
      airTemp: {
        current: readings[0].air_temp,
        avg: readings.reduce((sum, r) => sum + r.air_temp, 0) / readings.length,
        min: Math.min(...readings.map(r => r.air_temp)),
        max: Math.max(...readings.map(r => r.air_temp)),
      },
      humidity: {
        current: readings[0].humidity,
        avg: readings.reduce((sum, r) => sum + r.humidity, 0) / readings.length,
        min: Math.min(...readings.map(r => r.humidity)),
        max: Math.max(...readings.map(r => r.humidity)),
      },
      waterTemp: {
        current: readings[0].water_temp,
        avg: readings.reduce((sum, r) => sum + r.water_temp, 0) / readings.length,
        min: Math.min(...readings.map(r => r.water_temp)),
        max: Math.max(...readings.map(r => r.water_temp)),
      },
    };

    // Preparar prompt para IA
    const systemPrompt = `Você é um especialista em cultivo hidropônico de cannabis. Analise os dados dos sensores e forneça insights precisos sobre a saúde do cultivo.

Parâmetros ideais de referência:
- pH: 5.5 - 6.5
- EC (Condutividade Elétrica): 800 - 1400 µS/cm
- Temperatura do ar: 20°C - 28°C
- Umidade: 40% - 70%
- Temperatura da água: 18°C - 22°C

Responda SEMPRE no seguinte formato JSON:
{
  "insights": [
    {
      "type": "anomaly" | "trend" | "recommendation" | "correlation",
      "title": "Título curto e direto",
      "description": "Descrição detalhada do insight",
      "severity": "info" | "warning" | "critical",
      "recommendations": ["Recomendação 1", "Recomendação 2"]
    }
  ]
}`;

    const userPrompt = `Analise os seguintes dados do sistema hidropônico:

Dados atuais:
- pH: ${stats.ph.current.toFixed(2)} (média: ${stats.ph.avg.toFixed(2)}, variação: ${stats.ph.min.toFixed(2)} - ${stats.ph.max.toFixed(2)})
- EC: ${stats.ec.current.toFixed(2)} µS/cm (média: ${stats.ec.avg.toFixed(2)}, variação: ${stats.ec.min.toFixed(2)} - ${stats.ec.max.toFixed(2)})
- Temp. Ar: ${stats.airTemp.current.toFixed(1)}°C (média: ${stats.airTemp.avg.toFixed(1)}, variação: ${stats.airTemp.min.toFixed(1)} - ${stats.airTemp.max.toFixed(1)})
- Umidade: ${stats.humidity.current.toFixed(1)}% (média: ${stats.humidity.avg.toFixed(1)}, variação: ${stats.humidity.min.toFixed(1)} - ${stats.humidity.max.toFixed(1)})
- Temp. Água: ${stats.waterTemp.current.toFixed(1)}°C (média: ${stats.waterTemp.avg.toFixed(1)}, variação: ${stats.waterTemp.min.toFixed(1)} - ${stats.waterTemp.max.toFixed(1)})

Identifique:
1. Anomalias críticas que requerem ação imediata
2. Tendências preocupantes nos parâmetros
3. Correlações entre os diferentes sensores
4. Recomendações específicas para otimizar o cultivo`;

    // Chamar Lovable AI
    console.log('Chamando Lovable AI para análise...');
    const aiResponse = await fetch('https://ai.gateway.lovable.dev/v1/chat/completions', {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${lovableApiKey}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        model: 'google/gemini-2.5-flash',
        messages: [
          { role: 'system', content: systemPrompt },
          { role: 'user', content: userPrompt }
        ],
        temperature: 0.7,
      }),
    });

    if (!aiResponse.ok) {
      const errorText = await aiResponse.text();
      console.error('Erro na chamada à IA:', errorText);
      throw new Error(`AI API error: ${aiResponse.status}`);
    }

    const aiData = await aiResponse.json();
    const aiContent = aiData.choices[0].message.content;
    
    console.log('Resposta da IA:', aiContent);

    // Parse da resposta JSON
    let insights;
    try {
      const jsonMatch = aiContent.match(/\{[\s\S]*\}/);
      if (jsonMatch) {
        const parsed = JSON.parse(jsonMatch[0]);
        insights = parsed.insights || [];
      } else {
        throw new Error('Formato de resposta inválido');
      }
    } catch (parseError) {
      console.error('Erro ao fazer parse da resposta:', parseError);
      insights = [{
        type: 'recommendation',
        title: 'Análise em Processamento',
        description: 'Os dados estão sendo analisados. Por favor, tente novamente em alguns instantes.',
        severity: 'info',
        recommendations: ['Aguarde a próxima análise automática']
      }];
    }

    // Desativar insights antigos
    await supabase
      .from('ai_insights')
      .update({ is_active: false })
      .eq('is_active', true);

    // Salvar novos insights no banco
    const insightsToSave = insights.map((insight: any) => ({
      insight_type: insight.type,
      title: insight.title,
      description: insight.description,
      severity: insight.severity,
      recommendations: insight.recommendations || [],
      data_points: stats,
      is_active: true
    }));

    const { error: insertError } = await supabase
      .from('ai_insights')
      .insert(insightsToSave);

    if (insertError) {
      console.error('Erro ao salvar insights:', insertError);
      throw insertError;
    }

    console.log(`${insights.length} insights salvos com sucesso`);

    return new Response(
      JSON.stringify({ 
        message: 'Análise concluída com sucesso',
        insights_count: insights.length 
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Erro:', error);
    return new Response(
      JSON.stringify({ error: error instanceof Error ? error.message : 'Erro desconhecido' }),
      { 
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' }
      }
    );
  }
});
