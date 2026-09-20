import { THRESHOLDS } from './config';
import { applyEvent, notify, setAlert, setDevices, store } from './store';
import type { EventData, EventType, NiscEvent, Operator, Priority } from './types';
import { gasLevel, heartRateLevel, spo2Level } from './util';

// Simulazione per la demo: ogni soccorritore segue un percorso di ricerca a passo d'uomo e
// il gilet trasmette come farebbe quello vero (posizione ogni 3 s, parametri ogni 6 s,
// STATUS ogni 12 s). Sopra c'è uno scenario a tempo con SOS, zona sicura, gas e perdita di segnale.

const BASE_LAT = 40.8518;
const BASE_LNG = 14.2681;
const M_PER_DEG_LAT = 111_320;
const M_PER_DEG_LNG = 111_320 * Math.cos((BASE_LAT * Math.PI) / 180);

type Point = [east: number, north: number];

const OPERATORS: Operator[] = [
  {
    id: 1, name: 'Mario Rossi', role: 'Soccorritore', device_id: '01', in_mission: true, since: 'Gen 2025',
    missions: [
      { date: '17/09/2026', title: 'Ricerca Zona Est', state: 'in corso' },
      { date: '02/09/2026', title: 'Esercitazione notturna', state: 'conclusa' },
      { date: '20/08/2026', title: 'Intervento sentiero CAI 4', state: 'conclusa' },
    ],
  },
  {
    id: 2, name: 'Luca Bianchi', role: 'Soccorritore', device_id: '02', in_mission: true, since: 'Mar 2025',
    missions: [{ date: '17/09/2026', title: 'Ricerca Zona Est', state: 'in corso' }],
  },
  {
    id: 3, name: 'Sara Verdi', role: 'Caposquadra', device_id: '03', in_mission: true, since: 'Nov 2023',
    missions: [
      { date: '17/09/2026', title: 'Ricerca Zona Est', state: 'in corso' },
      { date: '02/09/2026', title: 'Esercitazione notturna', state: 'conclusa' },
    ],
  },
  {
    id: 4, name: 'Elena Neri', role: 'Soccorritrice', device_id: '05', in_mission: true, since: 'Giu 2024',
    missions: [{ date: '17/09/2026', title: 'Ricerca Zona Est', state: 'in corso' }],
  },
  {
    id: 5, name: 'Paolo Colombo', role: 'Coordinatore base', device_id: null, in_mission: false, since: 'Set 2022',
    missions: [],
  },
];

interface WalkerSeed {
  id: string;
  operator: number;
  battery: number;
  speed: number; // m/s
  route: Point[]; // metri rispetto alla base, percorso chiuso
  sensors: Record<string, boolean>;
}

const WALKERS: WalkerSeed[] = [
  // Pattuglia a rettangolo a nord-est.
  { id: '01', operator: 1, battery: 62, speed: 1.5, route: [[0, 0], [120, 0], [120, 80], [0, 80]], sensors: { gps: true, spo2: true, mq2: true, gesture: true } },
  // Avanti e indietro lungo due strade a sud-est. Sensore dei gesti guasto (requisito: codice di errore).
  { id: '02', operator: 2, battery: 82, speed: 1.3, route: [[200, -190], [330, -190], [330, -110], [200, -110]], sensors: { gps: true, spo2: true, mq2: true, gesture: false } },
  // Ricerca "a pettine" a sud-ovest: attraversa la zona con il gas.
  { id: '03', operator: 3, battery: 64, speed: 1.4, route: [[-177, -290], [-177, -200], [-137, -200], [-137, -290], [-97, -290], [-97, -200]], sensors: { gps: true, spo2: true, mq2: true, gesture: true } },
  // Giro ad anello a ovest: propone la zona sicura.
  { id: '05', operator: 4, battery: 91, speed: 1.6, route: [[-261, 134], [-190, 180], [-120, 134], [-190, 90]], sensors: { gps: true, spo2: true, mq2: true, gesture: true } },
];

// Gilet 04: non assegnato e spento da 12 minuti.
const OFFLINE_SEED = { id: '04', east: -120, north: 100, battery: 47 };

