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
    }
    Views: {
      [_ in never]: never
    }
    Functions: {
      [_ in never]: never
    }
    Enums: {
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
