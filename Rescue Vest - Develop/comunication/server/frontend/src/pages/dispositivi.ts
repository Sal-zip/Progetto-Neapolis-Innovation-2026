import { fetchDeviceEvents } from '../core/api';
import { patch } from '../core/dom';
import { operatorName, store, subscribe, subscribeEvents } from '../core/store';
import type { NiscEvent } from '../core/types';
import {
  batteryClass, esc, eventSummary, fmtTime, gasLevel, heartRateLevel, isNum, levelClass, outOfRange,
  priorityBadge, sensorName, spo2Level, timeAgo,
} from '../core/util';

const HISTORY_LIMIT = 50;

export function mount(el: HTMLElement): () => void {
  const root = document.createElement('div');
  root.className = 'flex flex-col lg:flex-row gap-4';
  root.innerHTML = `
    <div id="list" class="lg:w-72 shrink-0 card bg-base-100 shadow-sm border border-base-300 p-2 flex flex-col gap-1 self-start w-full"></div>
    <div id="detail" class="flex-1 min-w-0 card bg-base-100 shadow-sm border border-base-300"></div>`;
  el.appendChild(root);

  const list = root.querySelector<HTMLElement>('#list')!;
  const detail = root.querySelector<HTMLElement>('#detail')!;

  let history: NiscEvent[] = [];
  let historyFor: string | null = null;
  let historyError = '';
  let requestedFor: string | null = null;
  let request = 0;
  let refreshTimer: number | undefined;
  let disposed = false;

  function loadHistory(): void {
    const id = store.selectedDevice;
    if (!id) return;
    requestedFor = id;
    const req = ++request;
    fetchDeviceEvents(id, HISTORY_LIMIT)
      .then((events) => {
        if (disposed || req !== request) return;
        history = events;
        historyError = '';
        historyFor = id;
        render();
      })
      .catch((err: Error) => {
        if (disposed || req !== request) return;
        history = [];
        historyError = err.message;
        historyFor = id;
        render();
      });
  }

  function stat(label: string, value: string, cls = ''): string {
    return `<div class="rounded-box border border-base-300 p-4"><div class="text-xs text-base-content/60">${label}</div><div class="text-2xl font-bold ${cls}">${esc(value)}</div></div>`;
  }

  function recentEvents(deviceId: string): string {
    if (historyFor !== deviceId) return '<div class="px-4 py-3 text-sm text-base-content/60">Caricamento…</div>';
    if (historyError) return `<div class="px-4 py-3 text-sm text-error">Impossibile caricare lo storico: ${esc(historyError)}</div>`;
    return (
      history
        .slice(0, 6)
        .map((e) => {
          const p = priorityBadge(e.priority);
          return `<div class="flex items-center gap-3 px-4 py-2 text-sm border-b border-base-300 last:border-0"><span class="w-16 text-base-content/60">${esc(fmtTime(e.received_time))}</span><span class="badge ${p.cls} badge-sm w-20 justify-center">${esc(e.event_type)}</span><span class="truncate">${esc(eventSummary(e.event_type, e.data))}</span></div>`;
        })
        .join('') || '<div class="px-4 py-3 text-sm text-base-content/60">Nessun evento.</div>'
    );
  }

  function render(): void {
    const devices = [...store.devices.values()].sort((a, b) => a.device_id.localeCompare(b.device_id));
    if (!store.selectedDevice || !store.devices.has(store.selectedDevice)) {
      store.selectedDevice = devices[0]?.device_id ?? null;
    }
    if (store.selectedDevice && store.selectedDevice !== requestedFor) loadHistory();

    patch(
      list,
      devices
        .map((d) => {
          const dot = d.sos ? 'bg-error' : d.online ? 'bg-success' : 'bg-base-content/30';
          const active = d.device_id === store.selectedDevice ? 'bg-primary/10 shadow-[inset_3px_0_0_var(--color-primary)]' : 'hover:bg-base-200';
          return `
          <button data-key="${esc(d.device_id)}" data-select="${esc(d.device_id)}" class="flex items-center gap-3 rounded-box px-4 py-3 text-sm text-left ${active} ${d.online ? '' : 'opacity-55'}">
            <span class="size-2 rounded-full ${dot}"></span>
            <b>Gilet ${esc(d.device_id)}</b>
            <span class="ml-auto text-base-content/60">${esc(operatorName(d.operator_id))}</span>
          </button>`;
        })
        .join(''),
    );

    const d = store.selectedDevice ? store.devices.get(store.selectedDevice) : undefined;
    if (!d) {
      patch(detail, '<div class="card-body text-base-content/60">Nessun dispositivo disponibile.</div>');
      return;
    }

    const radio = d.radio ? stat('Segnale radio', `${d.radio.rssi} dBm`) : '';
    const anomalie = d.online ? outOfRange(d) : [];

    function bigBatteryIcon(b: number | null): string {
      if (typeof b !== 'number') return '<div class="p-4 text-base-content/50">Dati non disponibili</div>';
      const color = b <= 20 ? 'bg-error' : b <= 40 ? 'bg-warning' : 'bg-success';
      return `
        <div class="flex items-center justify-center p-8">
          <div class="relative flex flex-col justify-end w-24 h-48 border-4 border-base-content/30 rounded-xl p-1 bg-base-100/50">
            <!-- Terminal (top cap) -->
            <div class="absolute -top-3 left-1/2 -translate-x-1/2 w-8 h-2 bg-base-content/30 rounded-t-sm"></div>
            <!-- Inside liquid -->
            <div class="w-full rounded-lg transition-all duration-700 flex items-center justify-center ${color}" style="height: ${b}%">
              <span class="text-xl font-bold text-white drop-shadow-md mix-blend-overlay">${b}%</span>
            </div>
          </div>
        </div>
      `;
    }

    patch(
      detail,
      `
      <div class="card-body gap-4">
        <div class="flex items-start justify-between gap-3">
          <div>
            <h2 class="text-xl font-bold">Gilet ${esc(d.device_id)}</h2>
            <p class="text-sm text-base-content/60">Operatore: ${esc(operatorName(d.operator_id))} · Firmware ${esc(d.firmware)} · Ultimo contatto ${esc(timeAgo(d.last_seen))}</p>
            <div class="flex flex-wrap gap-1 mt-2">
              ${
                d.sensors_ok
                  ? Object.entries(d.sensors_ok)
                      .map(([k, ok]) => `<span class="badge badge-sm ${ok ? 'badge-ghost' : 'badge-warning'}">${esc(sensorName(k))}: ${ok ? 'ok' : 'guasto'}</span>`)
                      .join('')
                  : '<span class="text-xs text-base-content/50">Stato dei sensori non ancora ricevuto</span>'
              }
            </div>
          </div>
          ${
            d.sos && d.alert?.status === 'false_alarm'
              ? '<span class="badge badge-ghost">Falso allarme</span>'
              : d.sos
                ? '<span class="badge badge-error">SOS attivo</span>'
                : d.online
                  ? '<span class="badge badge-success">Online</span>'
                  : '<span class="badge badge-ghost">Offline</span>'
          }
        </div>
        ${
          anomalie.length
            ? `<p class="text-sm font-semibold ${anomalie.some((a) => a.level === 'danger') ? 'text-error' : 'text-warning'}">Valori fuori soglia: ${esc(anomalie.map((a) => a.label).join(', '))}</p>`
            : ''
        }
        <div class="grid grid-cols-2 lg:grid-cols-4 gap-3">
          ${stat('Batteria', isNum(d.battery) ? `${d.battery}%` : '-', batteryClass(d.battery))}
          ${stat('SpO2', String(d.spo2 ?? '-'), levelClass(spo2Level(d.spo2)))}
          ${stat('Frequenza cardiaca', d.heart_rate !== undefined ? `${d.heart_rate} bpm` : '-', levelClass(heartRateLevel(d.heart_rate)))}
          ${stat('Gas (MQ2 grezzo)', String(d.mq2_raw ?? '-'), levelClass(gasLevel(d.mq2_raw)))}
          ${radio}
        </div>
        <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
          <div>
            <h3 class="text-sm font-semibold text-base-content/60 mb-1">Stato Batteria</h3>
            <div class="rounded-box bg-base-200 border border-base-300 h-64 flex items-center justify-center">
              ${bigBatteryIcon(d.battery)}
            </div>
          </div>
          <div>
            <h3 class="text-sm font-semibold text-base-content/60 mb-1">Eventi recenti di questo gilet</h3>
            <div class="rounded-box border border-base-300 overflow-y-auto h-64">${recentEvents(d.device_id)}</div>
          </div>
        </div>
      </div>`,
    );
  }

  list.addEventListener('click', (e) => {
    const btn = (e.target as Element).closest<HTMLElement>('[data-select]');
    if (!btn) return;
    store.selectedDevice = btn.dataset.select!;
    render();
  });

  render();
  const unsubscribe = subscribe(render);
  // Nuovi eventi del gilet selezionato: si ricarica lo storico, al massimo una volta al secondo.
  const unsubscribeEvents = subscribeEvents((ev) => {
    if (ev.device_id !== store.selectedDevice || refreshTimer !== undefined) return;
    refreshTimer = window.setTimeout(() => {
      refreshTimer = undefined;
      loadHistory();
    }, 1000);
  });
  return () => {
    disposed = true;
    window.clearTimeout(refreshTimer);
    unsubscribe();
    unsubscribeEvents();
  };
}
