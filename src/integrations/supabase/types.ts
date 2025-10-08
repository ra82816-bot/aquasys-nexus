export type Json =
  | string
  | number
  | boolean
  | null
  | { [key: string]: Json | undefined }
  | Json[]

export type Database = {
  // Allows to automatically instantiate createClient with right options
  // instead of createClient<Database, { PostgrestVersion: 'XX' }>(URL, KEY)
  __InternalSupabase: {
    PostgrestVersion: "13.0.5"
  }
  public: {
    Tables: {
      ai_analysis_history: {
        Row: {
          action_taken: boolean | null
          confidence_score: number | null
          context_data: Json | null
          created_at: string | null
          cultivation_context_id: string | null
          feedback_rating: number | null
          id: string
          insights_generated: Json | null
          knowledge_sources_used: string[] | null
          plant_id: string | null
          predictions: Json | null
          processing_time_ms: number | null
          recommendations: Json | null
          sensor_data_snapshot: Json
          user_feedback: string | null
        }
        Insert: {
          action_taken?: boolean | null
          confidence_score?: number | null
          context_data?: Json | null
          created_at?: string | null
          cultivation_context_id?: string | null
          feedback_rating?: number | null
          id?: string
          insights_generated?: Json | null
          knowledge_sources_used?: string[] | null
          plant_id?: string | null
          predictions?: Json | null
          processing_time_ms?: number | null
          recommendations?: Json | null
          sensor_data_snapshot: Json
          user_feedback?: string | null
        }
        Update: {
          action_taken?: boolean | null
          confidence_score?: number | null
          context_data?: Json | null
          created_at?: string | null
          cultivation_context_id?: string | null
          feedback_rating?: number | null
          id?: string
          insights_generated?: Json | null
          knowledge_sources_used?: string[] | null
          plant_id?: string | null
          predictions?: Json | null
          processing_time_ms?: number | null
          recommendations?: Json | null
          sensor_data_snapshot?: Json
          user_feedback?: string | null
        }
        Relationships: [
          {
            foreignKeyName: "ai_analysis_history_cultivation_context_id_fkey"
            columns: ["cultivation_context_id"]
            isOneToOne: false
            referencedRelation: "cultivation_contexts"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "ai_analysis_history_plant_id_fkey"
            columns: ["plant_id"]
            isOneToOne: false
            referencedRelation: "plants"
            referencedColumns: ["id"]
          },
        ]
      }
      ai_insights: {
        Row: {
          created_at: string
          data_points: Json | null
          description: string
          id: string
          insight_type: string
          is_active: boolean
          recommendations: string[] | null
          severity: string
          title: string
        }
        Insert: {
          created_at?: string
          data_points?: Json | null
          description: string
          id?: string
          insight_type: string
          is_active?: boolean
          recommendations?: string[] | null
          severity: string
          title: string
        }
        Update: {
          created_at?: string
          data_points?: Json | null
          description?: string
          id?: string
          insight_type?: string
          is_active?: boolean
          recommendations?: string[] | null
          severity?: string
          title?: string
        }
        Relationships: []
      }
      ai_training_content: {
        Row: {
          approved: boolean | null
          approved_at: string | null
          approved_by: string | null
          categories: string[] | null
          content_type: string
          created_at: string | null
          effectiveness_score: number | null
          extracted_entities: Json | null
          id: string
          processed_content: string | null
          processing_date: string | null
          processing_method: string | null
          raw_content: string | null
          source_type: string | null
          source_url: string | null
          status: Database["public"]["Enums"]["processing_status"] | null
          tags: string[] | null
          updated_at: string | null
          used_in_insights_count: number | null
        }
        Insert: {
          approved?: boolean | null
          approved_at?: string | null
          approved_by?: string | null
          categories?: string[] | null
          content_type: string
          created_at?: string | null
          effectiveness_score?: number | null
          extracted_entities?: Json | null
          id?: string
          processed_content?: string | null
          processing_date?: string | null
          processing_method?: string | null
          raw_content?: string | null
          source_type?: string | null
          source_url?: string | null
          status?: Database["public"]["Enums"]["processing_status"] | null
          tags?: string[] | null
          updated_at?: string | null
          used_in_insights_count?: number | null
        }
        Update: {
          approved?: boolean | null
          approved_at?: string | null
          approved_by?: string | null
          categories?: string[] | null
          content_type?: string
          created_at?: string | null
          effectiveness_score?: number | null
          extracted_entities?: Json | null
          id?: string
          processed_content?: string | null
          processing_date?: string | null
          processing_method?: string | null
          raw_content?: string | null
          source_type?: string | null
          source_url?: string | null
          status?: Database["public"]["Enums"]["processing_status"] | null
          tags?: string[] | null
          updated_at?: string | null
          used_in_insights_count?: number | null
        }
        Relationships: []
      }
      cultivation_contexts: {
        Row: {
          adjustments_made: Json | null
          container_size_liters: number | null
          created_at: string | null
          cultivation_goals: string[] | null
          current_stage: Database["public"]["Enums"]["growth_stage"]
          environmental_conditions: Json | null
          grower_notes: string | null
          growing_medium: string | null
          id: string
          issues_encountered: Json | null
          light_setup: Json | null
          metrics_tracking: Json | null
          nutrient_regime: Json | null
          plant_id: string | null
          species_profile_id: string | null
          stage_history: Json | null
          stage_started_at: string
          system_type: string | null
          target_quality_metrics: Json | null
          target_yield_grams: number | null
          updated_at: string | null
        }
        Insert: {
          adjustments_made?: Json | null
          container_size_liters?: number | null
          created_at?: string | null
          cultivation_goals?: string[] | null
          current_stage: Database["public"]["Enums"]["growth_stage"]
          environmental_conditions?: Json | null
          grower_notes?: string | null
          growing_medium?: string | null
          id?: string
          issues_encountered?: Json | null
          light_setup?: Json | null
          metrics_tracking?: Json | null
          nutrient_regime?: Json | null
          plant_id?: string | null
          species_profile_id?: string | null
          stage_history?: Json | null
          stage_started_at: string
          system_type?: string | null
          target_quality_metrics?: Json | null
          target_yield_grams?: number | null
          updated_at?: string | null
        }
        Update: {
          adjustments_made?: Json | null
          container_size_liters?: number | null
          created_at?: string | null
          cultivation_goals?: string[] | null
          current_stage?: Database["public"]["Enums"]["growth_stage"]
          environmental_conditions?: Json | null
          grower_notes?: string | null
          growing_medium?: string | null
          id?: string
          issues_encountered?: Json | null
          light_setup?: Json | null
          metrics_tracking?: Json | null
          nutrient_regime?: Json | null
          plant_id?: string | null
          species_profile_id?: string | null
          stage_history?: Json | null
          stage_started_at?: string
          system_type?: string | null
          target_quality_metrics?: Json | null
          target_yield_grams?: number | null
          updated_at?: string | null
        }
        Relationships: [
          {
            foreignKeyName: "cultivation_contexts_plant_id_fkey"
            columns: ["plant_id"]
            isOneToOne: true
            referencedRelation: "plants"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "cultivation_contexts_species_profile_id_fkey"
            columns: ["species_profile_id"]
            isOneToOne: false
            referencedRelation: "plant_species_profiles"
            referencedColumns: ["id"]
          },
        ]
      }
      event_logs: {
        Row: {
          id: number
          message: string
          timestamp: string
          type: string
        }
        Insert: {
          id?: number
          message: string
          timestamp?: string
          type: string
        }
        Update: {
          id?: number
          message?: string
          timestamp?: string
          type?: string
        }
        Relationships: []
      }
      forum_comments: {
        Row: {
          content: string
          created_at: string | null
          id: string
          is_anonymous: boolean | null
          parent_comment_id: string | null
          post_id: string
          updated_at: string | null
          user_id: string
        }
        Insert: {
          content: string
          created_at?: string | null
          id?: string
          is_anonymous?: boolean | null
          parent_comment_id?: string | null
          post_id: string
          updated_at?: string | null
          user_id: string
        }
        Update: {
          content?: string
          created_at?: string | null
          id?: string
          is_anonymous?: boolean | null
          parent_comment_id?: string | null
          post_id?: string
          updated_at?: string | null
          user_id?: string
        }
        Relationships: [
          {
            foreignKeyName: "forum_comments_parent_comment_id_fkey"
            columns: ["parent_comment_id"]
            isOneToOne: false
            referencedRelation: "forum_comments"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "forum_comments_post_id_fkey"
            columns: ["post_id"]
            isOneToOne: false
            referencedRelation: "forum_posts"
            referencedColumns: ["id"]
          },
        ]
      }
      forum_likes: {
        Row: {
          comment_id: string | null
          created_at: string | null
          id: string
          post_id: string | null
          user_id: string
        }
        Insert: {
          comment_id?: string | null
          created_at?: string | null
          id?: string
          post_id?: string | null
          user_id: string
        }
        Update: {
          comment_id?: string | null
          created_at?: string | null
          id?: string
          post_id?: string | null
          user_id?: string
        }
        Relationships: [
          {
            foreignKeyName: "forum_likes_comment_id_fkey"
            columns: ["comment_id"]
            isOneToOne: false
            referencedRelation: "forum_comments"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "forum_likes_post_id_fkey"
            columns: ["post_id"]
            isOneToOne: false
            referencedRelation: "forum_posts"
            referencedColumns: ["id"]
          },
        ]
      }
      forum_posts: {
        Row: {
          ai_report: Json | null
          content: string
          created_at: string | null
          id: string
          images: string[] | null
          is_anonymous: boolean | null
          plant_id: string | null
          sensor_data: Json | null
          title: string
          updated_at: string | null
          user_id: string
          views_count: number | null
        }
        Insert: {
          ai_report?: Json | null
          content: string
          created_at?: string | null
          id?: string
          images?: string[] | null
          is_anonymous?: boolean | null
          plant_id?: string | null
          sensor_data?: Json | null
          title: string
          updated_at?: string | null
          user_id: string
          views_count?: number | null
        }
        Update: {
          ai_report?: Json | null
          content?: string
          created_at?: string | null
          id?: string
          images?: string[] | null
          is_anonymous?: boolean | null
          plant_id?: string | null
          sensor_data?: Json | null
          title?: string
          updated_at?: string | null
          user_id?: string
          views_count?: number | null
        }
        Relationships: [
          {
            foreignKeyName: "forum_posts_plant_id_fkey"
            columns: ["plant_id"]
            isOneToOne: false
            referencedRelation: "plants"
            referencedColumns: ["id"]
          },
        ]
      }
      knowledge_base: {
        Row: {
          author: string | null
          content_type: Database["public"]["Enums"]["knowledge_content_type"]
          created_at: string | null
          growth_stages_related:
            | Database["public"]["Enums"]["growth_stage"][]
            | null
          id: string
          language: string | null
          last_used_at: string | null
          metadata: Json | null
          original_content: string | null
          processed_content: string | null
          processing_error: string | null
          processing_status:
            | Database["public"]["Enums"]["processing_status"]
            | null
          publication_date: string | null
          quality_score: number | null
          relevance_score: number | null
          source_url: string | null
          species_related: string[] | null
          summary: string | null
          title: string
          topics: string[] | null
          updated_at: string | null
          usage_count: number | null
          verified: boolean | null
          verified_at: string | null
          verified_by: string | null
          word_count: number | null
        }
        Insert: {
          author?: string | null
          content_type: Database["public"]["Enums"]["knowledge_content_type"]
          created_at?: string | null
          growth_stages_related?:
            | Database["public"]["Enums"]["growth_stage"][]
            | null
          id?: string
          language?: string | null
          last_used_at?: string | null
          metadata?: Json | null
          original_content?: string | null
          processed_content?: string | null
          processing_error?: string | null
          processing_status?:
            | Database["public"]["Enums"]["processing_status"]
            | null
          publication_date?: string | null
          quality_score?: number | null
          relevance_score?: number | null
          source_url?: string | null
          species_related?: string[] | null
          summary?: string | null
          title: string
          topics?: string[] | null
          updated_at?: string | null
          usage_count?: number | null
          verified?: boolean | null
          verified_at?: string | null
          verified_by?: string | null
          word_count?: number | null
        }
        Update: {
          author?: string | null
          content_type?: Database["public"]["Enums"]["knowledge_content_type"]
          created_at?: string | null
          growth_stages_related?:
            | Database["public"]["Enums"]["growth_stage"][]
            | null
          id?: string
          language?: string | null
          last_used_at?: string | null
          metadata?: Json | null
          original_content?: string | null
          processed_content?: string | null
          processing_error?: string | null
          processing_status?:
            | Database["public"]["Enums"]["processing_status"]
            | null
          publication_date?: string | null
          quality_score?: number | null
          relevance_score?: number | null
          source_url?: string | null
          species_related?: string[] | null
          summary?: string | null
          title?: string
          topics?: string[] | null
          updated_at?: string | null
          usage_count?: number | null
          verified?: boolean | null
          verified_at?: string | null
          verified_by?: string | null
          word_count?: number | null
        }
        Relationships: []
      }
      knowledge_embeddings: {
        Row: {
          chunk_index: number
          chunk_text: string
          created_at: string | null
          embedding: string
          end_position: number | null
          id: string
          knowledge_id: string | null
          start_position: number | null
          token_count: number | null
        }
        Insert: {
          chunk_index: number
          chunk_text: string
          created_at?: string | null
          embedding: string
          end_position?: number | null
          id?: string
          knowledge_id?: string | null
          start_position?: number | null
          token_count?: number | null
        }
        Update: {
          chunk_index?: number
          chunk_text?: string
          created_at?: string | null
          embedding?: string
          end_position?: number | null
          id?: string
          knowledge_id?: string | null
          start_position?: number | null
          token_count?: number | null
        }
        Relationships: [
          {
            foreignKeyName: "knowledge_embeddings_knowledge_id_fkey"
            columns: ["knowledge_id"]
            isOneToOne: false
            referencedRelation: "knowledge_base"
            referencedColumns: ["id"]
          },
        ]
      }
      plant_observations: {
        Row: {
          created_at: string | null
          ec_level: number | null
          height_cm: number | null
          humidity: number | null
          id: string
          images: string[] | null
          notes: string | null
          observation_date: string
          ph_level: number | null
          plant_id: string
          temperature: number | null
        }
        Insert: {
          created_at?: string | null
          ec_level?: number | null
          height_cm?: number | null
          humidity?: number | null
          id?: string
          images?: string[] | null
          notes?: string | null
          observation_date: string
          ph_level?: number | null
          plant_id: string
          temperature?: number | null
        }
        Update: {
          created_at?: string | null
          ec_level?: number | null
          height_cm?: number | null
          humidity?: number | null
          id?: string
          images?: string[] | null
          notes?: string | null
          observation_date?: string
          ph_level?: number | null
          plant_id?: string
          temperature?: number | null
        }
        Relationships: [
          {
            foreignKeyName: "plant_observations_plant_id_fkey"
            columns: ["plant_id"]
            isOneToOne: false
            referencedRelation: "plants"
            referencedColumns: ["id"]
          },
        ]
      }
      plant_species_profiles: {
        Row: {
          common_deficiencies: string[] | null
          common_diseases: string[] | null
          common_names: string[] | null
          common_pests: string[] | null
          created_at: string | null
          cultivation_notes: string | null
          default_ec_max: number | null
          default_ec_min: number | null
          default_humidity_max: number | null
          default_humidity_min: number | null
          default_ph_max: number | null
          default_ph_min: number | null
          default_temp_max: number | null
          default_temp_min: number | null
          default_water_temp_max: number | null
          default_water_temp_min: number | null
          description: string | null
          difficulty_level: string | null
          growth_cycle_days: number | null
          harvest_indicators: string[] | null
          id: string
          micronutrients: Json | null
          nitrogen_requirements: Json | null
          nutrient_sensitivity: string | null
          ph_sensitivity: string | null
          phosphorus_requirements: Json | null
          potassium_requirements: Json | null
          scientific_name: string | null
          source_references: string[] | null
          species_name: string
          stress_tolerance: string | null
          updated_at: string | null
          verification_date: string | null
          verified: boolean | null
        }
        Insert: {
          common_deficiencies?: string[] | null
          common_diseases?: string[] | null
          common_names?: string[] | null
          common_pests?: string[] | null
          created_at?: string | null
          cultivation_notes?: string | null
          default_ec_max?: number | null
          default_ec_min?: number | null
          default_humidity_max?: number | null
          default_humidity_min?: number | null
          default_ph_max?: number | null
          default_ph_min?: number | null
          default_temp_max?: number | null
          default_temp_min?: number | null
          default_water_temp_max?: number | null
          default_water_temp_min?: number | null
          description?: string | null
          difficulty_level?: string | null
          growth_cycle_days?: number | null
          harvest_indicators?: string[] | null
          id?: string
          micronutrients?: Json | null
          nitrogen_requirements?: Json | null
          nutrient_sensitivity?: string | null
          ph_sensitivity?: string | null
          phosphorus_requirements?: Json | null
          potassium_requirements?: Json | null
          scientific_name?: string | null
          source_references?: string[] | null
          species_name: string
          stress_tolerance?: string | null
          updated_at?: string | null
          verification_date?: string | null
          verified?: boolean | null
        }
        Update: {
          common_deficiencies?: string[] | null
          common_diseases?: string[] | null
          common_names?: string[] | null
          common_pests?: string[] | null
          created_at?: string | null
          cultivation_notes?: string | null
          default_ec_max?: number | null
          default_ec_min?: number | null
          default_humidity_max?: number | null
          default_humidity_min?: number | null
          default_ph_max?: number | null
          default_ph_min?: number | null
          default_temp_max?: number | null
          default_temp_min?: number | null
          default_water_temp_max?: number | null
          default_water_temp_min?: number | null
          description?: string | null
          difficulty_level?: string | null
          growth_cycle_days?: number | null
          harvest_indicators?: string[] | null
          id?: string
          micronutrients?: Json | null
          nitrogen_requirements?: Json | null
          nutrient_sensitivity?: string | null
          ph_sensitivity?: string | null
          phosphorus_requirements?: Json | null
          potassium_requirements?: Json | null
          scientific_name?: string | null
          source_references?: string[] | null
          species_name?: string
          stress_tolerance?: string | null
          updated_at?: string | null
          verification_date?: string | null
          verified?: boolean | null
        }
        Relationships: []
      }
      plants: {
        Row: {
          adjustments_made: string | null
          avg_humidity: number | null
          avg_ph: number | null
          avg_temperature: number | null
          created_at: string | null
          current_growth_stage:
            | Database["public"]["Enums"]["growth_stage"]
            | null
          general_notes: string | null
          genetics: string | null
          germination_date: string | null
          harvest_date: string | null
          id: string
          light_cycle: string | null
          nickname: string
          nutrients_frequency: string | null
          nutrients_type: string | null
          origin: Database["public"]["Enums"]["plant_origin"]
          pests_diseases: string | null
          quality_aroma: number | null
          quality_color: number | null
          quality_density: number | null
          quality_resin: number | null
          registration_number: string | null
          sex: Database["public"]["Enums"]["plant_sex"] | null
          species: string | null
          species_profile_id: string | null
          stage_started_at: string | null
          status: Database["public"]["Enums"]["plant_status"] | null
          stress_events: string | null
          substrate_type: string | null
          transplant_date: string | null
          updated_at: string | null
          user_id: string
          yield_grams: number | null
        }
        Insert: {
          adjustments_made?: string | null
          avg_humidity?: number | null
          avg_ph?: number | null
          avg_temperature?: number | null
          created_at?: string | null
          current_growth_stage?:
            | Database["public"]["Enums"]["growth_stage"]
            | null
          general_notes?: string | null
          genetics?: string | null
          germination_date?: string | null
          harvest_date?: string | null
          id?: string
          light_cycle?: string | null
          nickname: string
          nutrients_frequency?: string | null
          nutrients_type?: string | null
          origin?: Database["public"]["Enums"]["plant_origin"]
          pests_diseases?: string | null
          quality_aroma?: number | null
          quality_color?: number | null
          quality_density?: number | null
          quality_resin?: number | null
          registration_number?: string | null
          sex?: Database["public"]["Enums"]["plant_sex"] | null
          species?: string | null
          species_profile_id?: string | null
          stage_started_at?: string | null
          status?: Database["public"]["Enums"]["plant_status"] | null
          stress_events?: string | null
          substrate_type?: string | null
          transplant_date?: string | null
          updated_at?: string | null
          user_id: string
          yield_grams?: number | null
        }
        Update: {
          adjustments_made?: string | null
          avg_humidity?: number | null
          avg_ph?: number | null
          avg_temperature?: number | null
          created_at?: string | null
          current_growth_stage?:
            | Database["public"]["Enums"]["growth_stage"]
            | null
          general_notes?: string | null
          genetics?: string | null
          germination_date?: string | null
          harvest_date?: string | null
          id?: string
          light_cycle?: string | null
          nickname?: string
          nutrients_frequency?: string | null
          nutrients_type?: string | null
          origin?: Database["public"]["Enums"]["plant_origin"]
          pests_diseases?: string | null
          quality_aroma?: number | null
          quality_color?: number | null
          quality_density?: number | null
          quality_resin?: number | null
          registration_number?: string | null
          sex?: Database["public"]["Enums"]["plant_sex"] | null
          species?: string | null
          species_profile_id?: string | null
          stage_started_at?: string | null
          status?: Database["public"]["Enums"]["plant_status"] | null
          stress_events?: string | null
          substrate_type?: string | null
          transplant_date?: string | null
          updated_at?: string | null
          user_id?: string
          yield_grams?: number | null
        }
        Relationships: [
          {
            foreignKeyName: "plants_species_profile_id_fkey"
            columns: ["species_profile_id"]
            isOneToOne: false
            referencedRelation: "plant_species_profiles"
            referencedColumns: ["id"]
          },
        ]
      }
      profiles: {
        Row: {
          created_at: string
          email: string
          id: string
          updated_at: string
        }
        Insert: {
          created_at?: string
          email: string
          id: string
          updated_at?: string
        }
        Update: {
          created_at?: string
          email?: string
          id?: string
          updated_at?: string
        }
        Relationships: []
      }
      readings: {
        Row: {
          air_temp: number
          ec: number
          humidity: number
          id: number
          ph: number
          timestamp: string
          water_temp: number
        }
        Insert: {
          air_temp: number
          ec: number
          humidity: number
          id?: number
          ph: number
          timestamp?: string
          water_temp: number
        }
        Update: {
          air_temp?: number
          ec?: number
          humidity?: number
          id?: number
          ph?: number
          timestamp?: string
          water_temp?: number
        }
        Relationships: []
      }
      relay_commands: {
        Row: {
          command: boolean
          executed: boolean
          id: number
          relay_index: number
          timestamp: string
        }
        Insert: {
          command: boolean
          executed?: boolean
          id?: number
          relay_index: number
          timestamp?: string
        }
        Update: {
          command?: boolean
          executed?: boolean
          id?: number
          relay_index?: number
          timestamp?: string
        }
        Relationships: []
      }
      relay_configs: {
        Row: {
          cycle_off_min: number | null
          cycle_on_min: number | null
          ec_pulse_sec: number | null
          ec_threshold: number | null
          humidity_threshold_off: number | null
          humidity_threshold_on: number | null
          led_off_hour: number | null
          led_on_hour: number | null
          mode: Database["public"]["Enums"]["relay_mode"]
          name: string | null
          ph_pulse_sec: number | null
          ph_threshold_high: number | null
          ph_threshold_low: number | null
          relay_index: number
          temp_threshold_off: number | null
          temp_threshold_on: number | null
          updated_at: string
        }
        Insert: {
          cycle_off_min?: number | null
          cycle_on_min?: number | null
          ec_pulse_sec?: number | null
          ec_threshold?: number | null
          humidity_threshold_off?: number | null
          humidity_threshold_on?: number | null
          led_off_hour?: number | null
          led_on_hour?: number | null
          mode?: Database["public"]["Enums"]["relay_mode"]
          name?: string | null
          ph_pulse_sec?: number | null
          ph_threshold_high?: number | null
          ph_threshold_low?: number | null
          relay_index: number
          temp_threshold_off?: number | null
          temp_threshold_on?: number | null
          updated_at?: string
        }
        Update: {
          cycle_off_min?: number | null
          cycle_on_min?: number | null
          ec_pulse_sec?: number | null
          ec_threshold?: number | null
          humidity_threshold_off?: number | null
          humidity_threshold_on?: number | null
          led_off_hour?: number | null
          led_on_hour?: number | null
          mode?: Database["public"]["Enums"]["relay_mode"]
          name?: string | null
          ph_pulse_sec?: number | null
          ph_threshold_high?: number | null
          ph_threshold_low?: number | null
          relay_index?: number
          temp_threshold_off?: number | null
          temp_threshold_on?: number | null
          updated_at?: string
        }
        Relationships: []
      }
      relay_status: {
        Row: {
          id: number
          relay1_led: boolean
          relay2_pump: boolean
          relay3_ph_up: boolean
          relay4_fan: boolean
          relay5_humidity: boolean
          relay6_ec: boolean
          relay7_co2: boolean
          relay8_generic: boolean
          timestamp: string
        }
        Insert: {
          id?: number
          relay1_led: boolean
          relay2_pump: boolean
          relay3_ph_up: boolean
          relay4_fan: boolean
          relay5_humidity: boolean
          relay6_ec: boolean
          relay7_co2: boolean
          relay8_generic: boolean
          timestamp?: string
        }
        Update: {
          id?: number
          relay1_led?: boolean
          relay2_pump?: boolean
          relay3_ph_up?: boolean
          relay4_fan?: boolean
          relay5_humidity?: boolean
          relay6_ec?: boolean
          relay7_co2?: boolean
          relay8_generic?: boolean
          timestamp?: string
        }
        Relationships: []
      }
      shared_cultivation_data: {
        Row: {
          created_at: string | null
          environmental_data: Json | null
          final_yield: number | null
          genetics: string | null
          growth_data: Json | null
          id: string
          issues_encountered: string[] | null
          nutrients_data: Json | null
          plant_id: string | null
          quality_metrics: Json | null
          substrate_type: string | null
          successful_adjustments: string[] | null
        }
        Insert: {
          created_at?: string | null
          environmental_data?: Json | null
          final_yield?: number | null
          genetics?: string | null
          growth_data?: Json | null
          id?: string
          issues_encountered?: string[] | null
          nutrients_data?: Json | null
          plant_id?: string | null
          quality_metrics?: Json | null
          substrate_type?: string | null
          successful_adjustments?: string[] | null
        }
        Update: {
          created_at?: string | null
          environmental_data?: Json | null
          final_yield?: number | null
          genetics?: string | null
          growth_data?: Json | null
          id?: string
          issues_encountered?: string[] | null
          nutrients_data?: Json | null
          plant_id?: string | null
          quality_metrics?: Json | null
          substrate_type?: string | null
          successful_adjustments?: string[] | null
        }
        Relationships: [
          {
            foreignKeyName: "shared_cultivation_data_plant_id_fkey"
            columns: ["plant_id"]
            isOneToOne: false
            referencedRelation: "plants"
            referencedColumns: ["id"]
          },
        ]
      }
      species_stage_parameters: {
        Row: {
          critical_parameters: string[] | null
          dark_hours: number | null
          duration_days_max: number | null
          duration_days_min: number | null
          ec_max: number
          ec_min: number
          feeding_schedule: Json | null
          growth_stage: Database["public"]["Enums"]["growth_stage"]
          humidity_max: number
          humidity_min: number
          id: string
          light_hours: number | null
          light_intensity_max: number | null
          light_intensity_min: number | null
          maintenance_tasks: string[] | null
          ph_max: number
          ph_min: number
          phase_notes: string | null
          species_id: string | null
          temp_max: number
          temp_min: number
          water_temp_max: number
          water_temp_min: number
        }
        Insert: {
          critical_parameters?: string[] | null
          dark_hours?: number | null
          duration_days_max?: number | null
          duration_days_min?: number | null
          ec_max: number
          ec_min: number
          feeding_schedule?: Json | null
          growth_stage: Database["public"]["Enums"]["growth_stage"]
          humidity_max: number
          humidity_min: number
          id?: string
          light_hours?: number | null
          light_intensity_max?: number | null
          light_intensity_min?: number | null
          maintenance_tasks?: string[] | null
          ph_max: number
          ph_min: number
          phase_notes?: string | null
          species_id?: string | null
          temp_max: number
          temp_min: number
          water_temp_max: number
          water_temp_min: number
        }
        Update: {
          critical_parameters?: string[] | null
          dark_hours?: number | null
          duration_days_max?: number | null
          duration_days_min?: number | null
          ec_max?: number
          ec_min?: number
          feeding_schedule?: Json | null
          growth_stage?: Database["public"]["Enums"]["growth_stage"]
          humidity_max?: number
          humidity_min?: number
          id?: string
          light_hours?: number | null
          light_intensity_max?: number | null
          light_intensity_min?: number | null
          maintenance_tasks?: string[] | null
          ph_max?: number
          ph_min?: number
          phase_notes?: string | null
          species_id?: string | null
          temp_max?: number
          temp_min?: number
          water_temp_max?: number
          water_temp_min?: number
        }
        Relationships: [
          {
            foreignKeyName: "species_stage_parameters_species_id_fkey"
            columns: ["species_id"]
            isOneToOne: false
            referencedRelation: "plant_species_profiles"
            referencedColumns: ["id"]
          },
        ]
      }
    }
    Views: {
      [_ in never]: never
    }
    Functions: {
      binary_quantize: {
        Args: { "": string } | { "": unknown }
        Returns: unknown
      }
      get_ideal_parameters: {
        Args: {
          p_growth_stage: Database["public"]["Enums"]["growth_stage"]
          p_species_id: string
        }
        Returns: {
          ec_max: number
          ec_min: number
          humidity_max: number
          humidity_min: number
          ph_max: number
          ph_min: number
          temp_max: number
          temp_min: number
          water_temp_max: number
          water_temp_min: number
        }[]
      }
      halfvec_avg: {
        Args: { "": number[] }
        Returns: unknown
      }
      halfvec_out: {
        Args: { "": unknown }
        Returns: unknown
      }
      halfvec_send: {
        Args: { "": unknown }
        Returns: string
      }
      halfvec_typmod_in: {
        Args: { "": unknown[] }
        Returns: number
      }
      hnsw_bit_support: {
        Args: { "": unknown }
        Returns: unknown
      }
      hnsw_halfvec_support: {
        Args: { "": unknown }
        Returns: unknown
      }
      hnsw_sparsevec_support: {
        Args: { "": unknown }
        Returns: unknown
      }
      hnswhandler: {
        Args: { "": unknown }
        Returns: unknown
      }
      ivfflat_bit_support: {
        Args: { "": unknown }
        Returns: unknown
      }
      ivfflat_halfvec_support: {
        Args: { "": unknown }
        Returns: unknown
      }
      ivfflathandler: {
        Args: { "": unknown }
        Returns: unknown
      }
      l2_norm: {
        Args: { "": unknown } | { "": unknown }
        Returns: number
      }
      l2_normalize: {
        Args: { "": string } | { "": unknown } | { "": unknown }
        Returns: string
      }
      search_knowledge_by_vector: {
        Args: {
          match_count?: number
          match_threshold?: number
          query_embedding: string
        }
        Returns: {
          chunk_text: string
          id: string
          knowledge_id: string
          similarity: number
        }[]
      }
      sparsevec_out: {
        Args: { "": unknown }
        Returns: unknown
      }
      sparsevec_send: {
        Args: { "": unknown }
        Returns: string
      }
      sparsevec_typmod_in: {
        Args: { "": unknown[] }
        Returns: number
      }
      vector_avg: {
        Args: { "": number[] }
        Returns: string
      }
      vector_dims: {
        Args: { "": string } | { "": unknown }
        Returns: number
      }
      vector_norm: {
        Args: { "": string }
        Returns: number
      }
      vector_out: {
        Args: { "": string }
        Returns: unknown
      }
      vector_send: {
        Args: { "": string }
        Returns: string
      }
      vector_typmod_in: {
        Args: { "": unknown[] }
        Returns: number
      }
    }
    Enums: {
      growth_stage:
        | "germination"
        | "seedling"
        | "vegetative_early"
        | "vegetative_late"
        | "pre_flowering"
        | "flowering_early"
        | "flowering_mid"
        | "flowering_late"
        | "ripening"
        | "harvest"
      knowledge_content_type:
        | "article"
        | "scientific_paper"
        | "forum_post"
        | "video_transcript"
        | "pdf_document"
        | "manual"
        | "guide"
        | "case_study"
        | "research"
        | "user_experience"
      plant_origin: "seed" | "clone"
      plant_sex: "unknown" | "female" | "male" | "hermaphrodite"
      plant_status:
        | "germinating"
        | "vegetative"
        | "flowering"
        | "harvested"
        | "discontinued"
      processing_status:
        | "pending"
        | "processing"
        | "completed"
        | "failed"
        | "archived"
      relay_mode:
        | "unused"
        | "led"
        | "cycle"
        | "ph_up"
        | "ph_down"
        | "temperature"
        | "humidity"
        | "ec"
    }
    CompositeTypes: {
      [_ in never]: never
    }
  }
}

