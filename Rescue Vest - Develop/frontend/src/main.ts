import 'leaflet/dist/leaflet.css';
import './style.css';
import { audioBlocked, startAlarm, unlockAudio } from './core/alarm';
import { applySession, fetchSession, login, logout, refreshHealth, setUnauthorizedHandler } from './core/api';
import { config } from './core/config';
import { ROUTES, startRouter } from './core/router';
import { connectSocket } from './core/socket';
import { operatorName, startOfflineWatch, store, subscribe, subscribeEvents } from './core/store';
import type { ServiceStatus, Session } from './core/types';
import { alarmReasonLabel, esc, sosCauseLabel } from './core/util';

const NAV: { path: string; label: string }[] = [
  { path: '/', label: 'Dashboard' },
  { path: '/mappa', label: 'Mappa live' },
  { path: '/eventi', label: 'Storico eventi' },
  { path: '/dispositivi', label: 'Dispositivi' },
  { path: '/operatori', label: 'Operatori' },
  { path: '/assistente', label: 'Assistente AI' },
];

// Giacca ad alta visibilità a maniche corte, con bande catarifrangenti.
const LOGO = (size: string) => `
  <svg class="${size} shrink-0" viewBox="0 0 32 32" aria-hidden="true">
    <path d="M11 3.5 6 5.5 1.5 10.5l4.5 3.5 2-2V28.5h16V12l2 2 4.5-3.5L26 5.5l-5-2c-1 2.2-2.8 3.3-5 3.3s-4-1.1-5-3.3Z"
      fill="#f97316" stroke="#c2410c" stroke-width="1" stroke-linejoin="round" />
    <path d="M8 17.5h16v2.6H8zM8 22.6h16v2.6H8z" fill="#e5e7eb" stroke="#94a3b8" stroke-width=".5" />
    <path d="M10.6 4.6 12.8 5.8v11.7h-2.2zM21.4 4.6 19.2 5.8v11.7h2.2z" fill="#e5e7eb" stroke="#94a3b8" stroke-width=".5" />
    <path d="M3.4 11.9 5.2 9.6l1.6 1.2-1.9 2.3zM28.6 11.9 26.8 9.6l-1.6 1.2 1.9 2.3z" fill="#e5e7eb" />
    <path d="M16 6.8v21.7" stroke="#9a3412" stroke-width="1" />
  </svg>`;

const app = document.getElementById('app')!;

// ---------- Login (guida §10.2: ogni comando deve avere un autore) ----------

function showLogin(): void {
  app.innerHTML = `
    <div class="min-h-screen flex items-center justify-center p-6">
      <form id="login-form" class="card w-full max-w-sm">
        <div class="card-body gap-3">
          <div class="flex items-center gap-2.5 mb-2">
            ${LOGO('size-10')}
            <h1 class="text-xl font-bold">Rescue veST</h1>
          </div>
          <p class="text-sm text-base-content/60">Accesso operatore della casa base</p>
          <label class="text-xs text-base-content/60" for="login-user">Nome utente</label>
          <input id="login-user" class="input input-bordered w-full" autocomplete="username" required />
          <label class="text-xs text-base-content/60" for="login-pass">Password</label>
          <input id="login-pass" type="password" class="input input-bordered w-full" autocomplete="current-password" required />
          <p id="login-error" class="text-sm text-error empty:hidden" role="alert"></p>
          <button class="btn btn-primary mt-2" type="submit">Accedi</button>
          ${import.meta.env.VITE_USE_MOCK !== 'false' ? '<p class="text-xs text-base-content/50">Modalità dimostrativa: qualunque nome e password.</p>' : ''}
        </div>
      </form>
    </div>`;
  const form = document.getElementById('login-form') as HTMLFormElement;
  const error = document.getElementById('login-error')!;
  (document.getElementById('login-user') as HTMLInputElement).focus();
  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    error.textContent = '';
    const user = (document.getElementById('login-user') as HTMLInputElement).value;
    const pass = (document.getElementById('login-pass') as HTMLInputElement).value;
    try {
      startApp(await login(user, pass));
    } catch (err) {
      error.textContent = (err as Error).message;
    }
  });
}

// ---------- Applicazione ----------

function dot(status: ServiceStatus): string {
  return status === 'online' ? 'bg-success' : status === 'degraded' ? 'bg-warning' : 'bg-error';
}

