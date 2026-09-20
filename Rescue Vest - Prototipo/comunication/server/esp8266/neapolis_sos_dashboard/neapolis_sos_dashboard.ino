/*
 * Neapolis 2026 - Dashboard web MQTT per ESP8266
 *
 * Usa un secondo ESP8266: questo sketch sostituisce ESP-AT. Il modem collegato
 * alla Nucleo deve invece conservare lo sketch tcp_bridge/firmware ESP-AT.
 *
 * Dipendenze:
 *   - ESP8266 by ESP8266 Community
 *   - PubSubClient by Nick O'Leary
 *
 * Il JSON non viene decodificato sul microcontrollore: viene conservato in una
 * coda circolare e interpretato dal browser. In questo modo non serve
 * ArduinoJson e la RAM dell'ESP resta sotto controllo.
 */

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/*
 * Configurazione autonoma della dashboard. Copiare qui i valori della
 * configurazione ChibiOS funzionante, mantenendo un Client ID diverso.
 */
#define APP_WIFI_SSID                  "RESCUE_VEST_WIFI_ESEMPIO"
#define APP_WIFI_PASSWORD              "PASSWORD_WIFI_DI_ESEMPIO"
#define APP_MQTT_BROKER_HOST           "10.162.161.51"
#define APP_MQTT_BROKER_PORT           1883U
#define APP_MQTT_USERNAME              ""
#define APP_MQTT_PASSWORD              ""
#define APP_MQTT_DASHBOARD_CLIENT_ID   "neapolis-dashboard-01"
#define APP_MQTT_SOS_TOPIC             "neapolis/2026/sos/requests"
#define APP_MQTT_TELEMETRY_TOPIC       "neapolis/2026/operators/telemetry"
#define APP_MQTT_STATUS_TOPIC          "neapolis/2026/operators/status"

static const size_t HISTORY_CAPACITY = 20U;
static const size_t MQTT_BUFFER_SIZE = 1024U;

struct MqttEntry {
  String topic;
  String payload;
  unsigned long receivedAtMs;
  uint32_t number;
};

WiFiClient tcpClient;
PubSubClient mqttClient(tcpClient);
ESP8266WebServer webServer(80);
MqttEntry historyEntries[HISTORY_CAPACITY];
size_t historyCount = 0U;
size_t nextHistorySlot = 0U;
uint32_t nextMessageNumber = 1U;
unsigned long lastMqttAttemptMs = 0UL;
wl_status_t previousWifiStatus = WL_IDLE_STATUS;