type DatabaseWithoutInternals = Omit<Database, "__InternalSupabase">

type DefaultSchema = DatabaseWithoutInternals[Extract<keyof Database, "public">]

export type Tables<
  DefaultSchemaTableNameOrOptions extends
    | keyof (DefaultSchema["Tables"] & DefaultSchema["Views"])
    | { schema: keyof DatabaseWithoutInternals },
  TableName extends DefaultSchemaTableNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof (DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"] &
        DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Views"])
    : never = never,
> = DefaultSchemaTableNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? (DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"] &
      DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Views"])[TableName] extends {
      Row: infer R
    }
    ? R
    : never
  : DefaultSchemaTableNameOrOptions extends keyof (DefaultSchema["Tables"] &
        DefaultSchema["Views"])
    ? (DefaultSchema["Tables"] &
        DefaultSchema["Views"])[DefaultSchemaTableNameOrOptions] extends {
        Row: infer R
      }
      ? R
      : never
    : never

export type TablesInsert<
  DefaultSchemaTableNameOrOptions extends
    | keyof DefaultSchema["Tables"]
    | { schema: keyof DatabaseWithoutInternals },
  TableName extends DefaultSchemaTableNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"]
    : never = never,
> = DefaultSchemaTableNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"][TableName] extends {
      Insert: infer I
    }
    ? I
    : never
  : DefaultSchemaTableNameOrOptions extends keyof DefaultSchema["Tables"]
    ? DefaultSchema["Tables"][DefaultSchemaTableNameOrOptions] extends {
        Insert: infer I
      }
      ? I
      : never
    : never

