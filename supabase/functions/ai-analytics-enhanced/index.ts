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

    const { plantId } = await req.json();
    const startTime = Date.now();

    console.log('🚀 Iniciando análise AI Enhanced');

    // 1. Buscar contexto da planta se fornecido
    let cultivationContext = null;
    let speciesProfile = null;
    let stageParameters = null;

    if (plantId) {
      console.log('📋 Buscando contexto da planta:', plantId);
      
      const { data: contextData } = await supabase
        .from('cultivation_contexts')
        .select(`
          *,
          species_profile:species_profile_id (
            species_name,
            common_names,
            description,
            ph_sensitivity,
            nutrient_sensitivity,
            stress_tolerance,
            common_deficiencies,
            common_pests,
            cultivation_notes
          )
        `)
        .eq('plant_id', plantId)
        .single();

      if (contextData) {
        cultivationContext = contextData;
        speciesProfile = contextData.species_profile;

        // Buscar parâmetros da fase atual
        const { data: paramsData } = await supabase
          .from('species_stage_parameters')
          .select('*')
          .eq('species_id', contextData.species_profile_id)
          .eq('growth_stage', contextData.current_stage)
          .single();

        stageParameters = paramsData;
        console.log('✅ Contexto encontrado:', {
          species: speciesProfile?.species_name,
          stage: contextData.current_stage
        });
      }
    }

    // 2. Buscar últimas 50 leituras de sensores
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

    // 3. Calcular estatísticas dos dados
    const stats = {
      ph: calculateStats(readings.map(r => r.ph)),
      ec: calculateStats(readings.map(r => r.ec)),
      airTemp: calculateStats(readings.map(r => r.air_temp)),
      humidity: calculateStats(readings.map(r => r.humidity)),
      waterTemp: calculateStats(readings.map(r => r.water_temp)),
    };

    // 4. Buscar conhecimento relevante usando RAG
    console.log('🔍 Buscando conhecimento relevante...');
    
    const knowledgeQuery = buildKnowledgeQuery(stats, speciesProfile, cultivationContext);
    
    const { data: relevantKnowledge } = await supabase
      .from('knowledge_base')
      .select('title, summary, processed_content')
      .eq('verified', true)
      .limit(5);

    // 5. Construir prompt contextualizado para a IA
    const systemPrompt = buildSystemPrompt(speciesProfile, stageParameters);
    const userPrompt = buildUserPrompt(stats, cultivationContext, stageParameters, relevantKnowledge || []);

    console.log('🤖 Chamando IA com contexto enriquecido...');

    // 6. Chamar Lovable AI com contexto completo
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
    
    console.log('✅ Resposta da IA recebida');

    // 7. Parse da resposta JSON
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

    // 8. Desativar insights antigos
    await supabase
      .from('ai_insights')
      .update({ is_active: false })
      .eq('is_active', true);

    // 9. Salvar novos insights no banco
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

    // 10. Salvar no histórico de análises
    if (plantId && cultivationContext) {
      await supabase
        .from('ai_analysis_history')
        .insert({
          plant_id: plantId,
          cultivation_context_id: cultivationContext.id,
          sensor_data_snapshot: stats,
          context_data: {
            species: speciesProfile,
            stage: cultivationContext.current_stage,
            parameters: stageParameters
          },
          knowledge_sources_used: relevantKnowledge?.map((k: any) => k.id) || [],
          insights_generated: insights,
          recommendations: insights.flatMap((i: any) => i.recommendations || []),
          confidence_score: 0.85,
          processing_time_ms: Date.now() - startTime
        });
    }

    console.log(`✅ ${insights.length} insights salvos com sucesso`);

    return new Response(
      JSON.stringify({ 
        message: 'Análise concluída com sucesso',
        insights_count: insights.length,
        processing_time_ms: Date.now() - startTime,
        context_used: !!speciesProfile
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('❌ Erro:', error);
    return new Response(
      JSON.stringify({ error: error instanceof Error ? error.message : 'Erro desconhecido' }),
      { 
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' }
      }
    );
  }
});

// Funções auxiliares

function calculateStats(values: number[]) {
  return {
    current: values[0],
    avg: values.reduce((sum, v) => sum + v, 0) / values.length,
    min: Math.min(...values),
    max: Math.max(...values),
  };
}

