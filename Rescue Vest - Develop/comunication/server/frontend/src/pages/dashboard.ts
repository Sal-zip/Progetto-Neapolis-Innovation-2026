import { COMMAND_PRESETS, CUSTOM_MESSAGE } from '../core/config';
import { acknowledgeAlert, closeAlert, sendCommand } from '../core/api';
import { patch } from '../core/dom';
import { operatorName, store, subscribe } from '../core/store';
import {
  alarmReasonLabel, esc, eventSummary, faultySensors, fmtTime, heartRateLevel, levelClass, outOfRange,
  priorityBadge, renderBattery, sosCauseLabel, spo2Level, timeAgo,
} from '../core/util';

export function mount(el: HTMLElement): () => void {
  const root = document.createElement('div');
  root.className = 'flex flex-col gap-4';
  root.innerHTML = `
    <div id="sos-banner" class="flex flex-col gap-2"></div>
    <p id="alert-error" class="text-sm text-error empty:hidden" role="alert"></p>
    <div class="flex flex-col xl:flex-row gap-4">
      <div class="flex-1 min-w-0 flex flex-col gap-3">
        <h2 id="fleet-title" class="text-sm font-semibold text-base-content/60"></h2>
        <div id="fleet" class="grid grid-cols-1 md:grid-cols-2 2xl:grid-cols-3 gap-3"></div>
        <h2 class="text-sm font-semibold text-base-content/60 mt-2">Ultimi eventi</h2>
        <div id="recent" class="card bg-base-100 shadow-sm border border-base-300 overflow-hidden"></div>
      </div>
      <aside class="xl:w-72 shrink-0">
        <div class="card bg-base-100 shadow-sm border border-base-300">
          <form id="cmd-form" class="card-body p-5 gap-3">
            <h2 class="font-bold">Comandi rapidi</h2>
            <label class="text-xs text-base-content/60" for="cmd-target">Destinatario</label>
            <select id="cmd-target" class="select select-bordered w-full"></select>
            <label class="text-xs text-base-content/60" for="cmd-preset">Messaggio</label>
            <select id="cmd-preset" class="select select-bordered w-full">
              ${COMMAND_PRESETS.map((p) => `<option value="${esc(p.id)}">${esc(p.label)}</option>`).join('')}
              <option value="${CUSTOM_MESSAGE.id}">Messaggio personalizzato…</option>
            </select>
            <div id="cmd-custom" class="flex flex-col gap-1" hidden>
              <label class="text-xs text-base-content/60" for="cmd-text">Testo del messaggio</label>
              <textarea id="cmd-text" class="textarea textarea-bordered w-full" rows="3" maxlength="${CUSTOM_MESSAGE.maxLength}" placeholder="Es. Raggiungete il punto di raccolta nord"></textarea>
              <span id="cmd-count" class="text-xs text-base-content/50 self-end">0/${CUSTOM_MESSAGE.maxLength}</span>
            </div>
            <button class="btn btn-primary" type="submit">Invia comando</button>
            <p id="cmd-result" class="text-xs min-h-4"></p>
          </form>
        </div>
      </aside>
    </div>`;
  el.appendChild(root);

  const banner = root.querySelector<HTMLElement>('#sos-banner')!;
  const fleet = root.querySelector<HTMLElement>('#fleet')!;
  const fleetTitle = root.querySelector<HTMLElement>('#fleet-title')!;
  const recent = root.querySelector<HTMLElement>('#recent')!;
  const target = root.querySelector<HTMLSelectElement>('#cmd-target')!;
  const form = root.querySelector<HTMLFormElement>('#cmd-form')!;
  const result = root.querySelector<HTMLElement>('#cmd-result')!;
  let targetKey = '';

  function render(): void {
    const devices = [...store.devices.values()].sort((a, b) => a.device_id.localeCompare(b.device_id));

    patch(
      banner,
      devices
        .filter((d) => d.sos)
        .map((d) => {
          const id = esc(d.device_id);
          const reason = d.sos_reason ? ` · ${esc(alarmReasonLabel(d.sos_reason, d.sos_value))}` : '';
          const where = `Gilet ${id} · ${esc(operatorName(d.operator_id))} · ${esc(sosCauseLabel(d.sos_cause))}${reason}`;
          const contact = `Ultimo contatto ${esc(timeAgo(d.last_seen))}${d.online ? '' : ' · GILET NON RAGGIUNGIBILE'}`;
          // Il gilet ha annullato l'SOS: resta in evidenza, ma senza sirena e senza rosso.
          if (d.alert?.status === 'false_alarm') {
            return `
            <div data-key="${id}" class="rounded-box bg-base-100 border border-base-300 p-4 flex items-center gap-4">
              <span class="size-2.5 rounded-full bg-base-content/30"></span>
              <div class="flex-1">
                <div class="font-bold text-sm">FALSO ALLARME — ${where}</div>
                <div class="text-xs text-base-content/60">Annullato dal gilet · ${contact}</div>
              </div>
              <button class="btn btn-sm btn-outline" data-close="${id}">Chiudi allarme</button>
            </div>`;
          }
          if (d.alert?.status === 'acknowledged') {
            return `
            <div data-key="${id}" class="rounded-box bg-linear-to-br from-amber-500 to-amber-600 text-white p-4 flex items-center gap-4">
              <span class="size-2.5 rounded-full bg-white"></span>
              <div class="flex-1">
                <div class="font-bold text-sm">SOS PRESO IN CARICO — ${where}</div>
                <div class="text-xs opacity-90">da ${esc(d.alert.acknowledged_by ?? 'operatore della base')} · ${contact}</div>
              </div>
              <button class="btn btn-sm bg-white text-warning border-0" data-close="${id}">Chiudi allarme</button>
            </div>`;
          }
          const button = d.alert
            ? `<button class="btn btn-sm bg-white text-error border-0" data-ack="${id}">Prendi in carico</button>`
            : `<button class="btn btn-sm bg-white/80 text-error border-0" disabled>In attesa del server…</button>`;
          return `
          <div data-key="${id}" class="sos-banner rounded-box bg-linear-to-br from-red-500 to-red-600 text-white p-4 flex items-center gap-4">
            <span class="size-2.5 rounded-full bg-white"></span>
            <div class="flex-1">
              <div class="font-bold text-sm">SOS ATTIVO — ${where}</div>
              <div class="text-xs opacity-85">${contact}</div>
            </div>
            ${button}
          </div>`;
        })
        .join(''),
    );

    fleetTitle.textContent = `Flotta gilet (${devices.length})`;
    patch(
      fleet,
      devices
        .map((d) => {
          const dot = d.sos ? 'bg-error' : d.online ? 'bg-success' : 'bg-base-content/30';
          const vitals = d.online
            ? `${renderBattery(d.battery)}<span class="${levelClass(spo2Level(d.spo2))}">SpO2 ${esc(d.spo2 ?? '-')}</span><span class="${levelClass(heartRateLevel(d.heart_rate))}">HR ${esc(d.heart_rate ?? '-')}</span>`
            : `<span>offline · ${esc(timeAgo(d.last_seen))}</span>`;
          // Valori oltre le soglie di config.ts: la batteria è già nell'icona, qui non si ripete.
          const anomalie = d.online ? outOfRange(d, false) : [];
          return `
          <a href="/dispositivi" data-link data-key="${esc(d.device_id)}" data-select-device="${esc(d.device_id)}"
             class="card bg-base-100 shadow-sm border border-base-300 hover:shadow-md transition-shadow ${d.online ? '' : 'opacity-60'}">
            <div class="card-body p-4 gap-1">
              <div class="flex items-center justify-between"><span class="font-bold">Gilet ${esc(d.device_id)}</span><span class="size-2 rounded-full ${dot}"></span></div>
              <div class="text-xs text-base-content/60">${esc(operatorName(d.operator_id))}</div>
              <div class="flex gap-3 mt-2 text-xs text-base-content/70">${vitals}</div>
              ${anomalie.length ? `<div class="text-xs font-semibold mt-1 ${anomalie.some((a) => a.level === 'danger') ? 'text-error' : 'text-warning'}">Fuori soglia: ${esc(anomalie.map((a) => a.label).join(', '))}</div>` : ''}
              ${faultySensors(d.sensors_ok).length ? `<div class="text-xs text-warning font-semibold mt-1">Sensore guasto: ${esc(faultySensors(d.sensors_ok).join(', '))}</div>` : ''}
            </div>
          </a>`;
        })
        .join('') || '<p class="text-sm text-base-content/60">Nessun gilet registrato.</p>',
    );

    patch(
      recent,
      store.events
        .slice(0, 5)
        .map((e) => {
          const p = priorityBadge(e.priority);
          return `
          <div class="flex items-center gap-3 px-4 py-3 border-b border-base-300 last:border-0 text-sm">
            <span class="w-16 text-base-content/60">${esc(fmtTime(e.received_time))}</span>
            <span class="badge ${p.cls} badge-sm w-20 justify-center">${esc(e.event_type)}</span>
            <span class="truncate">Gilet ${esc(e.device_id)} — ${esc(eventSummary(e.event_type, e.data))}</span>
          </div>`;
        })
        .join('') || '<p class="px-4 py-3 text-sm text-base-content/60">Nessun evento ricevuto.</p>',
    );

    const ids = devices.map((d) => d.device_id);
    const key = ids.join(',');
    if (key !== targetKey) {
      const current = target.value;
      targetKey = key;
      target.innerHTML =
        `<option value="broadcast">Tutti i gilet</option>` +
        ids.map((id) => `<option value="${esc(id)}">Gilet ${esc(id)}</option>`).join('');
      target.value = ids.includes(current) ? current : 'broadcast';
    }
  }

  const alertError = root.querySelector<HTMLElement>('#alert-error')!;

  async function runAlertAction(action: () => Promise<void>): Promise<void> {
    alertError.textContent = '';
    try {
      await action();
    } catch (err) {
      alertError.textContent = `Errore: ${(err as Error).message}`;
    }
  }

  root.addEventListener('click', (e) => {
    const t = e.target as Element;
    // Si legge tutto prima di agire: la presa in carico trasforma lo stesso pulsante in
    // "Chiudi allarme", e rileggendolo dopo lo stesso click chiuderebbe anche l'allarme.
    const ackId = t.closest<HTMLElement>('[data-ack]')?.dataset.ack;
    const closeId = t.closest<HTMLElement>('[data-close]')?.dataset.close;
    if (ackId) void runAlertAction(() => acknowledgeAlert(ackId));
    else if (closeId) void runAlertAction(() => closeAlert(closeId));
    const card = t.closest<HTMLElement>('[data-select-device]');
    if (card) store.selectedDevice = card.dataset.selectDevice!;
  });

  const presetSel = root.querySelector<HTMLSelectElement>('#cmd-preset')!;
  const customBox = root.querySelector<HTMLElement>('#cmd-custom')!;
  const customText = root.querySelector<HTMLTextAreaElement>('#cmd-text')!;
  const customCount = root.querySelector<HTMLElement>('#cmd-count')!;

  presetSel.addEventListener('change', () => {
    customBox.hidden = presetSel.value !== CUSTOM_MESSAGE.id;
    if (!customBox.hidden) customText.focus();
  });
  customText.addEventListener('input', () => {
    customCount.textContent = `${customText.value.length}/${CUSTOM_MESSAGE.maxLength}`;
  });

  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    const preset = presetSel.value;
    result.className = 'text-xs min-h-4 text-base-content/60';
    result.textContent = 'Invio…';
    try {
      await sendCommand(target.value, preset, customText.value);
      if (preset === CUSTOM_MESSAGE.id) {
        customText.value = '';
        customCount.textContent = `0/${CUSTOM_MESSAGE.maxLength}`;
      }
      result.className = 'text-xs min-h-4 text-success';
      result.textContent = `Comando inviato da ${store.user?.name ?? 'operatore'}.`;
    } catch (err) {
      result.className = 'text-xs min-h-4 text-error';
      result.textContent = `Errore: ${(err as Error).message}`;
    }
  });

  render();
  return subscribe(render);
}
