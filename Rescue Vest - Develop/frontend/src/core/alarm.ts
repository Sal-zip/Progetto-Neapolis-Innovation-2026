import { store, subscribe } from './store';

// Allarme sonoro (guida §7.1 e §13): suona finché un SOS non viene preso in carico.
// I browser bloccano l'audio finché l'utente non interagisce con la pagina: il login sblocca.

const INTERVAL_MS = 1600;
let ctx: AudioContext | null = null;
let timer: number | undefined;

// Suona solo per un SOS ancora da prendere in carico: tace quando è preso in carico e quando il
// gilet ha segnalato che era un falso allarme.
function ringing(): boolean {
  for (const d of store.devices.values()) {
    if (d.sos && d.alert?.status !== 'acknowledged' && d.alert?.status !== 'false_alarm') return true;
  }
  return false;
}

function tone(freq: number, start: number, duration: number): void {
  if (!ctx) return;
  const osc = ctx.createOscillator();
  const gain = ctx.createGain();
  osc.type = 'square';
  osc.frequency.value = freq;
  gain.gain.setValueAtTime(0.0001, start);
  gain.gain.exponentialRampToValueAtTime(0.15, start + 0.02);
  gain.gain.exponentialRampToValueAtTime(0.0001, start + duration);
  osc.connect(gain).connect(ctx.destination);
  osc.start(start);
  osc.stop(start + duration + 0.02);
}

function siren(): void {
  if (!ctx || ctx.state !== 'running') return;
  const t = ctx.currentTime;
  tone(880, t, 0.25);
  tone(660, t + 0.3, 0.25);
}

export function audioBlocked(): boolean {
  return ringing() && ctx?.state !== 'running';
}

let changed: () => void = () => {};

export function unlockAudio(): void {
  ctx ??= new AudioContext();
  if (ctx.state !== 'running') void ctx.resume().then(() => changed());
}

export function startAlarm(onChange: () => void): void {
  changed = onChange;
  for (const type of ['pointerdown', 'keydown'] as const) {
    document.addEventListener(type, unlockAudio, { passive: true });
  }
  subscribe(() => {
    if (ringing() && timer === undefined) {
      siren();
      timer = window.setInterval(siren, INTERVAL_MS);
    } else if (!ringing() && timer !== undefined) {
      window.clearInterval(timer);
      timer = undefined;
    }
    onChange();
  });
}
