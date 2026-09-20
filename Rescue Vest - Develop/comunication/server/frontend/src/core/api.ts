import { COMMAND_PRESETS, CUSTOM_MESSAGE, config } from './config';
import { applyEvent, normalizeEvent, notify, operatorName, setAlert, setDevices, store } from './store';
import type { AiAnswer, Alert, DeviceState, EventsPage, Health, NiscEvent, Operator, SafeZone, Session } from './types';

let onUnauthorized: () => void = () => {};

export function setUnauthorizedHandler(fn: () => void): void {
  onUnauthorized = fn;
}

async function request<T>(method: 'GET' | 'POST', path: string, body?: unknown): Promise<T> {
  const headers: Record<string, string> = {};
  if (method === 'POST') {
    headers['Content-Type'] = 'application/json';
    if (store.csrfToken) headers['X-CSRF-Token'] = store.csrfToken;
  }
  const res = await fetch(`${config.API_BASE}${path}`, {
    method,
    headers,
    credentials: 'same-origin',
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  // Sessione scaduta durante l'uso: si torna al login (non per i controlli di sessione stessi).
  if (res.status === 401 && path !== '/api/me' && path !== '/api/login') onUnauthorized();
  if (!res.ok) throw new Error(`${path}: HTTP ${res.status}`);
  return res.json() as Promise<T>;
}

const get = <T>(path: string) => request<T>('GET', path);
const post = <T>(path: string, body: unknown) => request<T>('POST', path, body);

function validEvents(list: NiscEvent[]): NiscEvent[] {
  return list.map(normalizeEvent).filter((e): e is NiscEvent => e !== null);
}

// Eventi arrivati dalla socket mentre è in corso il caricamento iniziale: vengono applicati dopo,
// altrimenti la risposta del server li sovrascriverebbe.
let buffered: NiscEvent[] | null = null;
let loading: Promise<void> | null = null;
let loadAgain = false;

// Una riconnessione durante un caricamento non ne avvia un secondo in parallelo (che azzererebbe
// gli eventi in attesa): si ricarica di nuovo appena il primo è finito.
export function loadInitial(): Promise<void> {
  if (loading) {
    loadAgain = true;
    return loading;
  }
  loading = (async () => {
    try {
      do {
        loadAgain = false;
        await loadOnce();
      } while (loadAgain);
    } finally {
      loading = null;
    }
  })();
  return loading;
}

async function loadOnce(): Promise<void> {
  buffered = [];
  try {
    const [devices, operators, zones, events] = await Promise.all([
      get<DeviceState[]>('/api/devices'),
      get<Operator[]>('/api/operators'),
      get<SafeZone[]>('/api/safe-zones'),
      get<EventsPage>('/api/events?limit=100&offset=0'),
    ]);
    setDevices(devices);
    store.operators = operators;
    store.safeZones = zones;
    store.events = validEvents(events.events);
    notify();
  } finally {
    const pending = buffered ?? [];
    buffered = null;
    pending.forEach(applyEvent);
  }
}

export function ingestSocketEvent(ev: NiscEvent): void {
  if (buffered) buffered.push(ev);
  else applyEvent(ev);
}

export async function refreshHealth(): Promise<void> {
  try {
    store.health = await get<Health>('/api/health');
  } catch {
    store.health = { broker: 'offline', bridge: 'offline', database: 'offline' };
  }
  notify();
}

export async function refreshSafeZones(): Promise<void> {
  try {
    store.safeZones = await get<SafeZone[]>('/api/safe-zones');
    notify();
  } catch (err) {
    console.error('Aggiornamento zone sicure fallito', err);
  }
}

export interface EventFilters {
  device_id?: string;
  event_type?: string;
  limit: number;
  offset: number;
}

export async function fetchEvents(f: EventFilters): Promise<EventsPage> {
  if (config.USE_MOCK) {
    const all = store.events.filter(
      (e) => (!f.device_id || e.device_id === f.device_id) && (!f.event_type || e.event_type === f.event_type),
    );
    return { total: all.length, events: all.slice(f.offset, f.offset + f.limit) };
  }
  const q = new URLSearchParams({ limit: String(f.limit), offset: String(f.offset) });
  if (f.device_id) q.set('device_id', f.device_id);
  if (f.event_type) q.set('event_type', f.event_type);
  const page = await get<EventsPage>(`/api/events?${q}`);
  return { total: page.total, events: validEvents(page.events) };
}

export async function fetchDeviceEvents(deviceId: string, limit: number): Promise<NiscEvent[]> {
  if (config.USE_MOCK) return store.events.filter((e) => e.device_id === deviceId).slice(0, limit);
  const page = await get<EventsPage>(`/api/devices/${encodeURIComponent(deviceId)}/events?limit=${limit}&offset=0`);
  return validEvents(page.events);
}

export async function sendCommand(target: string, presetId: string, text = ''): Promise<void> {
  let payload: { code: number; params: Record<string, string> };
  if (presetId === CUSTOM_MESSAGE.id) {
    const clean = text.trim();
    if (!clean) throw new Error('Scrivi il testo del messaggio');
    if (clean.length > CUSTOM_MESSAGE.maxLength) throw new Error(`Massimo ${CUSTOM_MESSAGE.maxLength} caratteri`);
    payload = { code: CUSTOM_MESSAGE.code, params: { text: clean } };
  } else {
    const preset = COMMAND_PRESETS.find((p) => p.id === presetId);
    if (!preset) throw new Error('Messaggio sconosciuto');
    payload = { code: preset.code, params: { text_id: preset.id } };
  }
  if (config.USE_MOCK) return;
  await post(`/api/devices/${encodeURIComponent(target)}/commands`, { type: 'MESSAGE', payload });
}

export async function askAi(question: string): Promise<AiAnswer> {
  if (!config.USE_MOCK) return post<AiAnswer>('/api/ai/query', { question });

  await new Promise((r) => setTimeout(r, 700));
  const q = question.toLowerCase();

  if (q.includes('sos')) {
    const sos = store.events.filter((e) => e.event_type === 'SOS');
    return {
      answer: sos.length
        ? `Risultano ${sos.length} SOS: ${sos.map((e) => `Gilet ${e.device_id} (${operatorName(store.devices.get(e.device_id)?.operator_id ?? null)})`).join(', ')}.`
        : 'Nessun SOS registrato finora.',
      sql: "SELECT device_id, event_time, priority FROM events WHERE event_type = 'SOS' AND event_time::date = CURRENT_DATE;",
      rows: sos.map((e) => ({ device_id: e.device_id, event_time: e.event_time, priority: e.priority })),
    };
  }

  if (q.includes('batteria')) {
    const online = [...store.devices.values()].filter((d) => d.online && d.battery !== null);
    const avg = online.length ? Math.round(online.reduce((s, d) => s + (d.battery ?? 0), 0) / online.length) : 0;
    return {
      answer: `La batteria media dei ${online.length} gilet online è ${avg}%.`,
      sql: 'SELECT AVG(battery) FROM devices WHERE online = true;',
      rows: [{ avg_battery: avg }],
    };
  }

  return {
    answer: 'Nella modalità dimostrativa capisco solo domande su SOS e batteria. Con il backend collegato risponderò a qualsiasi domanda sui dati.',
    sql: '',
    rows: [],
  };
}

// ---------- Sessione dell'operatore della base ----------

export function applySession(session: Session): void {
  store.user = session.user;
  store.csrfToken = session.csrf_token;
  notify();
}

export async function fetchSession(): Promise<Session | null> {
  if (config.USE_MOCK) return null;
  try {
    return await get<Session>('/api/me');
  } catch {
    return null;
  }
}

export async function login(username: string, password: string): Promise<Session> {
  if (!username.trim() || !password) throw new Error('Inserisci nome utente e password.');
  if (config.USE_MOCK) {
    return { user: { id: 0, name: username.trim(), role: 'Coordinatore' }, csrf_token: '' };
  }
  try {
    return await post<Session>('/api/login', { username: username.trim(), password });
  } catch (err) {
    if ((err as Error).message.endsWith('HTTP 401')) throw new Error('Nome utente o password errati.');
    throw new Error('Server non raggiungibile.');
  }
}

export async function logout(): Promise<void> {
  if (!config.USE_MOCK) await post('/api/logout', {}).catch(() => undefined);
}

// ---------- Allarmi: aperto -> preso in carico -> chiuso ----------

function openAlertOf(deviceId: string): Alert {
  const alert = store.devices.get(deviceId)?.alert;
  if (!alert) throw new Error('Allarme non ancora registrato dal server.');
  return alert;
}

export async function acknowledgeAlert(deviceId: string): Promise<void> {
  const alert = openAlertOf(deviceId);
  if (config.USE_MOCK) {
    setAlert(deviceId, { ...alert, status: 'acknowledged', acknowledged_by: store.user?.name ?? null });
    return;
  }
  setAlert(deviceId, await post<Alert>(`/api/alerts/${alert.id}/acknowledge`, {}));
}

export async function closeAlert(deviceId: string): Promise<void> {
  const alert = openAlertOf(deviceId);
  if (!config.USE_MOCK) await post(`/api/alerts/${alert.id}/close`, {});
  setAlert(deviceId, null);
}

// ---------- Zone sicure: la base valida o invalida quelle proposte ----------

export async function setSafeZoneStatus(id: number, status: 'validated' | 'invalidated'): Promise<void> {
  if (config.USE_MOCK) {
    const zone = store.safeZones.find((z) => z.id === id);
    if (zone) zone.status = status;
    notify();
    return;
  }
  const updated = await post<SafeZone>(`/api/safe-zones/${id}`, { status });
  store.safeZones = store.safeZones.map((z) => (z.id === id ? updated : z));
  notify();
}
