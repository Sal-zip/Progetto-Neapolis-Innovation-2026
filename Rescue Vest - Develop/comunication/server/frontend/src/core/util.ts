import { THRESHOLDS } from './config';
import type { DeviceState, EventData, EventType } from './types';

const ESC: Record<string, string> = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };

export function esc(value: unknown): string {
  return String(value ?? '').replace(/[&<>"']/g, (c) => ESC[c]);
}

export function isNum(value: unknown): value is number {
  return typeof value === 'number' && Number.isFinite(value);
}

export function coord(value: unknown): string {
  return isNum(value) ? value.toFixed(4) : '-';
}

function parseDate(iso: string): Date | null {
  const d = new Date(iso);
  return Number.isNaN(d.getTime()) ? null : d;
}

export function fmtTime(iso: string): string {
  return parseDate(iso)?.toLocaleTimeString('it-IT', { hour: '2-digit', minute: '2-digit', second: '2-digit' }) ?? '-';
}

export function fmtDateTime(iso: string): string {
  return parseDate(iso)?.toLocaleString('it-IT', { dateStyle: 'short', timeStyle: 'medium' }) ?? '-';
}

export function timeAgo(iso: string): string {
  const d = parseDate(iso);
  if (!d) return '-';
  const s = Math.max(0, Math.round((Date.now() - d.getTime()) / 1000));
  if (s < 60) return `${s}s fa`;
  if (s < 3600) return `${Math.floor(s / 60)} min fa`;
  return `${Math.floor(s / 3600)} h fa`;
}

export function batteryClass(b: number | null): string {
  if (!isNum(b)) return 'text-base-content/40';
  if (b <= 20) return 'text-error';
  if (b <= 40) return 'text-warning';
  return 'text-success';
}

export function renderBattery(b: number | null): string {
  if (!isNum(b)) return '<span class="text-base-content/40 text-xs">Batt. -</span>';
  const color = b <= 20 ? 'bg-error' : 'bg-success';
  return `
    <div class="flex items-center gap-1" title="Batteria ${b}%">
      <div class="w-6 h-3 border border-base-content/30 rounded-[2px] p-[1px] relative flex items-center mr-1">
        <div class="h-full ${color} rounded-[1px] transition-all" style="width: ${b}%"></div>
        <div class="absolute -right-[2px] w-[2px] h-1.5 bg-base-content/30 rounded-r-sm"></div>
      </div>
      <span class="text-[10px] font-bold text-base-content/70">${b}%</span>
    </div>
  `;
}

// ---------- Soglie di allarme (config.ts, guida §9.4) ----------

export type Level = 'unknown' | 'ok' | 'warning' | 'danger';

export function spo2Level(v: unknown): Level {
  if (!isNum(v)) return 'unknown';
  if (v <= THRESHOLDS.spo2.danger) return 'danger';
  if (v <= THRESHOLDS.spo2.warning) return 'warning';
  return 'ok';
}

export function heartRateLevel(v: unknown): Level {
  if (!isNum(v)) return 'unknown';
  const t = THRESHOLDS.heart_rate;
  if (v <= t.dangerLow || v >= t.dangerHigh) return 'danger';
  if (v <= t.warningLow || v >= t.warningHigh) return 'warning';
  return 'ok';
}

export function gasLevel(v: unknown): Level {
  if (!isNum(v)) return 'unknown';
  if (v >= THRESHOLDS.mq2_raw.danger) return 'danger';
  if (v >= THRESHOLDS.mq2_raw.warning) return 'warning';
  return 'ok';
}

export function batteryLevel(v: unknown): Level {
  if (!isNum(v)) return 'unknown';
  if (v <= THRESHOLDS.battery.danger) return 'danger';
  if (v <= THRESHOLDS.battery.warning) return 'warning';
  return 'ok';
}

export function levelClass(level: Level): string {
  if (level === 'danger') return 'text-error';
  if (level === 'warning') return 'text-warning';
  if (level === 'unknown') return 'text-base-content/40';
  return '';
}

// Valori dell'ultimo dato ricevuto che sono fuori soglia, già pronti da mostrare.
export function outOfRange(
  d: Pick<DeviceState, 'spo2' | 'heart_rate' | 'mq2_raw' | 'battery'>,
  includeBattery = true,
): { label: string; level: Level }[] {
  const checks: { label: string; level: Level }[] = [
    { label: `SpO2 ${d.spo2}%`, level: spo2Level(d.spo2) },
    { label: `battito ${d.heart_rate} bpm`, level: heartRateLevel(d.heart_rate) },
    { label: `gas ${d.mq2_raw}`, level: gasLevel(d.mq2_raw) },
  ];
  if (includeBattery) checks.push({ label: `batteria ${d.battery}%`, level: batteryLevel(d.battery) });
  return checks.filter((c) => c.level === 'warning' || c.level === 'danger');
}

const PRIORITY_BADGE: Record<string, { label: string; cls: string }> = {
  critical: { label: 'Critica', cls: 'badge-error' },
  high: { label: 'Alta', cls: 'badge-warning' },
  medium: { label: 'Media', cls: 'badge-ghost' },
  normal: { label: 'Normale', cls: 'badge-ghost' },
};

export function priorityBadge(priority: string): { label: string; cls: string } {
  return Object.hasOwn(PRIORITY_BADGE, priority)
    ? PRIORITY_BADGE[priority]
    : { label: priority || '-', cls: 'badge-ghost' };
}

export function eventSummary(type: EventType, d: EventData): string {
  switch (type) {
    case 'SOS':
      return `${d.origin === 'auto' ? 'automatico' : 'volontario'} — ${sosCauseLabel(d.cause)}${d.reason ? ` (${alarmReasonLabel(d.reason, d.value)})` : ''}${isNum(d.battery) ? `, batteria ${d.battery}%` : ''}`;
    case 'SOS_CANCEL':
      return `falso allarme — ${sosCauseLabel(d.cause)}`;
    case 'TELEMETRY':
      return `SpO2 ${d.spo2 ?? '-'}, HR ${d.heart_rate ?? '-'}, MQ2 ${d.mq2_raw ?? '-'}`;
    case 'POSITION':
      return `${coord(d.latitude)}, ${coord(d.longitude)}`;
    case 'SAFE_ZONE':
      return 'zona sicura proposta';
    case 'STATUS':
      return `batteria ${d.battery ?? '-'}%, ${d.online === false ? 'offline' : 'online'}`;
    default:
      return '';
  }
}

// Causa dell'SOS (campo cause dell'evento, codificato dal firmware).
const SOS_CAUSES: Record<string, string> = {
  manual_gesture: 'gesto SOS',
  accidental_gesture: 'gesto involontario',
  fall_detected: 'caduta rilevata',
  ppg_emergency: 'parametri vitali anomali',
  gas_emergency: 'gas rilevato',
  auto: 'rilevato automaticamente',
};

export function sosCauseLabel(cause: unknown): string {
  if (typeof cause !== 'string' || !cause) return 'causa non indicata';
  return Object.hasOwn(SOS_CAUSES, cause) ? SOS_CAUSES[cause] : cause;
}

// Quale soglia è stata superata (campo reason degli allarmi automatici, guida §9.4).
const ALARM_REASONS: Record<string, string> = {
  spo2_low: 'ossigenazione troppo bassa',
  hr_high: 'battito troppo alto',
  hr_low: 'battito troppo basso',
  gas_high: 'gas oltre la soglia',
  battery_low: 'batteria quasi scarica',
  sensor_fault: 'sensore guasto',
};

export function alarmReasonLabel(reason: unknown, value?: number): string {
  if (typeof reason !== 'string' || !reason) return '';
  const label = Object.hasOwn(ALARM_REASONS, reason) ? ALARM_REASONS[reason] : reason;
  return isNum(value) ? `${label}: ${value}` : label;
}

const SENSOR_NAMES: Record<string, string> = { gps: 'GPS', spo2: 'SpO2/battito', mq2: 'gas', gesture: 'gesti' };

export function sensorName(key: string): string {
  return Object.hasOwn(SENSOR_NAMES, key) ? SENSOR_NAMES[key] : key;
}

// Sensori che il gilet ha dichiarato guasti nell'ultimo STATUS ricevuto.
export function faultySensors(sensors: Record<string, boolean> | undefined): string[] {
  if (!sensors) return [];
  return Object.entries(sensors).filter(([, ok]) => ok === false).map(([k]) => sensorName(k));
}

// Posizione "recente" solo se aggiornata da poco e con il gilet raggiungibile; altrimenti è
// l'ultima conosciuta e va detto chiaramente.
export function positionFreshness(online: boolean, positionTime: string | undefined, freshMs: number): { fresh: boolean; label: string } {
  const t = positionTime ? Date.parse(positionTime) : NaN;
  const age = Number.isFinite(t) ? Date.now() - t : Infinity;
  if (online && age <= freshMs) return { fresh: true, label: `posizione aggiornata ${timeAgo(positionTime!)}` };
  return { fresh: false, label: Number.isFinite(t) ? `ultima posizione nota · ${timeAgo(positionTime!)}` : 'ultima posizione nota' };
}
