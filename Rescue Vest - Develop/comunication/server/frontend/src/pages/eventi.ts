import { fetchEvents } from '../core/api';
import { patch } from '../core/dom';
import { store, subscribeEvents } from '../core/store';
import { esc, eventSummary, fmtDateTime, priorityBadge } from '../core/util';

const PAGE_SIZE = 12;
const TYPES = ['SOS', 'SOS_CANCEL', 'TELEMETRY', 'POSITION', 'SAFE_ZONE', 'STATUS'];

export function mount(el: HTMLElement): () => void {
  const root = document.createElement('div');
  root.className = 'flex flex-col gap-4';
  root.innerHTML = `
    <div class="flex flex-wrap gap-3 items-center">
      <select id="f-device" class="select select-bordered" aria-label="Filtra per dispositivo"></select>
      <select id="f-type" class="select select-bordered" aria-label="Filtra per tipo">
        <option value="">Tutti i tipi</option>
        ${TYPES.map((t) => `<option value="${t}">${t}</option>`).join('')}
      </select>
    </div>
    <div class="card bg-base-100 shadow-sm border border-base-300 overflow-x-auto">
      <table class="table">
        <thead><tr><th>Ora</th><th>Dispositivo</th><th>Tipo</th><th>Priorità</th><th>Dettagli</th></tr></thead>
        <tbody id="rows"></tbody>
      </table>
    </div>
    <div class="flex items-center justify-between text-sm text-base-content/60">
      <span id="total"></span>
      <div class="join">
        <button id="prev" class="join-item btn btn-sm" aria-label="Pagina precedente">‹</button>
        <button id="page" class="join-item btn btn-sm btn-primary pointer-events-none" tabindex="-1"></button>
        <button id="next" class="join-item btn btn-sm" aria-label="Pagina successiva">›</button>
      </div>
    </div>`;
  el.appendChild(root);

  const selDevice = root.querySelector<HTMLSelectElement>('#f-device')!;
  const selType = root.querySelector<HTMLSelectElement>('#f-type')!;
  const rows = root.querySelector<HTMLElement>('#rows')!;
  const total = root.querySelector<HTMLElement>('#total')!;
  const pageLabel = root.querySelector<HTMLElement>('#page')!;
  const prev = root.querySelector<HTMLButtonElement>('#prev')!;
  const next = root.querySelector<HTMLButtonElement>('#next')!;

  let page = 0;
  let deviceKey = '';
  let disposed = false;
  let request = 0;
  let refreshTimer: number | undefined;

  function fillDevices(): void {
    const ids = [...store.devices.keys()].sort();
    const key = ids.join(',');
    if (key === deviceKey) return;
    deviceKey = key;
    const current = selDevice.value;
    selDevice.innerHTML =
      `<option value="">Tutti i dispositivi</option>` + ids.map((i) => `<option value="${esc(i)}">Gilet ${esc(i)}</option>`).join('');
    selDevice.value = ids.includes(current) ? current : '';
  }

  async function load(): Promise<void> {
    const id = ++request;
    fillDevices();
    try {
      const res = await fetchEvents({
        device_id: selDevice.value || undefined,
        event_type: selType.value || undefined,
        limit: PAGE_SIZE,
        offset: page * PAGE_SIZE,
      });
      if (disposed || id !== request) return;
      const pages = Math.max(1, Math.ceil(res.total / PAGE_SIZE));
      patch(
        rows,
        res.events
          .map((e) => {
            const p = priorityBadge(e.priority);
            return `
            <tr>
              <td class="whitespace-nowrap">${esc(fmtDateTime(e.received_time))}</td>
              <td>Gilet ${esc(e.device_id)}</td>
              <td>${esc(e.event_type)}</td>
              <td><span class="badge ${p.cls} badge-sm">${esc(p.label)}</span></td>
              <td>${esc(eventSummary(e.event_type, e.data))}</td>
            </tr>`;
          })
          .join('') || '<tr><td colspan="5" class="text-base-content/60">Nessun evento con questi filtri.</td></tr>',
      );
      total.textContent = `${res.total} eventi totali`;
      pageLabel.textContent = `${page + 1} / ${pages}`;
      prev.disabled = page === 0;
      next.disabled = page + 1 >= pages;
    } catch (err) {
      if (disposed || id !== request) return;
      patch(rows, `<tr><td colspan="5" class="text-error">Impossibile caricare gli eventi: ${esc((err as Error).message)}</td></tr>`);
    }
  }

  function reload(): void {
    void load();
  }

  selDevice.addEventListener('change', () => { page = 0; reload(); });
  selType.addEventListener('change', () => { page = 0; reload(); });
  prev.addEventListener('click', () => { page = Math.max(0, page - 1); reload(); });
  next.addEventListener('click', () => { page += 1; reload(); });

  reload();
  // Solo la prima pagina segue i nuovi eventi, al massimo una volta al secondo: con il server
  // collegato ogni aggiornamento è una richiesta HTTP.
  const unsubscribe = subscribeEvents(() => {
    if (page !== 0 || refreshTimer !== undefined) return;
    refreshTimer = window.setTimeout(() => {
      refreshTimer = undefined;
      reload();
    }, 1000);
  });
  return () => {
    disposed = true;
    window.clearTimeout(refreshTimer);
    unsubscribe();
  };
}