const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Neapolis 2026 · Operations Map</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif}*{box-sizing:border-box}body{margin:0;display:grid;grid-template-columns:360px 1fr;height:100vh;background:#071018;color:#eef5f4;overflow:hidden}aside{padding:22px;background:#0b171e;border-right:1px solid #29404b;overflow:auto}h1{margin:.3rem 0;font-size:1.65rem}.eyebrow{font-size:.68rem;letter-spacing:.2em;color:#52df9b;font-weight:800}.muted{color:#8197a1}.connection{display:flex;align-items:center;gap:8px;padding:16px 0}.connection small{margin-left:auto}.dot{width:9px;height:9px;border-radius:50%;background:#ef5350}.dot.on{background:#52df9b;box-shadow:0 0 10px #52df9b}select{width:100%;padding:10px;background:#13262f;color:#eef5f4;border:1px solid #2b4652;border-radius:8px}.metrics{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:14px 0 22px}.metric{padding:11px;background:#11222a;border:1px solid #213942;border-radius:9px}.metric.wide{grid-column:1/-1}.metric small,.event span{display:block;color:#8197a1;font-size:.7rem}.metric strong{display:block;margin-top:4px}.danger{color:#ff6767}.section-title{display:flex;justify-content:space-between;align-items:center}.events{display:grid;gap:8px}.event{padding:9px;border-left:3px solid #4c7486;background:#102029}.event.sos{border-color:#ff5757;background:#281719}.event.safe{border-color:#52df9b}main{position:relative}#map{height:100%}.legend{position:absolute;z-index:500;right:15px;bottom:15px;padding:9px;background:#081219dd;border:1px solid #29404b;border-radius:8px;font-size:.72rem}.pin{width:22px;height:22px;border:3px solid white;border-radius:50%;background:#34a8ff;box-shadow:0 2px 12px #0009}.pin.sos{background:#ff3d45;animation:pulse 1.2s infinite}@keyframes pulse{50%{box-shadow:0 0 0 10px #ff3d4544}}@media(max-width:760px){body{display:block;height:auto;overflow:auto}main{height:52vh;position:fixed;top:0;left:0;right:0}aside{margin-top:52vh;min-height:48vh}}
.help{font-size:.72rem;line-height:1.45;color:#78909b;margin:-5px 0 10px}.operator-list{display:grid;gap:8px;margin-bottom:14px}.operator-card{width:100%;display:grid;grid-template-columns:auto 1fr auto;align-items:center;gap:10px;text-align:left;padding:11px;border:1px solid #26404b;border-radius:9px;background:#10212a;color:#eef5f4;cursor:pointer}.operator-card:hover,.operator-card.active{border-color:#47c99a;background:#15302f}.operator-card.sos{border-color:#ff5757;background:#281719}.operator-card .identity strong,.operator-card .identity small{display:block}.operator-card .identity small{color:#78909b;margin-top:2px}.status-dot{width:9px;height:9px;border-radius:50%;background:#607d87}.status-dot.online{background:#52df9b}.status-dot.sos{background:#ff5757;box-shadow:0 0 8px #ff5757}.operator-card .arrow{color:#78909b;font-size:1.2rem}.operator-details{border:1px solid #28414c;border-radius:10px;background:#0d1d24;margin-bottom:24px}.operator-details[hidden]{display:none}.operator-details summary{cursor:pointer;padding:12px 13px;font-weight:700}.operator-details .metrics{padding:0 10px 10px;margin:0}.group-title{grid-column:1/-1;color:#52df9b;font-size:.68rem;letter-spacing:.12em;text-transform:uppercase;margin-top:7px}.empty-list{padding:16px;border:1px dashed #28414c;border-radius:9px;color:#78909b;text-align:center;font-size:.82rem}
.leaflet-tooltip.operator-label{background:#0d1d24;color:#eef5f4;border:1px solid #3c5965;border-radius:6px;box-shadow:0 2px 8px #0008;font-weight:700;padding:4px 7px}.leaflet-tooltip.operator-label:before{border-top-color:#3c5965}
</style></head><body>
<aside><span class="eyebrow">NEAPOLIS 2026</span><h1>Operations map</h1><div class="muted">Operatori, sensori e richieste di soccorso</div>
<div class="connection"><i id="dot" class="dot"></i><strong id="mqtt">Mosquitto…</strong><small id="count" class="muted">0 messaggi</small></div>
<div class="section-title"><h2>Operatori rilevati</h2><span id="operatorCount" class="muted">0</span></div>
<p class="help">Un operatore è online quando invia telemetria recente. Selezionalo per consultare tutti i dati.</p>
<div id="operatorList" class="operator-list"><div class="empty-list">In attesa di JSON validi…</div></div>
<details id="operatorDetails" class="operator-details" hidden open><summary id="operatorTitle">Dettagli operatore</summary><div id="metrics" class="metrics"></div></details>
<div class="section-title"><h2>Segnalazioni recenti</h2><span id="eventCount" class="muted">0</span></div>
<p class="help">SOS, gesture, cambi di stato ed eventuali payload non validi.</p><div id="events" class="events"></div></aside>
<main><div id="map"></div><div class="legend">● blu operatore &nbsp; ● rosso SOS &nbsp; ■ verde zona sicura</div></main>
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<script>
const labels={sample:'Telemetria periodica',activated:'Operatore attivato',manual_gesture:'SOS volontario',ppg_emergency:'Emergenza PPG',gas_emergency:'Emergenza gas',zone_safe:'Zona sicura',invalid_payload:'Payload JSON non valido'};
const ONLINE_MS=30000;
const map=L.map('map').setView([40.8518,14.2681],16);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:20,attribution:'&copy; OpenStreetMap'}).addTo(map);
const state={operators:new Map(),events:[],seen:new Set(),zones:new Map(),valid:0,invalid:0};
const markers=new Map(),paths=new Map();let selected='',fitted=false;
const $=id=>document.getElementById(id);
const num=(v,d=0)=>Number.isFinite(Number(v))?Number(v):d;
const yesNo=v=>v?'Sì':'No';
function gpsOf(p){const raw=p.gps||{},lat=num(raw.latitudeE7)/1e7,lon=num(raw.longitudeE7)/1e7;return{...raw,valid:!!raw.valid&&lat>=-90&&lat<=90&&lon>=-180&&lon<=180,lat,lon}}
function addSafeZone(op,g){if(!g.valid)return;if(!op.origin)op.origin=[g.lat,g.lon];const k=111320*Math.cos(op.origin[0]*Math.PI/180),x=(g.lon-op.origin[1])*k,y=(g.lat-op.origin[0])*110540,cx=Math.floor(x/5),cy=Math.floor(y/5),id=op.id+':'+cx+':'+cy;if(state.zones.has(id))return;const x0=cx*5,y0=cy*5,corners=[[op.origin[0]+y0/110540,op.origin[1]+x0/k],[op.origin[0]+(y0+5)/110540,op.origin[1]+x0/k],[op.origin[0]+(y0+5)/110540,op.origin[1]+(x0+5)/k],[op.origin[0]+y0/110540,op.origin[1]+(x0+5)/k]];state.zones.set(id,L.polygon(corners,{color:'#32c982',fillColor:'#42e49a',fillOpacity:.4,weight:2}).addTo(map).bindTooltip('Zona sicura · '+op.id))}
function invalidEvent(){state.invalid++;state.events.unshift({device:'MQTT',event:'invalid_payload',alarm:false,time:new Date().toLocaleTimeString(),detail:'Il messaggio non rispetta schemaVersion 1'});if(state.events.length>30)state.events.pop()}
function ingest(entry){
  if(state.seen.has(entry.number))return;state.seen.add(entry.number);
  let p;try{p=JSON.parse(entry.payload)}catch(e){invalidEvent();return}
  if(p.schemaVersion!==1||typeof p.deviceId!=='string'||!p.deviceId.trim()){invalidEvent();return}
  state.valid++;
  const id=p.deviceId.trim();let op=state.operators.get(id);
  if(!op){op={id,path:[],alarmUntil:0,activated:false,lastAction:'Nessuna'};state.operators.set(id,op)}
  const g=gpsOf(p),ppg=p.ppg||{},mq2=p.mq2||{},event=p.event||'sample',alarm=p.type==='sos'||!!ppg.emergency||!!mq2.emergency,now=Date.now();
  if(g.valid)op.lastValidGps=g;
  if(event!=='sample')op.lastAction=labels[event]||event;
  Object.assign(op,{gps:g,ppg,mq2,event,type:p.type||'non definito',eventId:p.eventId||'non definito',sequence:num(p.sequence),uptimeMs:num(p.uptimeMs),lastSeen:new Date().toLocaleTimeString(),lastReceivedMs:now,activated:op.activated||event==='activated'});
  if(g.valid){if(!op.origin)op.origin=[g.lat,g.lon];const last=op.path[op.path.length-1];if(!last||last[0]!==g.lat||last[1]!==g.lon){op.path.push([g.lat,g.lon,num(ppg.bpm)]);if(op.path.length>100)op.path.shift()}}
  if(alarm)op.alarmUntil=now+60000;if(event==='zone_safe')addSafeZone(op,g);
  if(p.type!=='telemetry'||event!=='sample'||alarm){state.events.unshift({device:id,event,alarm,bpm:num(ppg.bpm),mq2:num(mq2.rawAdc),time:op.lastSeen});if(state.events.length>30)state.events.pop()}
}
function icon(alarm){return L.divIcon({className:'',html:'<div class="pin '+(alarm?'sos':'')+'"></div>',iconSize:[22,22],iconAnchor:[11,11]})}
function metric(name,value,wide=false,danger=false){const d=document.createElement('div');d.className='metric'+(wide?' wide':'');const s=document.createElement('small'),b=document.createElement('strong');s.textContent=name;b.textContent=String(value);if(danger)b.className='danger';d.append(s,b);return d}
function group(title){const d=document.createElement('div');d.className='group-title';d.textContent=title;return d}
function panel(op){
  const details=$('operatorDetails'),box=$('metrics');box.replaceChildren();
  if(!op){details.hidden=true;return}details.hidden=false;$('operatorTitle').textContent='Dettagli · '+op.id;
  const online=Date.now()-op.lastReceivedMs<ONLINE_MS,alarm=op.alarmUntil>Date.now(),g=op.gps||{},p=op.ppg||{},m=op.mq2||{};
  box.append(group('Stato operativo'),metric('Connessione stimata',online?'Online':'Nessun dato recente',false,!online),metric('Allarme',alarm?'SOS ATTIVO':'Nessuno',false,alarm),metric('Attivazione',op.activated?'Confermata':'Non ricevuta'),metric('Ultima azione',op.lastAction,true));
  box.append(group('PPG'),metric('BPM',p.valid?num(p.bpm):'Non valido',false,!!p.emergency),metric('Emergenza',yesNo(!!p.emergency),false,!!p.emergency),metric('Red raw',num(p.redRaw)),metric('Infrared raw',num(p.infraredRaw)),metric('Fresh',yesNo(!!p.fresh)),metric('Status',num(p.status)));
  box.append(group('MQ2'),metric('ADC grezzo',m.valid?num(m.rawAdc):'Non valido',false,!!m.emergency),metric('Emergenza',yesNo(!!m.emergency),false,!!m.emergency),metric('Fresh',yesNo(!!m.fresh)),metric('Status',num(m.status)));
  const shownGps=g.valid?g:op.lastValidGps;
  box.append(group('GPS'),metric('Validità',g.valid?'Fix valido':shownGps?'Ultima posizione valida':'Fix non valido',false,!g.valid),metric('Fresh',yesNo(!!g.fresh)),metric('Latitudine',shownGps?shownGps.lat.toFixed(7):'—'),metric('Longitudine',shownGps?shownGps.lon.toFixed(7):'—'),metric('Velocità',num(g.speedMilliKnots)/1000+' kn'),metric('Direzione',num(g.headingMilliDegrees)/1000+'°'));
  box.append(group('Messaggio'),metric('Tipo',op.type),metric('Evento',labels[op.event]||op.event),metric('Sequenza',op.sequence),metric('Uptime',Math.floor(op.uptimeMs/1000)+' s'),metric('Event ID',op.eventId,true),metric('Ricevuto alle',op.lastSeen,true));
}
function selectOperator(op){selected=op.id;$('operatorDetails').open=true;const g=op.gps?.valid?op.gps:op.lastValidGps;if(g)map.panTo([g.lat,g.lon]);renderOperators();panel(op)}
function renderOperators(){
  const list=$('operatorList'),ops=[...state.operators.values()];list.replaceChildren();$('operatorCount').textContent=ops.length;
  if(!ops.length){const empty=document.createElement('div');empty.className='empty-list';empty.textContent=state.invalid?'Nessun operatore: controllare i payload non validi sotto.':'In attesa di JSON validi…';list.append(empty);return}
  for(const op of ops){const online=Date.now()-op.lastReceivedMs<ONLINE_MS,alarm=op.alarmUntil>Date.now(),button=document.createElement('button');button.type='button';button.className='operator-card'+(op.id===selected?' active':'')+(alarm?' sos':'');const dot=document.createElement('i');dot.className='status-dot '+(alarm?'sos':online?'online':'');const identity=document.createElement('span');identity.className='identity';const name=document.createElement('strong'),status=document.createElement('small'),arrow=document.createElement('span');name.textContent=op.id;status.textContent=alarm?'SOS attivo':online?'Online · dati recenti':'Nessun dato recente';arrow.className='arrow';arrow.textContent='›';identity.append(name,status);button.append(dot,identity,arrow);button.addEventListener('click',()=>selectOperator(op));list.append(button)}
}
function renderEvents(){const box=$('events');box.replaceChildren();$('eventCount').textContent=state.events.length;if(!state.events.length){const e=document.createElement('div');e.className='empty-list';e.textContent='Nessuna segnalazione';box.append(e);return}state.events.slice(0,20).forEach(e=>{const d=document.createElement('div');d.className='event '+(e.alarm?'sos':e.event==='zone_safe'?'safe':'');const b=document.createElement('strong'),s=document.createElement('span');b.textContent=e.device+' · '+(labels[e.event]||e.event);s.textContent=e.detail?e.time+' · '+e.detail:e.time+' · BPM '+e.bpm+' · MQ2 ADC '+e.mq2;d.append(b,s);box.append(d)})}
function renderMap(){const bounds=[];for(const op of state.operators.values()){const g=op.gps?.valid?op.gps:op.lastValidGps;if(!g)continue;const ll=[g.lat,g.lon],alarm=op.alarmUntil>Date.now();bounds.push(ll);let marker=markers.get(op.id);if(!marker){marker=L.marker(ll,{icon:icon(alarm)}).addTo(map).on('click',()=>selectOperator(op));markers.set(op.id,marker)}else{marker.setLatLng(ll);marker.setIcon(icon(alarm))}marker.bindTooltip(op.id,{permanent:true,direction:'top',offset:[0,-12],className:'operator-label'});if(paths.has(op.id))map.removeLayer(paths.get(op.id));if(op.path.length>1){const layer=L.layerGroup().addTo(map);for(let i=1;i<op.path.length;i++){const bpm=op.path[i][2],color=bpm<100?'#2ecc71':bpm<130?'#f1c40f':'#e74c3c';L.polyline([op.path[i-1].slice(0,2),op.path[i].slice(0,2)],{color,weight:4}).addTo(layer)}paths.set(op.id,layer)}}if(!fitted&&bounds.length){map.fitBounds(bounds,{maxZoom:18,padding:[30,30]});fitted=true}}
function render(){if(!selected&&state.operators.size)selected=state.operators.keys().next().value;renderOperators();panel(state.operators.get(selected));renderEvents();renderMap()}
async function refresh(){try{const [status,messages]=await Promise.all([fetch('/api/status',{cache:'no-store'}).then(r=>r.json()),fetch('/api/messages',{cache:'no-store'}).then(r=>r.json())]);$('dot').classList.toggle('on',status.mqtt);$('mqtt').textContent=status.mqtt?'Mosquitto connesso':'Mosquitto disconnesso';messages.forEach(ingest);$('count').textContent=status.totalMessages+' MQTT · '+state.valid+' validi';render()}catch(e){$('dot').classList.remove('on');$('mqtt').textContent='ESP non raggiungibile'}}
refresh();setInterval(refresh,1000);
</script></body></html>
)HTML";

