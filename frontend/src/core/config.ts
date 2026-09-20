export const config = {
  // Dati finti di default. Per usare il server vero: avviare con VITE_USE_MOCK=false.
  USE_MOCK: import.meta.env.VITE_USE_MOCK !== 'false',
  // Vuoto = stesso indirizzo della pagina. In sviluppo Vite inoltra /api e /socket.io a Flask
  // (vite.config.ts), quindi al backend non serve abilitare CORS.
  API_BASE: '',
  // Un gilet che non trasmette da più di questo tempo viene mostrato offline. Il gilet deve
  // mandare STATUS almeno ogni 10–15 s (da scrivere nel firmware).
  OFFLINE_AFTER_MS: 30_000,
  // Oltre questo tempo la mappa mostra la posizione come "ultima nota", non come attuale.
  // Da allineare alla frequenza di invio del GPS, ancora da decidere.
  FRESH_POSITION_MS: 60_000,
  // Mappe offline: tessere copiate in public/tiles/ (vedi tiles.json). Se mancano si usano
  // quelle online di OpenStreetMap, che richiedono Internet.
  LOCAL_TILES_URL: '/tiles/{z}/{x}/{y}.png',
  LOCAL_TILES_MANIFEST: '/tiles/tiles.json',
};

// Soglie di allarme automatico (guida §9.4). Battito e SpO2 vengono dal NEWS2 (National Early
// Warning Score 2, Royal College of Physicians 2017), la scala usata negli ospedali per
// riconoscere un paziente che sta peggiorando:
//   SpO2  <=91 = punteggio 3 (rischio massimo), 92-95 = punteggio 1-2.
//   Battito <=40 o >=131 = punteggio 3, 91-130 = punteggio 1-2.
// La soglia di attenzione del battito è alzata a 111 perché un soccorritore che cammina sta già
// intorno ai 100 bpm e altrimenti sarebbe sempre in allarme.
// Il gas non ha una soglia standard: l'MQ-2 dà un valore grezzo che dipende dalla calibrazione,
// quindi questi due numeri sono provvisori e vanno tarati sul sensore vero.
// ATTENZIONE: sono valori di allerta, non una diagnosi. Vanno confermati con il referente sanitario
// del team e concordati con Martina (guida §8: il controllo può stare sul gilet, alla base o su
// entrambi; qui la base li usa solo per evidenziare i valori e per la simulazione).
export const THRESHOLDS = {
  spo2: { danger: 91, warning: 95 }, // percentuale, si allarma sotto
  heart_rate: { dangerLow: 40, warningLow: 50, warningHigh: 111, dangerHigh: 131 }, // bpm
  mq2_raw: { warning: 2200, danger: 2600 }, // valore grezzo ADC, da calibrare
  battery: { warning: 20, danger: 10 }, // percentuale
  // Un valore deve restare oltre soglia per questo tempo prima di far scattare l'allarme:
  // una singola lettura sbagliata del sensore non deve allarmare la base.
  SUSTAINED_MS: 30_000,
  // Il gas è un pericolo immediato: si aspetta molto meno prima di lanciare l'SOS.
  GAS_SUSTAINED_MS: 10_000,
};

// Messaggio a testo libero: codice 0 riservato e lunghezza massima, da concordare con Martina e
// col firmware (il frame LoRa ha un payload di pochi byte).
export const CUSTOM_MESSAGE = { id: 'custom', code: 0, maxLength: 80 };

// Codici da concordare con il firmware del gilet (campo payload.code del comando).
export const COMMAND_PRESETS = [
  { id: 'return_to_base', code: 12, label: 'Rientra alla base' },
  { id: 'hold_position', code: 13, label: 'Mantieni la posizione' },
  { id: 'check_in', code: 14, label: 'Conferma il tuo stato' },
];
