# Rescue veST — Dashboard della centrale operativa

Applicazione web usata alla **casa base** (la centrale operativa, C4) per seguire in tempo reale una
squadra di soccorso che indossa il gilet sensorizzato **Rescue veST**.

Il coordinatore vede dove si trova ogni operatore, come stanno i suoi parametri (battito, SpO2, gas,
batteria), riceve gli **SOS** con sirena e avviso visivo, li prende in carico, valida le zone sicure
segnalate dal campo e invia messaggi ai gilet.

La dashboard **non parla mai con l'hardware**: legge e scrive solo tramite il server Flask
(backend), che a sua volta riceve i dati dal broker MQTT. Così il passaggio da Wi-Fi a LoRa non
richiede modifiche al frontend.

```
Gilet ──radio──▶ Bridge ──MQTT──▶ Mosquitto ──▶ Flask (API REST + Socket.IO) ──▶ questa dashboard
```

---

## Indice

1. [Cosa fa, pagina per pagina](#1-cosa-fa-pagina-per-pagina)
2. [Come si avvia](#2-come-si-avvia)
3. [Modalità con dati simulati](#3-modalità-con-dati-simulati)
4. [Architettura e struttura del codice](#4-architettura-e-struttura-del-codice)
5. [Collegamento con il backend](#5-collegamento-con-il-backend)
6. [Configurazione](#6-configurazione)
7. [Mappe offline](#7-mappe-offline)
8. [Affidabilità e sicurezza](#8-affidabilità-e-sicurezza)
---

## 1. Cosa fa, pagina per pagina

### Accesso

- Schermata di **login** dell'operatore della base (nome utente e password).
- Il nome di chi è collegato compare in basso nella barra laterale, con il pulsante **Esci**.
- Ogni comando e ogni presa in carico viene registrata dal server con il nome di chi l'ha fatta.
- Se la sessione scade mentre si usa l'app, si torna automaticamente al login.
- In modalità simulata qualunque nome e password sono accettati (lo dice la schermata stessa).

### Elementi sempre visibili

| Elemento | Cosa fa |
|---|---|
| **Barra laterale** | Navigazione tra le 6 pagine; si chiude e si riapre con il pulsante ☰ in alto |
| **Indicatori di stato** (in alto a destra) | Pallini verde / arancione / rosso per Broker MQTT, Bridge, Database, Radio e Server. "Server" diventa rosso appena si perde la connessione in tempo reale |
| **Sirena** | Suona finché c'è almeno un SOS **non ancora preso in carico** e non annullato come falso allarme. Se il browser blocca l'audio compare il pulsante rosso "Attiva audio allarmi" (di norma il login lo sblocca già) |
| **Avviso SOS** | Riquadro rosso in basso a destra su qualunque pagina: gilet, operatore, causa dell'SOS e pulsante "Vai alla dashboard" |
| **Etichetta "Dati simulati"** | Accanto al titolo, **solo** nella versione con dati simulati |

### Dashboard

- **Banner SOS** in cima, uno per ogni gilet in allarme, con operatore, **causa** (gesto SOS,
  caduta rilevata, parametri vitali anomali, gas rilevato) e ultimo contatto. L'allarme ha tre stati:
  1. **Aperto**: banner rosso pulsante, sirena attiva, pulsante **Prendi in carico**.
  2. **Preso in carico**: banner ambra con il nome di chi l'ha preso, sirena spenta, pulsante
     **Chiudi allarme**.
  3. **Chiuso**: il banner sparisce.

  **Falso allarme**: se il gilet annulla l'SOS (evento `SOS_CANCEL`) il banner diventa grigio con la
  scritta "FALSO ALLARME", la sirena si ferma subito ma il banner **resta** finché la base non lo
  chiude, così chi coordina sa che cosa è successo. Se l'SOS è **automatico** il banner dice anche
  quale soglia è stata superata (es. "ossigenazione troppo bassa: 88").

  Se il gilet non è più raggiungibile il banner lo segnala ("GILET NON RAGGIUNGIBILE").
- **Flotta gilet**: una scheda per gilet con operatore, stato (verde online, rosso SOS, grigio
  offline), icona della batteria, SpO2 e battito. Un gilet offline mostra da quanto non trasmette.
  I valori **fuori soglia** (vedi [§6](#6-configurazione)) diventano arancioni o rossi e sotto la
  scheda compare "Fuori soglia: …". Se il gilet segnala un sensore guasto compare "Sensore guasto: …". Cliccando la scheda si apre il
  dettaglio in *Dispositivi*.
- **Ultimi eventi**: gli ultimi 5 messaggi ricevuti, con ora, tipo e riassunto.
- **Comandi rapidi**: invio di un messaggio a un gilet o a tutti.
  - Messaggi predefiniti: *Rientra alla base*, *Mantieni la posizione*, *Conferma il tuo stato*.
  - **Messaggio personalizzato**: testo libero fino a 80 caratteri, con contatore. È pensato per la
    futura app dell'operatore: il gilet di oggi non ha uno schermo per mostrarlo.
  - Dopo l'invio compare "Comando inviato da *nome*".

### Mappa live

- Mappa con un **marker per ogni gilet**, etichettato con numero e cognome (es. "01 · Rossi").
  Colori: verde online, rosso SOS (più grande), grigio offline.
- **Movimento fluido**: tra una posizione e la successiva il marker scivola invece di saltare.
  L'animazione arriva sempre alla posizione reale ricevuta e dura al massimo 5 secondi.
- **Posizione recente o ultima nota**: nell'elenco a sinistra ogni gilet mostra "posizione
  aggiornata *Xs fa*" oppure, in arancione, "ultima posizione nota · *12 min fa*". I marker con
  posizione vecchia sono semitrasparenti e riportano "(ultima nota)", così non vengono scambiati
  per posizioni attuali.
- Cliccando un gilet nell'elenco la mappa lo inquadra; cliccando un marker si apre un riquadro con
  i pulsanti **Apri Dispositivo** e **Apri Operatore**.
- **Zone sicure**: cerchi blu, tratteggiati se solo proposte, continui se validate.
- **Zone sicure proposte**: pannello con le segnalazioni arrivate dal campo, ciascuna con
  **Valida** e **Invalida**. Una zona invalidata scompare dalla mappa.
- **Mappa offline**: se sono installate le tessere della zona (vedi [§7](#7-mappe-offline)) la
  mappa funziona senza Internet. Se non ci sono e la connessione cade, un avviso spiega che lo
  sfondo non è disponibile (posizioni e zone restano visibili).

### Storico eventi

- Tabella di tutti gli eventi ricevuti, dal più recente: ora, gilet, tipo, priorità (con colore) e
  riassunto leggibile.
- Filtri per **gilet** e per **tipo** (SOS, SOS_CANCEL, TELEMETRY, POSITION, SAFE_ZONE, STATUS).
- Paginazione da 12 righe; la prima pagina si aggiorna da sola quando arrivano eventi nuovi.

### Dispositivi

- Elenco dei gilet a sinistra e dettaglio del gilet selezionato a destra.
- Dettaglio: operatore, firmware, ultimo contatto, stato (SOS / online / offline), **stato di ogni
  sensore** (GPS, SpO2/battito, gas, gesti: "ok" o "guasto").
- Valori attuali: batteria, SpO2, frequenza cardiaca, gas (valore grezzo del sensore MQ-2, non in
  ppm), segnale radio (solo se il bridge lo fornisce).
- Icona grande della batteria e **eventi recenti di quel gilet**, caricati dal server.

### Operatori

- Elenco della squadra con ruolo e gilet assegnato.
- Scheda dell'operatore: ruolo, stato (in missione / in base), eventuale SOS in corso, gilet
  assegnato, missione corrente, data di ingresso in squadra, **missioni recenti** ed **eventi**
  del suo gilet.
- **Mostra sulla mappa**: apre la mappa centrata su quell'operatore e lo evidenzia nell'elenco.
- **Tracciato PPG**: il grafico mostrato quando il gilet è online è **illustrativo**. È generato a
  partire dal numero del gilet e non dai dati del sensore.

### Assistente AI

- Chat in cui si fanno domande sui dati in linguaggio naturale (es. *"Quanti SOS ci sono stati?"*).
- Ogni risposta mostra anche la **query SQL** usata (apribile) e, se presente, la **tabella dei
  risultati**: il coordinatore può verificare da dove viene la risposta.
- La conversazione resta anche cambiando pagina; una risposta arrivata mentre si era altrove
  compare al ritorno.
- Con il server collegato le domande vanno al backend (`POST /api/ai/query`), che le esegue con un
  utente del database **in sola lettura**. In modalità simulata risponde solo a domande su SOS e
  batteria.

---

## 2. Come si avvia

Requisiti: **Node.js 22 o superiore** (sviluppato con Node 24) e npm.

```bash
cd frontend
npm install          # solo la prima volta
```

La dashboard funziona in due modi. Sono la **stessa applicazione**: cambia solo da dove arrivano i
dati.

| | A · Dati finti | B · Collegata al server |
|---|---|---|
| Da dove vengono i dati | da `src/core/mock.ts`, dentro il browser | dal server Flask, via API REST e Socket.IO |
| Serve il backend acceso? | no | sì |
| Serve l'hardware (gilet, radio, broker)? | no | no: basta il server |
| Come si riconosce | etichetta **"Dati simulati"** accanto al titolo | nessuna etichetta |
| A cosa serve | sviluppare, provare la grafica, mostrare il progetto senza attrezzatura | provare il sistema vero, e la dimostrazione finale |

### A · Con i dati finti

```bash
cd frontend
npm run dev                 # si apre su http://localhost:5173
```

Nome utente e password qualsiasi. Parte da sola una squadra di quattro soccorritori che cammina,
trasmette parametri e fa scattare SOS: lo scenario è descritto in [§3](#3-modalità-con-dati-simulati).
Nessuna richiesta esce dal browser.

### B · Collegata al server Flask

Due possibilità, a seconda di cosa stai facendo.

**Mentre sviluppi** — la pagina si aggiorna da sola a ogni modifica del codice:

```bash
# PowerShell
$env:VITE_USE_MOCK='false'; npm run dev
# bash
VITE_USE_MOCK=false npm run dev
```

Si apre sempre su `http://localhost:5173`, ma le richieste `/api/...` e `/socket.io` vengono
inoltrate da Vite a Flask su `http://localhost:5000`. Per il browser partono e arrivano allo stesso
indirizzo, quindi **al backend non serve abilitare CORS**. Se Flask usa un'altra porta si cambia in
`vite.config.ts`.

**Per la dimostrazione** — un solo indirizzo, nessun server di sviluppo acceso:

```bash
npm run build               # crea frontend/dist/
```

`npm run build` produce **sempre** la versione collegata al server (lo impone `.env.production`): i
dati finti non vengono nemmeno inclusi nei file generati, quindi non c'è il rischio di mostrare
numeri inventati durante la presentazione.

Poi è **Flask a servire `frontend/dist/`** sulla porta 5000, insieme alle sue API: si apre
`http://localhost:5000` e c'è tutto. Flask deve restituire `index.html` per ogni percorso che non è
un file e non è un'API, altrimenti ricaricando la pagina su `/mappa` si ottiene un 404 (la
dashboard è una sola pagina che cambia contenuto da sola).

`dist/` si rigenera a ogni build e non viene pubblicata su GitHub: chi la vuole la crea con un
comando.

**Se il server non risponde**, la dashboard non si blocca: l'indicatore "Server" diventa rosso, e
appena il server torna ricarica i dati da sola.

**Per provare il modo B senza avere Flask pronto** c'è un finto backend che risponde secondo il
contratto (`strumenti_test/finto_flask.mjs`): vedi [Test automatici](#test-automatici).

### Test automatici

In `frontend/strumenti_test/` ci sono i test end-to-end: pilotano Microsoft Edge e controllano la
dashboard come farebbe una persona. C'è anche `finto_flask.mjs`, un finto backend che risponde
secondo il contratto, per provare la versione reale senza avere il server Flask acceso.

```bash
cd frontend
npm test                    # tutti i controlli
npm test -- allarmi         # solo soglie di allarme e falso allarme
```

Non serve preparare niente: il comando avvia da solo i server che gli servono, esegue i controlli
e alla fine li spegne. Stampa l'elenco dei controlli con `ok` o `FALLITO`, il totale e l'esito
finale; se qualcosa fallisce stampa anche il rapporto completo.

```
  ok   presa in carico: cambia il banner e non chiude l'allarme
  ok   offline: solo il gilet silenzioso passa offline
  ...
31/31 controlli superati
ESITO: OK
```

Serve **Microsoft Edge o Google Chrome**: vengono cercati nelle posizioni abituali, altrimenti si
indica il percorso con la variabile `EDGE_PATH`.

```bash
# PowerShell
$env:EDGE_PATH = 'C:\percorso\msedge.exe'; npm test
```

Il finto backend si può usare anche da solo, per provare la versione reale senza Flask:

```bash
cd frontend/strumenti_test
npm run finto-flask         # risponde sulla porta 5000 (utente "coord", password "pass")
```

---

## 3. Modalità con dati simulati

Serve a mostrare e provare la dashboard senza gilet né server. Tutto è in
`src/core/mock.ts`, che non entra nella versione reale.

Ogni soccorritore segue un **percorso di ricerca** a passo d'uomo (1,3–1,6 m/s) e il gilet
trasmette come quello vero: posizione ogni 3 s, parametri ogni 6 s, STATUS ogni 12 s. Il battito
sale quando l'operatore cammina (~100) e scende quando è fermo (~76); la batteria cala lentamente.

| Gilet | Operatore | Comportamento |
|---|---|---|
| 01 | Mario Rossi | Pattuglia a rettangolo |
| 02 | Luca Bianchi | Avanti e indietro tra due strade. **Sensore dei gesti guasto** |
| 03 | Sara Verdi | Ricerca "a pettine"; attraversa una zona con gas |
| 04 | non assegnato | Spento da 12 minuti (resta offline) |
| 05 | Elena Neri | Giro ad anello |

**Scenario a tempo** (secondi dall'accesso):

| Quando | Cosa succede |
|---|---|
| 8 s | Il Gilet 01 fa il **gesto SOS** e si ferma |
| 20 s | Il Gilet 05 **propone una zona sicura** |
| 30 → 42 s | Il Gilet 02 fa partire un SOS per sbaglio e poi lo **annulla** (SOS_CANCEL): il banner diventa grigio "FALSO ALLARME", la sirena tace, resta da chiudere |
| ~43 s | Il Gilet 03 attraversa la zona del gas: superata la soglia per 10 s parte da solo un **SOS automatico "gas rilevato"** |
| 60–110 s | Il Gilet 02 **perde il segnale**: dopo 30 s di silenzio risulta offline, poi rientra |
| ~105 s | L'ossigenazione del Gilet 05 scende sotto il 91 %: dopo 30 s sotto soglia parte un **SOS automatico "parametri vitali anomali"** |

---

## 4. Architettura e struttura del codice

È una **applicazione a pagina singola** scritta in **TypeScript senza framework** (niente React,
Vue o Next.js), costruita con **Vite** e stilizzata con **Tailwind CSS + DaisyUI**. La mappa usa
**Leaflet**, il tempo reale **Socket.IO**.

```
frontend/
├── index.html              pagina unica che carica l'app
├── vite.config.ts          proxy verso Flask (/api, /socket.io) e cartella di build
├── .env.production         la build è sempre la versione reale (VITE_USE_MOCK=false)
├── strumenti_test/         test end-to-end e finto backend (vedi §2)
├── dist/                   versione compilata, creata da `npm run build` (non su GitHub)
└── src/
    ├── main.ts             login, guscio dell'app (barra laterale, intestazione, avvisi), avvio
    ├── style.css           tema "rescue", stile liquid glass, animazioni
    ├── core/
    │   ├── config.ts       parametri modificabili (vedi §6)
    │   ├── types.ts        tipi TypeScript dei dati scambiati con il backend
    │   ├── store.ts        stato condiviso: gilet, eventi, zone, utente; notifica le pagine
    │   ├── api.ts          chiamate REST al backend (+ risposte simulate)
    │   ├── socket.ts       connessione Socket.IO unica per tutta la sessione
    │   ├── router.ts       cambio pagina senza ricaricare
    │   ├── dom.ts          aggiornamento dell'HTML senza distruggere gli elementi
    │   ├── alarm.ts        sirena degli SOS (Web Audio)
    │   ├── util.ts         formattazione, escape dell'HTML, etichette leggibili
    │   └── mock.ts         simulazione (solo sviluppo)
    └── pages/              una pagina per file: dashboard, mappa, eventi,
                            dispositivi, operatori, assistente
```

### Come scorrono i dati

```
 server ──REST (caricamento iniziale)──┐
                                       ▼
 server ──Socket.IO "nisc_event"────▶ store ──notifica──▶ pagina visibile ──▶ HTML
          "alert_updated"               ▲
                                        │
 mock.ts (solo in simulazione) ─────────┘
```

1. **Dopo il login** l'app apre la connessione Socket.IO. A ogni connessione, anche dopo una caduta
   di rete, **ricarica tutto** dal server (gilet, operatori, zone, ultimi 100 eventi), così non si
   perdono gli eventi arrivati mentre si era scollegati.
2. Ogni evento ricevuto passa da `store.applyEvent`, che lo **valida**, scarta i **duplicati**
   (stesso gilet e stesso numero di sequenza: un SOS ripetuto via radio apre un solo allarme) e
   aggiorna lo stato del gilet.
3. Lo store **notifica** le pagine, che si ridisegnano.
4. Ogni 5 secondi un controllo segna **offline** i gilet che non trasmettono da più di 30 secondi e
   aggiorna i tempi mostrati ("12s fa").

### Pagine e cambio pagina

- Ogni pagina esporta `mount(elemento)`, che disegna la pagina e restituisce una funzione di pulizia
  (timer, iscrizioni, mappa). Il router chiama la pulizia della pagina precedente prima di montare la
  nuova.
- La navigazione cambia l'indirizzo senza ricaricare (`history.pushState`): la connessione in tempo
  reale resta aperta e un SOS non va mai perso durante il cambio di pagina.
- Una navigazione più recente annulla quelle ancora in caricamento (evita di mostrare la Mappa con
  l'indirizzo di Eventi se si clicca rapidamente). Un percorso sconosciuto porta alla Dashboard.

---

## 5. Collegamento con il backend

Il formato completo dei dati è definito nel documento del backend (`Guida_Backend.docx`,
sezioni 5–7). Questa è la parte usata dal frontend.

### API REST

| Metodo e percorso | Uso nel frontend |
|---|---|
| `POST /api/login` · `GET /api/me` · `POST /api/logout` | Accesso, controllo della sessione all'avvio, uscita |
| `GET /api/devices` | Gilet e loro stato (compreso l'allarme aperto) |
| `GET /api/operators` | Squadra e missioni |
| `GET /api/events?device_id=&event_type=&limit=&offset=` | Storico eventi con filtri e pagine |
| `GET /api/devices/{id}/events` | Eventi recenti di un gilet (pagina Dispositivi) |
| `GET /api/safe-zones` · `POST /api/safe-zones/{id}` | Zone sicure; validazione o invalidazione |
| `POST /api/devices/{id}/commands` | Invio di un comando (`{id}` = `broadcast` per tutti i gilet) |
| `POST /api/alerts/{id}/acknowledge` · `POST /api/alerts/{id}/close` | Presa in carico e chiusura di un allarme |
| `GET /api/health` | Indicatori di stato (letto ogni 10 s) |
| `POST /api/ai/query` | Domande all'assistente |

### Eventi Socket.IO ricevuti

| Evento | Contenuto |
|---|---|
| `nisc_event` | Ogni messaggio valido di un gilet (SOS, SOS_CANCEL, TELEMETRY, POSITION, SAFE_ZONE, STATUS), nel formato `nisc.event.v1` |
| `alert_updated` | Un allarme aperto, preso in carico o chiuso: `{ device_id, alert }` |

### Autenticazione

- Sessione tramite **cookie** gestito dal server.
- Ogni richiesta `POST` (tranne il login) invia l'intestazione **`X-CSRF-Token`** con il valore
  restituito da login e `/api/me`.
- Una risposta **401** durante l'uso riporta al login.

---

## 6. Configurazione

In `src/core/config.ts`:

| Parametro | Valore | Significato |
|---|---|---|
| `USE_MOCK` | da `VITE_USE_MOCK` | Dati simulati sì/no (sì di default in sviluppo, no nella build) |
| `API_BASE` | `''` | Stesso indirizzo della pagina: in sviluppo ci pensa il proxy di Vite |
| `OFFLINE_AFTER_MS` | 30 000 | Dopo quanto silenzio un gilet è mostrato offline. Il gilet deve inviare STATUS almeno ogni 10–15 s |
| `FRESH_POSITION_MS` | 60 000 | Oltre questo tempo la posizione è mostrata come "ultima nota" |
| `LOCAL_TILES_URL` · `LOCAL_TILES_MANIFEST` | `/tiles/...` | Dove cercare le mappe offline |
| `CUSTOM_MESSAGE` | codice 0, max 80 caratteri | Messaggio a testo libero |
| `COMMAND_PRESETS` | codici 12, 13, 14 | Messaggi predefiniti (il codice è quello che riceve il firmware del gilet) |
| `THRESHOLDS` | vedi sotto | Soglie di allarme automatico |

L'indirizzo di Flask per lo sviluppo (`http://localhost:5000`) si cambia in `vite.config.ts`.

### Soglie di allarme (`THRESHOLDS`)

| Valore | Attenzione (arancione) | Pericolo (rosso) | Da dove viene |
|---|---|---|---|
| SpO2 | 92–95 % | ≤ 91 % | **NEWS2** (National Early Warning Score 2, Royal College of Physicians 2017) |
| Frequenza cardiaca | ≤ 50 o ≥ 111 bpm | ≤ 40 o ≥ 131 bpm | NEWS2; l'attenzione è alzata a 111 perché chi cammina sta già sui 100 bpm |
| Gas (MQ-2 grezzo) | ≥ 2200 | ≥ 2600 | **Provvisorio**: l'MQ-2 non ha una soglia standard, va calibrato sul sensore vero |
| Batteria | ≤ 20 % | ≤ 10 % | Scelta operativa |

Un valore deve restare oltre soglia per **30 s** (10 s per il gas) prima di far scattare un allarme:
una singola lettura sbagliata non deve allarmare la base.

Sono **valori di allerta, non una diagnosi**: vanno confermati con un referente sanitario prima di
usarli sul campo.

La dashboard usa queste soglie per **colorare i valori e segnalare "Fuori soglia"**. L'allarme vero
e proprio (l'SOS) lo apre chi rileva il superamento — il gilet o il server — e la dashboard mostra
il motivo che riceve nei campi `kind` e `reason` dell'evento.

---

## 7. Mappe offline

Per usare la mappa senza Internet, mettere le tessere della zona in `public/tiles/`:

```
public/tiles/
├── tiles.json           { "minZoom": 12, "maxZoom": 18, "bounds": [[40.80, 14.20], [40.90, 14.33]] }
└── {z}/{x}/{y}.png      tessere nel formato XYZ standard
```

- Se `tiles.json` esiste, la mappa usa le tessere locali sopra quelle online; altrimenti usa quelle
  di OpenStreetMap, che richiedono Internet.
- Con `npm run build` le tessere finiscono in `frontend/dist/tiles/` e Flask le serve senza codice
  aggiuntivo.
- **Non** scaricare le tessere in blocco dai server di OpenStreetMap: lo vieta il loro regolamento.
  Si generano dai dati OSM, per esempio con QGIS ("Genera tessere XYZ") partendo da un estratto
  della regione scaricato da Geofabrik.

---

## 8. Affidabilità e sicurezza

- **Dati esterni trattati come non affidabili**: ogni valore inserito nella pagina passa da
  `esc()` (niente HTML iniettato da un messaggio malformato); gli eventi senza campi obbligatori
  vengono scartati; coordinate e numeri non validi non vengono usati.
- **Un errore non blocca il resto**: se l'aggiornamento di una pagina fallisce, le altre continuano
  a ricevere i dati.
- **Click e focus non si perdono**: le parti che si aggiornano ogni pochi secondi usano
  `patch()` (`src/core/dom.ts`), che modifica solo testo e attributi cambiati invece di ricreare
  gli elementi. Un click su "Prendi in carico" durante un aggiornamento arriva sempre a
  destinazione.
- **Riconnessione automatica**: se il server cade la dashboard lo segnala (pallino "Server" rosso),
  si ricollega da sola e ricarica lo stato.
- **Richieste contenute**: lo Storico si aggiorna al massimo una volta al secondo; le risposte
  arrivate fuori ordine vengono ignorate.
- **Accesso protetto**: login, cookie di sessione, token CSRF, nessuna apertura CORS.

---
