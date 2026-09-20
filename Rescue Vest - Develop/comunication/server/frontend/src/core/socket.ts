import { io } from 'socket.io-client';
import type { Socket } from 'socket.io-client';
import { config } from './config';
import { ingestSocketEvent, loadInitial, refreshSafeZones } from './api';
import { notify, setAlert, store } from './store';
import type { AlertUpdate, NiscEvent } from './types';

let socket: Socket | null = null;

function setConnected(value: boolean): void {
  if (store.connected === value) return;
  store.connected = value;
  notify();
}

// A ogni (ri)connessione ricarica lo stato dal server: gli eventi arrivati mentre il browser era
// scollegato sono nel database ma non sono passati dalla socket.
async function resync(): Promise<void> {
  try {
    await loadInitial();
  } catch (err) {
    console.error('Caricamento dati fallito, nuovo tentativo tra 5 s', err);
    window.setTimeout(() => {
      if (socket?.connected) void resync();
    }, 5000);
  }
}

export function connectSocket(): void {
  if (socket) return;
  setConnected(false);
  socket = config.API_BASE ? io(config.API_BASE) : io();

  socket.on('connect', () => {
    setConnected(true);
    void resync();
  });
  socket.on('disconnect', (reason) => {
    setConnected(false);
    // Se è il server a chiudere la connessione (es. riavvio pulito di Flask), socket.io non
    // riprova da solo: senza questa riga la dashboard resterebbe scollegata fino al ricaricamento.
    if (reason === 'io server disconnect') socket?.connect();
  });
  socket.on('connect_error', () => setConnected(false));
  socket.on('alert_updated', (u: AlertUpdate) => {
    if (u && typeof u.device_id === 'string') setAlert(u.device_id, u.alert ?? null);
  });
  socket.on('nisc_event', (ev: NiscEvent) => {
    ingestSocketEvent(ev);
    if (ev?.event_type === 'SAFE_ZONE') void refreshSafeZones();
  });
}