export type TablesUpdate<
  DefaultSchemaTableNameOrOptions extends
    | keyof DefaultSchema["Tables"]
    | { schema: keyof DatabaseWithoutInternals },
  TableName extends DefaultSchemaTableNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"]
    : never = never,
> = DefaultSchemaTableNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"][TableName] extends {
      Update: infer U
    }
    ? U
    : never
  : DefaultSchemaTableNameOrOptions extends keyof DefaultSchema["Tables"]
    ? DefaultSchema["Tables"][DefaultSchemaTableNameOrOptions] extends {
        Update: infer U
      }
      ? U
      : never
    : never

export type Enums<
  DefaultSchemaEnumNameOrOptions extends
    | keyof DefaultSchema["Enums"]
    | { schema: keyof DatabaseWithoutInternals },
  EnumName extends DefaultSchemaEnumNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[DefaultSchemaEnumNameOrOptions["schema"]]["Enums"]
    : never = never,
> = DefaultSchemaEnumNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[DefaultSchemaEnumNameOrOptions["schema"]]["Enums"][EnumName]
  : DefaultSchemaEnumNameOrOptions extends keyof DefaultSchema["Enums"]
    ? DefaultSchema["Enums"][DefaultSchemaEnumNameOrOptions]
    : never

export type CompositeTypes<
  PublicCompositeTypeNameOrOptions extends
    | keyof DefaultSchema["CompositeTypes"]
    | { schema: keyof DatabaseWithoutInternals },
  CompositeTypeName extends PublicCompositeTypeNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[PublicCompositeTypeNameOrOptions["schema"]]["CompositeTypes"]
    : never = never,
> = PublicCompositeTypeNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[PublicCompositeTypeNameOrOptions["schema"]]["CompositeTypes"][CompositeTypeName]
  : PublicCompositeTypeNameOrOptions extends keyof DefaultSchema["CompositeTypes"]
    ? DefaultSchema["CompositeTypes"][PublicCompositeTypeNameOrOptions]
    : never

export const Constants = {
  public: {
    Enums: {
      growth_stage: [
        "germination",
        "seedling",
        "vegetative_early",
        "vegetative_late",
        "pre_flowering",
        "flowering_early",
        "flowering_mid",
        "flowering_late",
        "ripening",
        "harvest",
      ],
      knowledge_content_type: [
        "article",
        "scientific_paper",
        "forum_post",
        "video_transcript",
        "pdf_document",
        "manual",
        "guide",
        "case_study",
        "research",
        "user_experience",
      ],
      plant_origin: ["seed", "clone"],
      plant_sex: ["unknown", "female", "male", "hermaphrodite"],
      plant_status: [
        "germinating",
        "vegetative",
        "flowering",
        "harvested",
        "discontinued",
      ],
      processing_status: [
        "pending",
        "processing",
        "completed",
        "failed",
        "archived",
      ],
      relay_mode: [
        "unused",
        "led",
        "cycle",
        "ph_up",
        "ph_down",
        "temperature",
        "humidity",
        "ec",
      ],
    },
  },
} as const