function buildSystemPrompt(speciesProfile: any, stageParameters: any): string {
  let prompt = `Você é um especialista em cultivo hidropônico com conhecimento profundo sobre plantas e agricultura de precisão.`;

  if (speciesProfile) {
    prompt += `\n\nESPÉCIE ATUAL: ${speciesProfile.species_name}
Nomes comuns: ${speciesProfile.common_names?.join(', ') || 'N/A'}
Descrição: ${speciesProfile.description || 'N/A'}

SENSIBILIDADES DA ESPÉCIE:
- pH: ${speciesProfile.ph_sensitivity}
- Nutrientes: ${speciesProfile.nutrient_sensitivity}
- Estresse: ${speciesProfile.stress_tolerance}

PROBLEMAS COMUNS:
- Deficiências: ${speciesProfile.common_deficiencies?.join(', ') || 'N/A'}
- Pragas: ${speciesProfile.common_pests?.join(', ') || 'N/A'}

NOTAS DE CULTIVO:
${speciesProfile.cultivation_notes || 'N/A'}`;
  }

  if (stageParameters) {
    prompt += `\n\nPARÂMETROS IDEAIS PARA A FASE ATUAL:
- pH: ${stageParameters.ph_min} - ${stageParameters.ph_max}
- EC: ${stageParameters.ec_min} - ${stageParameters.ec_max} µS/cm
- Temperatura do ar: ${stageParameters.temp_min}°C - ${stageParameters.temp_max}°C
- Umidade: ${stageParameters.humidity_min}% - ${stageParameters.humidity_max}%
- Temperatura da água: ${stageParameters.water_temp_min}°C - ${stageParameters.water_temp_max}°C
- Luz: ${stageParameters.light_hours}h/${stageParameters.dark_hours}h

Parâmetros críticos nesta fase: ${stageParameters.critical_parameters?.join(', ') || 'N/A'}
Notas da fase: ${stageParameters.phase_notes || 'N/A'}`;
  } else {
    prompt += `\n\nParâmetros ideais gerais de referência:
- pH: 5.5 - 6.5
- EC: 800 - 1400 µS/cm
- Temperatura do ar: 20°C - 28°C
- Umidade: 40% - 70%
- Temperatura da água: 18°C - 22°C`;
  }

  prompt += `\n\nResponda SEMPRE no seguinte formato JSON:
{
  "insights": [
    {
      "type": "anomaly" | "trend" | "recommendation" | "correlation" | "prediction",
      "title": "Título curto e direto",
      "description": "Descrição detalhada do insight considerando as características da espécie",
      "severity": "info" | "warning" | "critical",
      "recommendations": ["Recomendação 1 específica para a espécie", "Recomendação 2"]
    }
  ]
}`;

  return prompt;
}

function buildUserPrompt(stats: any, context: any, params: any, knowledge: any[]): string {
  let prompt = `Analise os seguintes dados do sistema hidropônico:\n\n`;

  prompt += `DADOS ATUAIS DOS SENSORES:
- pH: ${stats.ph.current.toFixed(2)} (média: ${stats.ph.avg.toFixed(2)}, variação: ${stats.ph.min.toFixed(2)} - ${stats.ph.max.toFixed(2)})
- EC: ${stats.ec.current.toFixed(2)} µS/cm (média: ${stats.ec.avg.toFixed(2)}, variação: ${stats.ec.min.toFixed(2)} - ${stats.ec.max.toFixed(2)})
- Temp. Ar: ${stats.airTemp.current.toFixed(1)}°C (média: ${stats.airTemp.avg.toFixed(1)}, variação: ${stats.airTemp.min.toFixed(1)} - ${stats.airTemp.max.toFixed(1)})
- Umidade: ${stats.humidity.current.toFixed(1)}% (média: ${stats.humidity.avg.toFixed(1)}, variação: ${stats.humidity.min.toFixed(1)} - ${stats.humidity.max.toFixed(1)})
- Temp. Água: ${stats.waterTemp.current.toFixed(1)}°C (média: ${stats.waterTemp.avg.toFixed(1)}, variação: ${stats.waterTemp.min.toFixed(1)} - ${stats.waterTemp.max.toFixed(1)})\n\n`;

  if (context) {
    prompt += `CONTEXTO DO CULTIVO:
- Fase atual: ${context.current_stage}
- Dias nesta fase: ${Math.floor((Date.now() - new Date(context.stage_started_at).getTime()) / (1000 * 60 * 60 * 24))}
- Sistema: ${context.system_type || 'N/A'}
- Meio de cultivo: ${context.growing_medium || 'N/A'}
- Objetivos: ${context.cultivation_goals?.join(', ') || 'N/A'}
- Notas do cultivador: ${context.grower_notes || 'N/A'}\n\n`;
  }

  if (knowledge && knowledge.length > 0) {
    prompt += `CONHECIMENTO RELEVANTE DA BASE:\n`;
    knowledge.forEach((k: any, i: number) => {
      prompt += `${i + 1}. ${k.title}: ${k.summary || k.processed_content?.substring(0, 200)}\n`;
    });
    prompt += `\n`;
  }

  prompt += `Com base nestes dados e no conhecimento da espécie, identifique:
1. Anomalias críticas que requerem ação imediata
2. Tendências preocupantes nos parâmetros
3. Correlações entre os diferentes sensores
4. Previsões sobre o desenvolvimento da planta
5. Recomendações ESPECÍFICAS considerando a espécie e fase de crescimento`;

  return prompt;
}

function buildKnowledgeQuery(stats: any, species: any, context: any): string {
  const issues = [];
  
  if (species) {
    issues.push(species.species_name);
  }
  
  if (context?.current_stage) {
    issues.push(context.current_stage);
  }

  return issues.join(' ');
}