const GAS_CENTER: Point = [-177, -245];
const GAS_RADIUS_M = 30;

const SCENARIO = {
  sosManual: 8, // 01 fa il gesto SOS e si ferma
  safeZone: 20, // 05 propone una zona sicura
  falseAlarmFrom: 30, // 02 fa partire un SOS per sbaglio...
  falseAlarmTo: 42, // ...e lo annulla (SOS_CANCEL)
  desaturation: 70, // 05 inizia a perdere ossigenazione: allarme automatico su soglia
  signalLostFrom: 60, // 02 esce dalla copertura...
  signalLostTo: 110, // ...e rientra
};

function toLatLng([east, north]: Point): [number, number] {
  return [BASE_LAT + north / M_PER_DEG_LAT, BASE_LNG + east / M_PER_DEG_LNG];
}

function dist(a: Point, b: Point): number {
  return Math.hypot(b[0] - a[0], b[1] - a[1]);
}

function jitter(range: number): number {
  return (Math.random() - 0.5) * range;
}

function clamp(v: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, v));
}

const sequences = new Map<string, number>();

function nextSeq(id: string): number {
  const n = (sequences.get(id) ?? 1200) + 1;
  sequences.set(id, n);
  return n;
}

function makeEvent(deviceId: string, type: EventType, priority: Priority, data: EventData, ageSec = 0): NiscEvent {
  const t = new Date(Date.now() - ageSec * 1000).toISOString();
  return {
    schema: 'nisc.event.v1',
    device_id: deviceId,
    sequence: nextSeq(deviceId),
    event_type: type,
    event_time: t,
    received_time: t,
    priority,
    data,
  };
}

class Walker {
  leg = 0;
  progress = 0;
  paused = false;
  hr = 78;
  spo2 = 97;
  spo2Target = 97; // scende nello scenario della desaturazione
  mq2 = 1800;
  battery: number;

  constructor(readonly seed: WalkerSeed) {
    this.battery = seed.battery;
  }

  position(): Point {
    const r = this.seed.route;
    const a = r[this.leg];
    const b = r[(this.leg + 1) % r.length];
    const len = dist(a, b) || 1;
    const k = this.progress / len;
    return [a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k];
  }

  moving(): boolean {
    return !this.paused;
  }

  advance(dt: number): void {
    const r = this.seed.route;
    let remaining = this.seed.speed * dt;
    while (remaining > 0) {
      const len = dist(r[this.leg], r[(this.leg + 1) % r.length]);
      const left = len - this.progress;
      if (remaining < left) {
        this.progress += remaining;
        remaining = 0;
      } else {
        remaining -= left;
        this.leg = (this.leg + 1) % r.length;
        this.progress = 0;
      }
    }
  }

  // Battito verso 100 in movimento e 76 da fermo, SpO2 stabile, gas che sale dentro la zona.
  updateVitals(): void {
    const target = this.moving() ? 102 : 76;
    this.hr = clamp(this.hr + (target - this.hr) * 0.15 + jitter(3), 55, 160);
    this.spo2 = clamp(this.spo2 + (this.spo2Target - this.spo2) * 0.12 + jitter(0.4), 84, 99);
    const inGas = dist(this.position(), GAS_CENTER) < GAS_RADIUS_M;
    this.mq2 = clamp(inGas ? this.mq2 + 45 + jitter(10) : this.mq2 + (1800 - this.mq2) * 0.08 + jitter(15), 1650, 3200);
  }
}

// ---------- Allarmi automatici su soglia (THRESHOLDS in config.ts, guida §9.4) ----------
// Regola: il valore deve restare oltre soglia per un tempo minimo, altrimenti una lettura sbagliata
// del sensore farebbe partire un SOS. Nel sistema vero questo controllo lo fa il gilet o il backend;
// qui serve a far vedere il comportamento durante la dimostrazione.

interface ThresholdCheck {
  reason: string;
  kind: 'vital' | 'environment';
  cause: string;
  over: boolean;
  value: number;
  holdSec: number;
}

const overSince = new Map<string, number>();
let alertSeq = 10;