String jsonEscape(const String &source) {
  String escaped;
  escaped.reserve(source.length() + 16U);
  for (size_t i = 0U; i < source.length(); ++i) {
    const char value = source.charAt(i);
    switch (value) {
      case '"': escaped += F("\\\""); break;
      case '\\': escaped += F("\\\\"); break;
      case '\n': escaped += F("\\n"); break;
      case '\r': escaped += F("\\r"); break;
      case '\t': escaped += F("\\t"); break;
      default:
        if (static_cast<uint8_t>(value) >= 0x20U) escaped += value;
        break;
    }
  }
  return escaped;
}

void onMqttMessage(char *topic, uint8_t *payload, unsigned int length) {
  MqttEntry &entry = historyEntries[nextHistorySlot];
  entry.topic = topic;
  entry.payload = "";
  entry.payload.reserve(length);
  for (unsigned int i = 0U; i < length; ++i) {
    entry.payload += static_cast<char>(payload[i]);
  }
  entry.receivedAtMs = millis();
  entry.number = nextMessageNumber++;
  nextHistorySlot = (nextHistorySlot + 1U) % HISTORY_CAPACITY;
  if (historyCount < HISTORY_CAPACITY) ++historyCount;
}

/* Ordine cronologico: il browser ricostruisce lo stato applicando gli eventi. */
void handleMessagesApi() {
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, F("application/json; charset=utf-8"), "");
  webServer.sendContent(F("["));
  const size_t oldest = (nextHistorySlot + HISTORY_CAPACITY - historyCount) % HISTORY_CAPACITY;
  for (size_t position = 0U; position < historyCount; ++position) {
    const MqttEntry &entry = historyEntries[(oldest + position) % HISTORY_CAPACITY];
    String item;
    item.reserve(entry.payload.length() + entry.topic.length() + 96U);
    if (position > 0U) item += ',';
    item += F("{\"number\":"); item += String(entry.number);
    item += F(",\"topic\":\""); item += jsonEscape(entry.topic);
    item += F("\",\"payload\":\""); item += jsonEscape(entry.payload);
    item += F("\",\"ageMs\":"); item += String(millis() - entry.receivedAtMs);
    item += '}';
    webServer.sendContent(item);
  }
  webServer.sendContent(F("]"));
}

