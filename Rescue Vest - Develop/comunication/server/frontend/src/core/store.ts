import type { Alert, DeviceState, EventData, Health, NiscEvent, Operator, SafeZone, SessionUser } from './types';
import { isNum } from './util';

const MAX_EVENTS = 300;

export const store = {
  operators: [] as Operator[],
  devices: new Map<string, DeviceState>(),
  events: [] as NiscEvent[],
  safeZones: [] as SafeZone[],
  health: { broker: 'online', bridge: 'online', database: 'online' } as Health,
  connected: true,
  selectedDevice: null as string | null,
  selectedOperator: null as number | null,
  user: null as SessionUser | null,
  // Gilet da inquadrare all'apertura della mappa (es. "Mostra sulla mappa" dalla pagina Operatori).
  mapFocus: null as string | null,
  csrfToken: '',
};

const listeners = new Set<() => void>();
const eventListeners = new Set<(ev: NiscEvent) => void>();
// Ora locale dell'ultimo messaggio ricevuto da ogni gilet: non dipende dall'orologio del server.
const heardAt = new Map<string, number>();

export function subscribe(fn: () => void): () => void {
  listeners.add(fn);
  return () => listeners.delete(fn);
}

export function subscribeEvents(fn: (ev: NiscEvent) => void): () => void {
  eventListeners.add(fn);
  return () => eventListeners.delete(fn);
}

export function notify(): void {
  for (const fn of listeners) {
    try {
      fn();
    } catch (err) {
      console.error('Errore durante l\'aggiornamento della pagina', err);
    }
  }
}

export function setDevices(list: DeviceState[]): void {
  const valid = list.filter((d) => d && typeof d.device_id === 'string');
  store.devices = new Map(valid.map((d) => [d.device_id, d]));
  heardAt.clear();
  for (const d of valid) heardAt.set(d.device_id, Date.parse(d.last_seen) || 0);
}

// I dati arrivano da hardware e rete: un evento malformato viene scartato invece di bloccare la UI.
export function normalizeEvent(ev: NiscEvent): NiscEvent | null {
  if (!ev || typeof ev.device_id !== 'string' || !isNum(ev.sequence) || typeof ev.event_type !== 'string') {
    console.warn('Evento scartato: formato non valido', ev);
    return null;
  }
  if (!ev.data || typeof ev.data !== 'object') ev.data = {};
  if (typeof ev.received_time !== 'string') ev.received_time = new Date().toISOString();
  return ev;
}

export function applyEvent(input: NiscEvent): void {
  const ev = normalizeEvent(input);
  if (!ev) return;

  const duplicate = store.events.some((e) => e.device_id === ev.device_id && e.sequence === ev.sequence);
  if (duplicate) return;

  const dev: DeviceState = store.devices.get(ev.device_id) ?? {
    device_id: ev.device_id,
    operator_id: null,
    firmware: '-',
    last_seen: ev.received_time,
    online: true,
    sos: false,
    battery: null,
  };
  const d: EventData = ev.data;
  dev.last_seen = ev.received_time;
  dev.online = true;
  heardAt.set(dev.device_id, Date.now());
  if (isNum(d.battery)) dev.battery = d.battery;
  if (d.radio && isNum(d.radio.rssi) && isNum(d.radio.snr)) dev.radio = d.radio;
  if (isNum(d.latitude) && isNum(d.longitude)) {
    dev.latitude = d.latitude;
    dev.longitude = d.longitude;
    dev.position_time = ev.event_time || ev.received_time;
  }

  switch (ev.event_type) {
    case 'STATUS':
      dev.online = d.online !== false;
      if (typeof d.firmware === 'string') dev.firmware = d.firmware;
      if (d.sensors_ok && typeof d.sensors_ok === 'object') dev.sensors_ok = d.sensors_ok;
      break;
    case 'TELEMETRY':
      if (isNum(d.spo2)) dev.spo2 = d.spo2;
      if (isNum(d.heart_rate)) dev.heart_rate = d.heart_rate;
      if (isNum(d.mq2_raw)) dev.mq2_raw = d.mq2_raw;
      break;
    case 'SOS':
      dev.sos = true;
      dev.sos_cause = typeof d.cause === 'string' ? d.cause : d.origin === 'auto' ? 'auto' : 'manual_gesture';
      dev.sos_reason = typeof d.reason === 'string' ? d.reason : undefined;
      dev.sos_value = isNum(d.value) ? d.value : undefined;
      break;
    // Falso allarme (guida §9.3): la sirena tace subito, ma la scheda dell'SOS resta finché
    // l'operatore della base non la chiude, così nessuno se ne accorge troppo tardi.
    case 'SOS_CANCEL':
      if (dev.alert) dev.alert = { ...dev.alert, status: 'false_alarm' };
      break;
  }

  store.devices.set(dev.device_id, dev);
  store.events.unshift(ev);
  if (store.events.length > MAX_EVENTS) store.events.length = MAX_EVENTS;

  for (const fn of eventListeners) {
    try {
      fn(ev);
    } catch (err) {
      console.error('Errore nella gestione di un evento', err);
    }
  }
  notify();
}

// Un gilet che non trasmette da più di `timeoutMs` passa offline. Il controllo aggiorna anche
// i tempi relativi mostrati ("12s fa") quando non arrivano eventi.
export function startOfflineWatch(timeoutMs: number): () => void {
  const id = window.setInterval(() => {
    const now = Date.now();
    for (const d of store.devices.values()) {
      const last = heardAt.get(d.device_id) ?? (Date.parse(d.last_seen) || 0);
      if (d.online && now - last > timeoutMs) d.online = false;
    }
    notify();
  }, 5000);
  return () => window.clearInterval(id);
}

// L'allarme è la fonte di verità per l'SOS: finché non viene chiuso, il gilet resta in SOS.
export function setAlert(deviceId: string, alert: Alert | null): void {
  const dev = store.devices.get(deviceId);
  if (!dev) return;
  dev.alert = alert;
  dev.sos = alert !== null;
  notify();
}

export function operatorName(id: number | null): string {
  if (id === null) return 'non assegnato';
  return store.operators.find((o) => o.id === id)?.name ?? 'sconosciuto';
}