function thresholdChecks(w: Walker): ThresholdCheck[] {
  const spo2 = Math.round(w.spo2);
  const hr = Math.round(w.hr);
  const gas = Math.round(w.mq2);
  const vitalHold = Math.round(THRESHOLDS.SUSTAINED_MS / 1000);
  return [
    { reason: 'spo2_low', kind: 'vital', cause: 'ppg_emergency', over: spo2Level(spo2) === 'danger', value: spo2, holdSec: vitalHold },
    {
      reason: hr <= THRESHOLDS.heart_rate.dangerLow ? 'hr_low' : 'hr_high',
      kind: 'vital', cause: 'ppg_emergency', over: heartRateLevel(hr) === 'danger', value: hr, holdSec: vitalHold,
    },
    {
      reason: 'gas_high', kind: 'environment', cause: 'gas_emergency', over: gasLevel(gas) === 'danger', value: gas,
      holdSec: Math.round(THRESHOLDS.GAS_SUSTAINED_MS / 1000),
    },
  ];
}

function checkThresholds(w: Walker, t: number, latitude: number, longitude: number): void {
  const dev = store.devices.get(w.seed.id);
  for (const c of thresholdChecks(w)) {
    const key = `${w.seed.id}:${c.reason}`;
    if (!c.over) {
      overSince.delete(key);
      continue;
    }
    const from = overSince.get(key) ?? t;
    overSince.set(key, from);
    // Se il gilet ha già un allarme aperto non se ne apre un secondo.
    if (dev?.alert || t - from < c.holdSec) continue;
    overSince.delete(key);
    const sos = makeEvent(w.seed.id, 'SOS', 'critical', {
      origin: 'auto', cause: c.cause, kind: c.kind, reason: c.reason, value: c.value,
      latitude, longitude, battery: w.battery,
    });
    applyEvent(sos);
    setAlert(w.seed.id, { id: ++alertSeq, status: 'open', opened_at: sos.received_time, acknowledged_by: null });
  }
}

