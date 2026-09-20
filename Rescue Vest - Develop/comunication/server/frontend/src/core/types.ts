// SOS_CANCEL = l'SOS era un falso allarme (guida §9.3): proposta ancora da concordare con Martina.
export type EventType = 'SOS' | 'SOS_CANCEL' | 'TELEMETRY' | 'POSITION' | 'SAFE_ZONE' | 'STATUS';
export type Priority = 'critical' | 'high' | 'medium' | 'normal';
export type ServiceStatus = 'online' | 'offline' | 'degraded';

export interface RadioInfo {
  rssi: number;
  snr: number;
}

export interface EventData {
  origin?: 'voluntary' | 'auto';
  cause?: string;
  // Allarme automatico su soglia (guida §9.4): che tipo di soglia e quale valore l'ha superata.
  kind?: 'vital' | 'environment' | 'device_fault';
  reason?: string;
  value?: number;
  latitude?: number;
  longitude?: number;
  battery?: number;
  radio?: RadioInfo;
  spo2?: number;
  heart_rate?: number;
  mq2_raw?: number;
  accuracy_m?: number;
  fix?: string;
  triggered?: boolean;
  online?: boolean;
  sensors_ok?: Record<string, boolean>;
  firmware?: string;
}

export interface NiscEvent {
  schema: 'nisc.event.v1';
  device_id: string;
  sequence: number;
  event_type: EventType;
  event_time: string;
  received_time: string;
  priority: Priority;
  data: EventData;
}

export interface Mission {
  date: string;
  title: string;
  state: 'in corso' | 'conclusa';
}

export interface Operator {
  id: number;
  name: string;
  role: string;
  device_id: string | null;
  in_mission: boolean;
  since: string;
  missions: Mission[];
}

export interface Alert {
  id: number;
  // false_alarm = rientrato (SOS_CANCEL dal gilet): la sirena tace ma la scheda resta finché
  // l'operatore della base non chiude l'allarme.
  status: 'open' | 'acknowledged' | 'false_alarm';
  opened_at: string;
  acknowledged_by: string | null;
}

export interface AlertUpdate {
  device_id: string;
  alert: Alert | null;
}

export interface SessionUser {
  id: number;
  name: string;
  role: string;
}

export interface Session {
  user: SessionUser;
  csrf_token: string;
}

export interface DeviceState {
  device_id: string;
  operator_id: number | null;
  firmware: string;
  last_seen: string;
  online: boolean;
  sos: boolean;
  battery: number | null;
  spo2?: number;
  heart_rate?: number;
  mq2_raw?: number;
  latitude?: number;
  longitude?: number;
  radio?: RadioInfo;
  // Allarme non ancora chiuso (aperto o preso in carico); null se non c'è.
  alert?: Alert | null;
  // Ricavati nel browser dagli eventi ricevuti, non fanno parte del contratto.
  position_time?: string;
  sensors_ok?: Record<string, boolean>;
  sos_cause?: string;
  sos_reason?: string;
  sos_value?: number;
}

export interface SafeZone {
  id: number;
  device_id: string;
  latitude: number;
  longitude: number;
  radius_m: number;
  operator_id: number | null;
  status: 'proposed' | 'validated' | 'invalidated';
  created_at: string;
}

export interface Health {
  broker: ServiceStatus;
  bridge: ServiceStatus;
  database: ServiceStatus;
  radio?: ServiceStatus;
}

export interface EventsPage {
  total: number;
  events: NiscEvent[];
}

export interface AiAnswer {
  answer: string;
  sql: string;
  rows: Record<string, unknown>[];
}