function startApp(session: Session): void {
  applySession(session);
  unlockAudio();

  app.innerHTML = `
    <div class="flex min-h-screen overflow-hidden">
      <aside id="sidebar" class="w-56 shrink-0 bg-neutral text-neutral-content p-3 flex flex-col gap-1 sticky top-0 h-screen transition-all duration-300">
        <div class="flex items-center gap-2.5 px-3 pt-2 pb-5">
          ${LOGO('size-8')}
          <span class="text-white font-semibold">Rescue veST</span>
        </div>
        <nav id="nav" class="flex flex-col gap-1">
          ${NAV.map((n) => `<a href="${n.path}" data-link data-path="${n.path}" class="rounded-field px-4 py-2.5 text-sm hover:bg-white/5">${n.label}</a>`).join('')}
        </nav>
        <div class="mt-auto px-3 py-2 flex flex-col gap-2">
          <div class="text-sm text-white">${esc(session.user.name)}</div>
          <div class="text-xs text-neutral-content/60">${esc(session.user.role)}</div>
          <button id="logout" class="btn btn-xs btn-ghost text-neutral-content justify-start px-0">Esci</button>
        </div>
      </aside>
      <div class="flex-1 min-w-0 flex flex-col max-h-screen overflow-auto">
        <header class="h-16 shrink-0 flex items-center justify-between px-6">
          <div class="flex items-center gap-4">
            <button id="sidebar-toggle" class="btn btn-square btn-ghost btn-sm" aria-label="Mostra o nascondi il menu">
              <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
                <path stroke-linecap="round" stroke-linejoin="round" d="M3.75 6.75h16.5M3.75 12h16.5m-16.5 5.25h16.5" />
              </svg>
            </button>
            <h1 id="title" class="text-xl font-bold"></h1>
            ${import.meta.env.VITE_USE_MOCK !== 'false' ? '<span class="badge badge-warning badge-sm" title="Questa versione mostra dati inventati, non quelli dei gilet">Dati simulati</span>' : ''}
          </div>
          <div class="flex items-center gap-2">
            <button id="audio-unlock" class="btn btn-sm btn-error" hidden>Attiva audio allarmi</button>
            <div id="health" class="gap-2 hidden md:flex"></div>
          </div>
        </header>
        <main id="view" class="flex-1 px-6 pb-8"></main>
      </div>
    </div>
    <div id="toasts" class="toast toast-end z-50"></div>`;

  const title = document.getElementById('title')!;
  const health = document.getElementById('health')!;
  const toasts = document.getElementById('toasts')!;
  const sidebar = document.getElementById('sidebar')!;
  const audioButton = document.getElementById('audio-unlock') as HTMLButtonElement;

  document.getElementById('sidebar-toggle')!.addEventListener('click', () => sidebar.classList.toggle('collapsed'));
  document.getElementById('logout')!.addEventListener('click', async () => {
    await logout();
    location.assign('/');
  });
  audioButton.addEventListener('click', unlockAudio);

  function setActive(path: string): void {
    document.querySelectorAll<HTMLElement>('#nav a').forEach((a) => {
      const active = a.dataset.path === path;
      a.classList.toggle('bg-white/10', active);
      a.classList.toggle('text-white', active);
      a.classList.toggle('shadow-[inset_3px_0_0_var(--color-primary)]', active);
    });
  }

  function renderHealth(): void {
    const items: [string, ServiceStatus][] = [
      ['Broker', store.health.broker],
      ['Bridge', store.health.bridge],
      ['Database', store.health.database],
    ];
    if (store.health.radio) items.push(['Radio', store.health.radio]);
    items.push(['Server', store.connected ? 'online' : 'offline']);
    health.innerHTML = items
      .map(([name, s]) => `<span class="badge badge-lg bg-base-100 shadow-sm gap-2 text-xs text-base-content/70"><i class="size-2 rounded-full ${dot(s)}"></i>${name}</span>`)
      .join('');
  }

  subscribeEvents((ev) => {
    if (ev.event_type !== 'SOS' && ev.event_type !== 'SOS_CANCEL') return;
    const dev = store.devices.get(ev.device_id);
    const cancel = ev.event_type === 'SOS_CANCEL';
    const reason = ev.data.reason ? ` · ${esc(alarmReasonLabel(ev.data.reason, ev.data.value))}` : '';
    const toast = document.createElement('div');
    toast.className = `alert shadow-lg ${cancel ? 'bg-base-100' : 'alert-error'}`;
    toast.innerHTML = `<span><b>${cancel ? 'Falso allarme' : ev.data.origin === 'auto' ? 'SOS automatico' : 'SOS'}</b> — Gilet ${esc(ev.device_id)} · ${esc(operatorName(dev?.operator_id ?? null))} · ${esc(sosCauseLabel(ev.data.cause))}${reason}</span>
      <a href="/" data-link class="btn btn-sm">Vai alla dashboard</a>`;
    toasts.appendChild(toast);
    window.setTimeout(() => toast.remove(), 12000);
  });

  toasts.addEventListener('click', (e) => {
    (e.target as Element).closest('a')?.closest('.alert')?.remove();
  });

  subscribe(renderHealth);
  renderHealth();
  startAlarm(() => {
    audioButton.hidden = !audioBlocked();
  });

  // Confronto scritto per esteso: nella build reale Vite lo riduce a "false" e i dati finti
  // non vengono nemmeno inclusi nei file pubblicati.
  if (import.meta.env.VITE_USE_MOCK !== 'false') {
    void import('./core/mock').then((m) => m.startMock());
  } else {
    void refreshHealth();
    window.setInterval(() => void refreshHealth(), 10000);
    connectSocket();
  }
  startOfflineWatch(config.OFFLINE_AFTER_MS);

  startRouter(document.getElementById('view')!, (path, pageTitle) => {
    title.textContent = ROUTES[path]?.title ?? pageTitle;
    document.title = `${pageTitle} · Rescue veST`;
    setActive(path);
  });
}

// Sessione scaduta mentre si usa l'app: si ricarica e si torna al login.
setUnauthorizedHandler(() => location.reload());

void fetchSession().then((session) => (session ? startApp(session) : showLogin()));