void handleStatusApi() {
  String json = F("{\"wifi\":");
  json += WiFi.status() == WL_CONNECTED ? F("true") : F("false");
  json += F(",\"mqtt\":");
  json += mqttClient.connected() ? F("true") : F("false");
  json += F(",\"ip\":\""); json += WiFi.localIP().toString();
  json += F("\",\"totalMessages\":"); json += String(nextMessageNumber - 1U);
  json += '}';
  webServer.send(200, F("application/json; charset=utf-8"), json);
}

void maintainMqttConnection() {
  if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) return;
  const unsigned long now = millis();
  if (now - lastMqttAttemptMs < 5000UL) return;
  lastMqttAttemptMs = now;
  const bool connected = APP_MQTT_USERNAME[0] == '\0'
      ? mqttClient.connect(APP_MQTT_DASHBOARD_CLIENT_ID)
      : mqttClient.connect(APP_MQTT_DASHBOARD_CLIENT_ID,
                           APP_MQTT_USERNAME, APP_MQTT_PASSWORD);
  if (connected) {
    const bool sosOk = mqttClient.subscribe(APP_MQTT_SOS_TOPIC, 1);
    const bool telemetryOk = mqttClient.subscribe(APP_MQTT_TELEMETRY_TOPIC, 1);
    const bool statusOk = mqttClient.subscribe(APP_MQTT_STATUS_TOPIC, 1);
    Serial.printf("MQTT connesso; subscribe SOS=%u telemetry=%u status=%u\r\n",
                  sosOk, telemetryOk, statusOk);
  } else {
    Serial.printf("Connessione MQTT fallita, stato: %d\r\n", mqttClient.state());
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.begin(APP_WIFI_SSID, APP_WIFI_PASSWORD);
  mqttClient.setServer(APP_MQTT_BROKER_HOST, APP_MQTT_BROKER_PORT);
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
  mqttClient.setCallback(onMqttMessage);
  webServer.on("/", HTTP_GET, []() {
    webServer.send_P(200, PSTR("text/html; charset=utf-8"), DASHBOARD_HTML);
  });
  webServer.on("/api/status", HTTP_GET, handleStatusApi);
  webServer.on("/api/messages", HTTP_GET, handleMessagesApi);
  webServer.onNotFound([]() { webServer.send(404, F("text/plain"), F("Not found")); });
  webServer.begin();
}

void loop() {
  const wl_status_t wifiStatus = WiFi.status();
  if (wifiStatus == WL_CONNECTED && previousWifiStatus != WL_CONNECTED) {
    Serial.print(F("Dashboard disponibile su http://"));
    Serial.println(WiFi.localIP());
  } else if (wifiStatus != WL_CONNECTED && previousWifiStatus == WL_CONNECTED) {
    Serial.println(F("Connessione Wi-Fi interrotta"));
  }
  previousWifiStatus = wifiStatus;
  webServer.handleClient();
  maintainMqttConnection();
  if (mqttClient.connected()) mqttClient.loop();
  yield();
}
