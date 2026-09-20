import L from 'leaflet';
import { setSafeZoneStatus } from '../core/api';
import { config } from '../core/config';
import { patch } from '../core/dom';
import { navigate } from '../core/router';
import { operatorName, store, subscribe } from '../core/store';
import { coord, esc, fmtTime, positionFreshness } from '../core/util';

const CENTER: L.LatLngTuple = [40.8518, 14.2681];

const COLORS = {
  online: '#059669',
  offline: '#94a3b8',
  sos: '#dc2626',
  zone: '#2563eb',
};

export function mount(el: HTMLElement): () => void {
  const root = document.createElement('div');
  root.className = 'flex flex-col lg:flex-row gap-4';
  root.innerHTML = `
    <aside class="lg:w-64 shrink-0 flex flex-col gap-2">
      <h2 class="text-sm font-semibold text-base-content/60">Gilet sul campo</h2>
      <div id="map-list" class="flex flex-col gap-2"></div>
      <div class="flex flex-wrap gap-2 mt-2 text-xs text-base-content/70">
        <span class="badge badge-ghost gap-1"><i class="size-2 rounded-full" style="background:${COLORS.online}"></i>Online</span>
        <span class="badge badge-ghost gap-1"><i class="size-2 rounded-full" style="background:${COLORS.offline}"></i>Offline</span>
        <span class="badge badge-ghost gap-1"><i class="size-2 rounded-full" style="background:${COLORS.sos}"></i>SOS</span>
        <span class="badge badge-ghost gap-1"><i class="size-2 rounded-full border-2" style="border-color:${COLORS.zone}"></i>Zona sicura</span>
      </div>
      <p id="map-notice" class="alert alert-warning text-xs mt-2" role="status" hidden>
        Nessuna connessione e nessuna mappa offline installata: lo sfondo della mappa non è disponibile, posizioni e zone restano visibili.
      </p>
      <h2 class="text-sm font-semibold text-base-content/60 mt-4">Zone sicure proposte</h2>
      <div id="zone-list" class="flex flex-col gap-2"></div>
      <p id="zone-error" class="text-xs text-error empty:hidden" role="alert"></p>
    </aside>
    <div id="map" class="flex-1 min-h-[28rem] h-[calc(100vh-9rem)] shadow-md"></div>`;
  el.appendChild(root);

  const map = L.map(root.querySelector<HTMLElement>('#map')!).setView(CENTER, 16);
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19,
    attribution: '&copy; OpenStreetMap',
  }).addTo(map);
  const sizeTimer = window.setTimeout(() => map.invalidateSize(), 0);

  const markers = new Map<string, L.CircleMarker>();
  const popups = new Map<string, string>();
  const zones = new Map<number, L.Circle>();
  const list = root.querySelector<HTMLElement>('#map-list')!;
  const zoneList = root.querySelector<HTMLElement>('#zone-list')!;
  const zoneError = root.querySelector<HTMLElement>('#zone-error')!;
  const notice = root.querySelector<HTMLElement>('#map-notice')!;
  let disposed = false;
  let localTiles = false;

  // Movimento fluido: tra una posizione ricevuta e la successiva il marker scivola invece di
  // saltare. L'animazione termina sempre sul punto reale e dura quanto l'intervallo tra gli
  // aggiornamenti, al massimo 5 s, così non resta indietro rispetto al dato. Salti grandi
  // (es. rientro dopo una perdita di segnale) non vengono animati.
  const MAX_GLIDE_MS = 5000;
  const MAX_GLIDE_M = 300;
  const glides = new Map<string, { from: L.LatLng; to: L.LatLng; start: number; duration: number }>();
  const lastTargetAt = new Map<string, number>();
  let frame = 0;

  function step(now: number): void {
    frame = 0;
    for (const [id, g] of glides) {
      const m = markers.get(id);
      if (!m) {
        glides.delete(id);
        continue;
      }
      const k = Math.min(1, (now - g.start) / g.duration);
      m.setLatLng([g.from.lat + (g.to.lat - g.from.lat) * k, g.from.lng + (g.to.lng - g.from.lng) * k]);
      if (k >= 1) glides.delete(id);
    }
    if (glides.size) frame = requestAnimationFrame(step);
  }

  function moveMarker(id: string, m: L.CircleMarker, pos: L.LatLngTuple): void {
    const target = L.latLng(pos);
    const current = m.getLatLng();
    if (current.equals(target) || glides.get(id)?.to.equals(target)) return;
    const now = performance.now();
    const since = now - (lastTargetAt.get(id) ?? now);
    lastTargetAt.set(id, now);
    if (current.distanceTo(target) > MAX_GLIDE_M || matchMedia('(prefers-reduced-motion: reduce)').matches) {
      glides.delete(id);
      m.setLatLng(target);
      return;
    }
    glides.set(id, { from: current, to: target, start: now, duration: Math.min(MAX_GLIDE_MS, Math.max(600, since)) });
    if (!frame) frame = requestAnimationFrame(step);
  }

  function updateNotice(): void {
    notice.hidden = localTiles || navigator.onLine;
  }

  // Mappe offline (guida §13.1): se sul PC ci sono le tessere della zona, stanno sopra quelle
  // online e la mappa funziona anche senza Internet.
  async function addLocalTiles(): Promise<void> {
    try {
      const res = await fetch(config.LOCAL_TILES_MANIFEST);
      if (!res.ok || !(res.headers.get('content-type') ?? '').includes('json')) return;
      const m = (await res.json()) as { minZoom: number; maxZoom: number; bounds: L.LatLngBoundsExpression };
      if (disposed) return;
      L.tileLayer(config.LOCAL_TILES_URL, {
        minZoom: m.minZoom,
        maxZoom: m.maxZoom,
        bounds: m.bounds,
        attribution: '&copy; OpenStreetMap (offline)',
      }).addTo(map);
      localTiles = true;
    } catch {
      // Nessuna mappa offline installata: resta quella online.
    } finally {
      if (!disposed) updateNotice();
    }
  }
  void addLocalTiles();
  window.addEventListener('online', updateNotice);
  window.addEventListener('offline', updateNotice);

  function render(): void {
    const devices = [...store.devices.values()].sort((a, b) => a.device_id.localeCompare(b.device_id));

    const placed = new Set<string>();
    for (const d of devices) {
      if (typeof d.latitude !== 'number' || typeof d.longitude !== 'number') continue;
      placed.add(d.device_id);
      const color = d.sos ? COLORS.sos : d.online ? COLORS.online : COLORS.offline;
      const pos: L.LatLngTuple = [d.latitude, d.longitude];
      const freshness = positionFreshness(d.online, d.position_time ?? d.last_seen, config.FRESH_POSITION_MS);
      const label = d.operator_id !== null ? `${d.device_id} · ${operatorName(d.operator_id).split(' ').pop()}` : d.device_id;
      const popupHtml = `
        <div class="flex flex-col gap-2 p-1 min-w-[140px] text-base-content">
          <div>
            <h3 class="font-bold m-0 text-sm">Gilet ${esc(d.device_id)}</h3>
            <p class="text-xs opacity-70 m-0 leading-tight mt-0.5">${esc(operatorName(d.operator_id))}</p>
            <p class="text-xs m-0 leading-tight mt-0.5 ${freshness.fresh ? 'opacity-70' : 'text-warning font-semibold'}">${esc(freshness.label)}</p>
          </div>
          <div class="flex flex-col gap-1 mt-1">
            <button data-goto-device="${esc(d.device_id)}" class="btn btn-xs btn-primary w-full text-white">Apri Dispositivo</button>
            ${d.operator_id ? `<button data-goto-operator="${esc(d.operator_id.toString())}" class="btn btn-xs btn-outline w-full">Apri Operatore</button>` : ''}
          </div>
        </div>
      `;

      let m = markers.get(d.device_id);
      if (!m) {
        m = L.circleMarker(pos, { radius: 9, weight: 3, color: '#fff', fillOpacity: 1 })
          .bindTooltip('', { permanent: true, direction: 'right', offset: [8, 0] })
          .bindPopup('')
          .addTo(map);
        markers.set(d.device_id, m);
      }
      moveMarker(d.device_id, m, pos);
      // Posizione vecchia: marker semitrasparente, così non sembra quella attuale.
      m.setStyle({ fillColor: color, radius: d.sos ? 12 : 9, fillOpacity: freshness.fresh ? 1 : 0.45, dashArray: freshness.fresh ? undefined : '3 3' });
      const tooltip = esc(label) + (freshness.fresh ? '' : ' (ultima nota)');
      if (m.getTooltip()?.getContent() !== tooltip) m.setTooltipContent(tooltip);
      // Ridisegnare un popup aperto a ogni aggiornamento farebbe perdere i click sui suoi pulsanti.
      if (popups.get(d.device_id) !== popupHtml) {
        popups.set(d.device_id, popupHtml);
        m.setPopupContent(popupHtml);
      }
    }
    for (const [id, m] of markers) {
      if (!placed.has(id)) {
        m.remove();
        markers.delete(id);
        popups.delete(id);
      }
    }

    const visible = new Set<number>();
    for (const z of store.safeZones) {
      if (z.status === 'invalidated' || typeof z.latitude !== 'number' || typeof z.longitude !== 'number') continue;
      visible.add(z.id);
      const style = { color: COLORS.zone, weight: 2, dashArray: z.status === 'proposed' ? '6 6' : undefined, fillOpacity: 0.1 };
      const label = `Zona sicura (${z.status === 'proposed' ? 'proposta' : 'validata'})`;
      let c = zones.get(z.id);
      if (!c) {
        c = L.circle([z.latitude, z.longitude], { radius: z.radius_m, ...style }).bindTooltip(label).addTo(map);
        zones.set(z.id, c);
      } else {
        // La base può spostare o ridimensionare una zona già segnalata.
        c.setLatLng([z.latitude, z.longitude]);
        c.setRadius(z.radius_m);
        c.setStyle(style);
        c.setTooltipContent(label);
      }
    }
    for (const [id, c] of zones) {
      if (!visible.has(id)) {
        c.remove();
        zones.delete(id);
      }
    }

    const proposed = store.safeZones.filter((z) => z.status === 'proposed');
    patch(
      zoneList,
      proposed
        .map((z) => `
          <div data-key="${esc(z.id)}" class="card bg-base-100 border border-base-300 shadow-sm">
            <div class="card-body p-3 gap-2">
              <div class="text-sm"><b>Zona ${esc(z.id)}</b> · Gilet ${esc(z.device_id)}</div>
              <div class="text-xs text-base-content/60">${esc(operatorName(z.operator_id))} · proposta alle ${esc(fmtTime(z.created_at))} · raggio ${esc(z.radius_m)} m</div>
              <div class="flex gap-2">
                <button class="btn btn-xs btn-success flex-1" data-zone-id="${esc(z.id)}" data-zone-status="validated">Valida</button>
                <button class="btn btn-xs btn-ghost flex-1" data-zone-id="${esc(z.id)}" data-zone-status="invalidated">Invalida</button>
              </div>
            </div>
          </div>`)
        .join('') || '<p class="text-xs text-base-content/60">Nessuna zona da valutare.</p>',
    );

    patch(
      list,
      devices
        .map((d) => {
          const border = d.sos ? 'border-error' : 'border-base-300';
          const has = placed.has(d.device_id);
          const freshness = positionFreshness(d.online, d.position_time ?? d.last_seen, config.FRESH_POSITION_MS);
          const selected = d.device_id === store.selectedDevice ? 'ring-2 ring-primary' : '';
          return `
          <button data-key="${esc(d.device_id)}" data-focus="${esc(d.device_id)}" class="card bg-base-100 border ${border} ${selected} shadow-sm text-left ${d.online ? '' : 'opacity-70'}">
            <span class="card-body p-3 gap-0">
              <span class="font-bold text-sm">Gilet ${esc(d.device_id)}${d.online ? '' : ' · offline'}</span>
              <span class="text-xs text-base-content/60">${esc(operatorName(d.operator_id))}</span>
              <span class="text-xs text-base-content/60">${esc(has ? `${coord(d.latitude)}, ${coord(d.longitude)}` : 'posizione non disponibile')}</span>
              ${has ? `<span class="text-xs ${freshness.fresh ? 'text-base-content/60' : 'text-warning font-semibold'}">${esc(freshness.label)}</span>` : ''}
            </span>
          </button>`;
        })
        .join(''),
    );
  }

  root.addEventListener('click', (e) => {
    const t = e.target as Element;
    const btn = t.closest<HTMLElement>('[data-focus]');
    if (btn) {
      store.selectedDevice = btn.dataset.focus!;
      render();
      const m = markers.get(btn.dataset.focus!);
      if (m) map.flyTo(m.getLatLng(), 17);
    }

    const devBtn = t.closest<HTMLElement>('[data-goto-device]');
    if (devBtn) {
      store.selectedDevice = devBtn.dataset.gotoDevice!;
      navigate('/dispositivi');
    }

    const zoneBtn = t.closest<HTMLElement>('[data-zone-id]');
    if (zoneBtn) {
      zoneError.textContent = '';
      const status = zoneBtn.dataset.zoneStatus === 'validated' ? 'validated' : 'invalidated';
      setSafeZoneStatus(Number(zoneBtn.dataset.zoneId), status).catch((err: Error) => {
        zoneError.textContent = `Errore: ${err.message}`;
      });
    }

    const opBtn = t.closest<HTMLElement>('[data-goto-operator]');
    if (opBtn) {
      store.selectedOperator = Number(opBtn.dataset.gotoOperator);
      navigate('/operatori');
    }
  });

  render();
  if (store.mapFocus) {
    store.selectedDevice = store.mapFocus;
    const m = markers.get(store.mapFocus);
    if (m) map.setView(m.getLatLng(), 17);
    store.mapFocus = null;
    render();
  }
  const unsubscribe = subscribe(render);
  return () => {
    disposed = true;
    cancelAnimationFrame(frame);
    unsubscribe();
    window.clearTimeout(sizeTimer);
    window.removeEventListener('online', updateNotice);
    window.removeEventListener('offline', updateNotice);
    map.remove();
  };
}
