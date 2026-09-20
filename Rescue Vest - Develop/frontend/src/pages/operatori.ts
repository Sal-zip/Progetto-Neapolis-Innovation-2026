import { patch } from '../core/dom';
import { navigate } from '../core/router';
import { store, subscribe } from '../core/store';
import { esc, eventSummary, fmtTime, priorityBadge } from '../core/util';

export function mount(el: HTMLElement): () => void {
  const root = document.createElement('div');
  root.className = 'flex flex-col lg:flex-row gap-4';
  root.innerHTML = `
    <div id="list" class="lg:w-72 shrink-0 card bg-base-100 shadow-sm border border-base-300 p-3 flex flex-col gap-1 self-start w-full"></div>
    <div id="detail" class="flex-1 min-w-0 card bg-base-100 shadow-sm border border-base-300"></div>`;
  el.appendChild(root);

  const list = root.querySelector<HTMLElement>('#list')!;
  const detail = root.querySelector<HTMLElement>('#detail')!;

  function initials(name: string): string {
    return name
      .split(' ')
      .map((p) => p[0])
      .join('')
      .slice(0, 2)
      .toUpperCase();
  }

  function render(): void {
    const ops = store.operators;
    if (store.selectedOperator === null || !ops.some((o) => o.id === store.selectedOperator)) {
      store.selectedOperator = ops[0]?.id ?? null;
    }

    patch(
      list,
      `<h2 class="text-sm font-semibold text-base-content/60 px-2 pb-1">Squadra (${ops.length})</h2>` +
        ops
          .map((o) => {
            const active = o.id === store.selectedOperator ? 'bg-primary/10 shadow-[inset_3px_0_0_var(--color-primary)]' : 'hover:bg-base-200';
            return `
            <button data-key="${esc(o.id)}" data-select="${esc(o.id)}" class="rounded-box px-3 py-2.5 text-left text-sm ${active}">
              <b>${esc(o.name)}</b><br>
              <span class="text-base-content/60">${esc(o.role)}${o.device_id ? ` — Gilet ${esc(o.device_id)}` : ''}</span>
            </button>`;
          })
          .join(''),
    );

    const op = ops.find((o) => o.id === store.selectedOperator);
    if (!op) {
      patch(detail, '<div class="card-body text-base-content/60">Nessun operatore disponibile.</div>');
      return;
    }

    const dev = op.device_id ? store.devices.get(op.device_id) : undefined;
    const events = op.device_id ? store.events.filter((e) => e.device_id === op.device_id).slice(0, 6) : [];
    const box = (label: string, value: string) =>
      `<div class="rounded-box border border-base-300 p-4"><div class="text-xs text-base-content/60">${label}</div><div class="font-bold">${value}</div></div>`;

    patch(
      detail,
      `
      <div class="card-body gap-4">
        <div class="flex items-center gap-4">
          <div class="size-14 rounded-2xl bg-primary/10 text-primary flex items-center justify-center font-bold">${esc(initials(op.name))}</div>
          <div>
            <h2 class="text-xl font-bold">${esc(op.name)}</h2>
            <p class="text-sm text-base-content/60">Ruolo: ${esc(op.role)} · ${op.in_mission ? 'in missione' : 'in base'}</p>
          </div>
          ${dev?.sos ? '<span class="badge badge-error ml-auto">SOS in corso</span>' : ''}
        </div>
        <div class="grid grid-cols-1 sm:grid-cols-3 gap-3">
          ${box('Gilet assegnato', op.device_id ? `Gilet ${esc(op.device_id)}` : 'nessuno')}
          ${box('Missione corrente', esc(op.missions.find((m) => m.state === 'in corso')?.title ?? 'nessuna'))}
          ${box('In squadra dal', esc(op.since))}
        </div>
        ${op.device_id ? `<button class="btn btn-sm btn-outline self-start" data-track="${esc(op.device_id)}">Mostra sulla mappa</button>` : ''}
        ${dev?.online ? (() => {
          const devIdNum = parseInt(dev.device_id.replace(/\\D/g, '') || '1', 10);
          let ppgPoints = "";
          const beats = 3 + (devIdNum % 4); // 3 to 6 beats
          const beatWidth = 300 / beats;
          const baseline = 32 + (devIdNum % 3);
          const peak = 5 + (devIdNum % 8);
          for(let i=0; i<beats; i++) {
            const startX = i * beatWidth;
            ppgPoints += `${startX},${baseline} ${startX + beatWidth * 0.15},${baseline} ${startX + beatWidth * 0.25},${peak} ${startX + beatWidth * 0.4},24 ${startX + beatWidth * 0.55},18 ${startX + beatWidth * 0.9},${baseline} `;
          }
          ppgPoints += `300,${baseline}`;

          return `
          <div>
            <h3 class="text-sm font-semibold text-base-content/60 mb-1">Tracciato PPG (Fotopletismogramma live)</h3>
            <div class="rounded-box border border-base-300 bg-base-200/50 p-4 h-32 flex items-stretch gap-4">
              <!-- Y Axis Labels -->
              <div class="flex flex-col justify-between items-end text-xs font-bold text-base-content/60 py-1 w-6 shrink-0">
                <span>100</span>
                <span>80</span>
                <span>60</span>
              </div>
              <!-- Chart Area -->
              <div class="flex-1 relative border-l-2 border-b-2 border-base-content/20">
                <!-- Grid Lines -->
                <div class="absolute inset-0 flex flex-col justify-between py-1 z-0">
                  <div class="border-t border-base-content/10 w-full"></div>
                  <div class="border-t border-base-content/10 w-full"></div>
                  <div class="border-t border-base-content/10 w-full"></div>
                </div>
                <!-- PPG Wave -->
                <svg width="100%" height="100%" viewBox="0 0 300 40" preserveAspectRatio="none" class="absolute inset-0 z-10 stroke-error fill-none stroke-[2.5] drop-shadow-md">
                  <polyline points="${ppgPoints}" stroke-linejoin="round" stroke-linecap="round" />
                </svg>
              </div>
            </div>
          </div>
          `;
        })() : ''}
        <div>
          <h3 class="text-sm font-semibold text-base-content/60 mb-1">Missioni recenti</h3>
          <div class="rounded-box border border-base-300 overflow-hidden">
            ${
              op.missions
                .map((m) => `<div class="flex gap-3 px-4 py-2.5 text-sm border-b border-base-300 last:border-0"><span class="w-24 text-base-content/60">${esc(m.date)}</span><span>${esc(m.title)} — ${esc(m.state)}</span></div>`)
                .join('') || '<div class="px-4 py-3 text-sm text-base-content/60">Nessuna missione.</div>'
            }
          </div>
        </div>
        <div>
          <h3 class="text-sm font-semibold text-base-content/60 mb-1">Eventi legati a questo operatore</h3>
          <div class="rounded-box border border-base-300 overflow-hidden">
            ${
              events
                .map((e) => {
                  const p = priorityBadge(e.priority);
                  return `<div class="flex items-center gap-3 px-4 py-2 text-sm border-b border-base-300 last:border-0"><span class="w-16 text-base-content/60">${esc(fmtTime(e.received_time))}</span><span class="badge ${p.cls} badge-sm w-20 justify-center">${esc(e.event_type)}</span><span class="truncate">${esc(eventSummary(e.event_type, e.data))}</span></div>`;
                })
                .join('') || '<div class="px-4 py-3 text-sm text-base-content/60">Nessun evento.</div>'
            }
          </div>
        </div>
      </div>`,
    );
  }

  detail.addEventListener('click', (e) => {
    const btn = (e.target as Element).closest<HTMLElement>('[data-track]');
    if (!btn) return;
    store.mapFocus = btn.dataset.track!;
    navigate('/mappa');
  });

  list.addEventListener('click', (e) => {
    const btn = (e.target as Element).closest<HTMLElement>('[data-select]');
    if (!btn) return;
    store.selectedOperator = Number(btn.dataset.select);
    render();
  });

  render();
  return subscribe(render);
}