export function startMock(): () => void {
  store.operators = OPERATORS;
  store.health = { broker: 'online', bridge: 'online', database: 'online', radio: 'online' };
  store.events = [];
  store.safeZones = [];
  sequences.clear();
  overSince.clear();
  alertSeq = 10;

  const walkers = WALKERS.map((s) => new Walker(s));
  const now = Date.now();
  const [offLat, offLng] = toLatLng([OFFLINE_SEED.east, OFFLINE_SEED.north]);
  setDevices([
    ...walkers.map((w) => {
      const [latitude, longitude] = toLatLng(w.position());
      return {
        device_id: w.seed.id, operator_id: w.seed.operator, firmware: 'v1.2', last_seen: new Date(now - 2000).toISOString(),
        online: true, sos: false, battery: w.battery, spo2: 97, heart_rate: 78, mq2_raw: 1800,
        latitude, longitude, position_time: new Date(now - 2000).toISOString(), sensors_ok: w.seed.sensors,
      };
    }),
    {
      device_id: OFFLINE_SEED.id, operator_id: null, firmware: 'v1.2', last_seen: new Date(now - 12 * 60000).toISOString(),
      online: false, sos: false, battery: OFFLINE_SEED.battery, latitude: offLat, longitude: offLng,
      position_time: new Date(now - 12 * 60000).toISOString(),
    },
  ]);

  // Un po' di storico dell'ultimo quarto d'ora, per le pagine Storico e Dispositivi.
  for (let i = 12; i >= 1; i--) {
    const w = walkers[i % walkers.length];
    applyEvent(makeEvent(w.seed.id, 'TELEMETRY', 'normal', {
      spo2: 96 + (i % 3), heart_rate: 90 + (i % 7), mq2_raw: 1780 + i * 3, battery: w.battery + (i % 3),
    }, i * 75));
  }
  for (const w of walkers) {
    const dev = store.devices.get(w.seed.id)!;
    dev.last_seen = new Date().toISOString();
    dev.battery = w.battery;
  }

  store.safeZones = [
    {
      id: 1, device_id: '03', latitude: BASE_LAT + 0.0007, longitude: BASE_LNG + 0.0010,
      radius_m: 30, operator_id: 3, status: 'validated', created_at: new Date(now - 40 * 60000).toISOString(),
    },
  ];
  notify();

  let t = 0;
  const interval = window.setInterval(() => {
    t++;
    walkers.forEach((w, i) => {
      if (w.moving()) w.advance(1);
      w.updateVitals();
      if (t % 40 === i * 10) w.battery = Math.max(3, w.battery - 1);

      const silent = w.seed.id === '02' && t >= SCENARIO.signalLostFrom && t < SCENARIO.signalLostTo;
      if (silent) return;
      const [latitude, longitude] = toLatLng(w.position());

      if ((t + i) % 3 === 0) {
        applyEvent(makeEvent(w.seed.id, 'POSITION', 'normal', { latitude, longitude, accuracy_m: 4 + Math.round(Math.random() * 3), fix: '3D' }));
      }
      if ((t + i * 2) % 6 === 0) {
        applyEvent(makeEvent(w.seed.id, 'TELEMETRY', 'normal', {
          spo2: Math.round(w.spo2), heart_rate: Math.round(w.hr), mq2_raw: Math.round(w.mq2), battery: w.battery,
        }));
      }
      if ((t + i * 3) % 12 === 0) {
        applyEvent(makeEvent(w.seed.id, 'STATUS', 'medium', { battery: w.battery, online: true, sensors_ok: w.seed.sensors, firmware: 'v1.2' }));
      }

      // Allarmi automatici: gas (03 attraversa la zona con la fuga) e parametri vitali (05).
      checkThresholds(w, t, latitude, longitude);
    });

    if (t === SCENARIO.sosManual) {
      const w = walkers.find((x) => x.seed.id === '01')!;
      w.paused = true;
      const [latitude, longitude] = toLatLng(w.position());
      const sos = makeEvent('01', 'SOS', 'critical', { origin: 'voluntary', cause: 'manual_gesture', latitude, longitude, battery: w.battery });
      applyEvent(sos);
      setAlert('01', { id: 1, status: 'open', opened_at: sos.received_time, acknowledged_by: null });
    }

    // Falso allarme: il gilet 02 interpreta un movimento come gesto SOS, la base lo vede subito e
    // poco dopo il soccorritore annulla (SOS_CANCEL): la sirena si ferma, la scheda resta da chiudere.
    if (t === SCENARIO.falseAlarmFrom) {
      const w = walkers.find((x) => x.seed.id === '02')!;
      const [latitude, longitude] = toLatLng(w.position());
      const sos = makeEvent('02', 'SOS', 'critical', { origin: 'voluntary', cause: 'manual_gesture', latitude, longitude, battery: w.battery });
      applyEvent(sos);
      setAlert('02', { id: 3, status: 'open', opened_at: sos.received_time, acknowledged_by: null });
    }
    if (t === SCENARIO.falseAlarmTo) {
      const w = walkers.find((x) => x.seed.id === '02')!;
      const [latitude, longitude] = toLatLng(w.position());
      applyEvent(makeEvent('02', 'SOS_CANCEL', 'high', { origin: 'voluntary', cause: 'accidental_gesture', latitude, longitude, battery: w.battery }));
    }

    // Elena (gilet 05) inizia a perdere ossigenazione: nessuno preme niente, è il controllo sulle
    // soglie a far partire l'SOS quando il valore resta basso abbastanza a lungo.
    if (t === SCENARIO.desaturation) {
      walkers.find((x) => x.seed.id === '05')!.spo2Target = 88;
    }

    if (t === SCENARIO.safeZone) {
      const w = walkers.find((x) => x.seed.id === '05')!;
      const [latitude, longitude] = toLatLng(w.position());
      applyEvent(makeEvent('05', 'SAFE_ZONE', 'high', { triggered: true, latitude, longitude }));
      store.safeZones.push({
        id: 2, device_id: '05', latitude, longitude, radius_m: 30, operator_id: 4,
        status: 'proposed', created_at: new Date().toISOString(),
      });
      notify();
    }
  }, 1000);

  return () => window.clearInterval(interval);
}
