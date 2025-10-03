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
    const supabase = createClient(supabaseUrl, supabaseKey);

    const url = new URL(req.url);
    const startDate = url.searchParams.get('start_date');
    const endDate = url.searchParams.get('end_date');
    const format = url.searchParams.get('format') || 'csv';

    let query = supabase
      .from('readings')
      .select('*')
      .order('timestamp', { ascending: true });

    if (startDate) {
      query = query.gte('timestamp', startDate);
    }
    if (endDate) {
      query = query.lte('timestamp', endDate);
    }

    const { data, error } = await query;

    if (error) {
      console.error('Erro ao buscar leituras:', error);
      throw error;
    }

    if (format === 'pdf') {
      const html = `<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Relatório HydroSmart Crepaldi</title>
  <style>
    @media print {
      body { margin: 0; }
      @page { margin: 10mm; }
    }
    body {
      font-family: Arial, sans-serif;
      margin: 20px;
      background: white;
    }
    .header {
      text-align: center;
      margin-bottom: 30px;
      border-bottom: 3px solid #22c55e;
      padding-bottom: 15px;
    }
    h1 {
      color: #22c55e;
      margin: 0;
      font-size: 28px;
    }
    .subtitle {
      color: #666;
      margin: 10px 0;
      font-size: 14px;
    }
    .info {
      background: #f0fdf4;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      border-left: 4px solid #22c55e;
    }
    .info p {
      margin: 5px 0;
      color: #333;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 20px;
      box-shadow: 0 1px 3px rgba(0,0,0,0.1);
    }
    th {
      background-color: #22c55e;
      color: white;
      padding: 12px 8px;
      text-align: left;
      font-size: 12px;
      font-weight: 600;
    }
    td {
      border: 1px solid #e5e7eb;
      padding: 10px 8px;
      font-size: 11px;
    }
    tr:nth-child(even) {
      background-color: #f9fafb;
    }
    tr:hover {
      background-color: #f0fdf4;
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>🌿 HydroSmart Crepaldi</h1>
    <div class="subtitle">Sistema de Cultivo de Cannabis de Precisão</div>
  </div>
  
  <div class="info">
    <p><strong>📊 Relatório de Leituras dos Sensores</strong></p>
    <p><strong>Período:</strong> ${startDate ? new Date(startDate).toLocaleString('pt-BR') : 'Início'} até ${endDate ? new Date(endDate).toLocaleString('pt-BR') : 'Fim'}</p>
    <p><strong>Total de leituras:</strong> ${data.length} registros</p>
    <p><strong>Gerado em:</strong> ${new Date().toLocaleString('pt-BR')}</p>
  </div>
  
  <table>
    <thead>
      <tr>
        <th>Data/Hora</th>
        <th>pH</th>
        <th>EC (µS/cm)</th>
        <th>Temp. Ar (°C)</th>
        <th>Temp. Água (°C)</th>
        <th>Umidade (%)</th>
      </tr>
    </thead>
    <tbody>
      ${data.map(row => `
        <tr>
          <td>${new Date(row.timestamp).toLocaleString('pt-BR')}</td>
          <td>${row.ph.toFixed(2)}</td>
          <td>${row.ec.toFixed(0)}</td>
          <td>${row.air_temp.toFixed(1)}</td>
          <td>${row.water_temp.toFixed(1)}</td>
          <td>${row.humidity.toFixed(1)}</td>
        </tr>
      `).join('')}
    </tbody>
  </table>
  
  <script>
    // Auto-print quando abrir
    window.onload = function() {
      window.print();
    };
  </script>
</body>
</html>`;

      return new Response(html, {
        headers: {
          ...corsHeaders,
          'Content-Type': 'text/html; charset=utf-8',
        },
      });
    }

    // CSV format (default)
    const headers = ['Timestamp', 'pH', 'EC (µS/cm)', 'Temp. Ar (°C)', 'Umidade (%)', 'Temp. Água (°C)'];
    const csvRows = [headers.join(',')];

    data.forEach(reading => {
      const row = [
        new Date(reading.timestamp).toLocaleString('pt-BR'),
        reading.ph,
        reading.ec,
        reading.air_temp,
        reading.humidity,
        reading.water_temp
      ];
      csvRows.push(row.join(','));
    });

    const csv = csvRows.join('\n');

    console.log(`Exportadas ${data.length} leituras`);

    return new Response(csv, {
      headers: {
        ...corsHeaders,
        'Content-Type': 'text/csv; charset=utf-8',
        'Content-Disposition': `attachment; filename="leituras_aquasys_${new Date().toISOString().split('T')[0]}.csv"`
      }
    });

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
