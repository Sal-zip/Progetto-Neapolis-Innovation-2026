// Finto Flask per i test: contratto attuale + login, CSRF, allarmi, zone, radio.
import http from 'node:http';
import { Server } from 'socket.io';

const CSRF = 'tok123';
const USER = { id: 1, name: 'Coordinatore Test', role: 'Coordinatore' };
const stats = { requests: {}, commands: [], ai: [], csrfRejected: 0, alerts: [], zones: [] };
const now = () => new Date().toISOString();
let seq = 5000;
const events = [];
const ev = (device_id, event_type, priority, data) => { const t = now(); return { schema: 'nisc.event.v1', device_id, sequence: ++seq, event_type, event_time: t, received_time: t, priority, data }; };
for (let i = 0; i < 20; i++) events.unshift(ev(i % 2 ? '01' : '02', 'TELEMETRY', 'normal', { spo2: 97, heart_rate: 80 + i, mq2_raw: 1800 + i, battery: 70 - i }));

const devices = [
  { device_id: '01', operator_id: 1, firmware: 'v1.2', last_seen: now(), online: true, sos: false, alert: null, battery: 55, spo2: 97, heart_rate: 81, mq2_raw: 1801, latitude: 40.8518, longitude: 14.2681, radio: { rssi: -91, snr: 7.5 } },
  { device_id: '02', operator_id: 2, firmware: 'v1.2', last_seen: now(), online: true, sos: false, alert: null, battery: 70, latitude: 40.8501, longitude: 14.2705 },
];
const operators = [
  { id: 1, name: 'Mario Rossi', role: 'Soccorritore', device_id: '01', in_mission: true, since: 'Gen 2025', missions: [] },
  { id: 2, name: 'Luca Bianchi', role: 'Soccorritore', device_id: '02', in_mission: true, since: 'Mar 2025', missions: [] },
];
const zones = [{ id: 1, device_id: '02', latitude: 40.8525, longitude: 14.2691, radius_m: 30, operator_id: 2, status: 'proposed', created_at: now() }];

function json(res, code, body, headers = {}) {
  res.writeHead(code, { 'Content-Type': 'application/json', ...headers });
  res.end(JSON.stringify(body));
}
function page(list, url) {
  const limit = Number(url.searchParams.get('limit') ?? 50), offset = Number(url.searchParams.get('offset') ?? 0);
  const type = url.searchParams.get('event_type'), dev = url.searchParams.get('device_id');
  const f = list.filter((e) => (!type || e.event_type === type) && (!dev || e.device_id === dev));
  return { total: f.length, events: f.slice(offset, offset + limit) };
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');
  const key = `${req.method} ${url.pathname.replace(/\/(devices|alerts|safe-zones)\/[^/]+/, '/$1/{id}')}`;
  stats.requests[key] = (stats.requests[key] ?? 0) + 1;
  let body = '';
  for await (const c of req) body += c;
  const logged = /(?:^|;\s*)sid=s1/.test(req.headers.cookie ?? '');

  if (url.pathname === '/__stats') return json(res, 200, stats);
  if (url.pathname === '/__sos') {
    const e = ev('01', 'SOS', 'critical', { origin: 'voluntary', cause: 'manual_gesture', latitude: 40.8518, longitude: 14.2681, battery: 54 });
    events.unshift(e);
    devices[0].sos = true;
    devices[0].alert = { id: 7, status: 'open', opened_at: e.received_time, acknowledged_by: null };
    io.emit('nisc_event', e);
    io.emit('nisc_event', e);
    io.emit('alert_updated', { device_id: '01', alert: devices[0].alert });
    return json(res, 200, { ok: true });
  }
  if (url.pathname === '/api/login' && req.method === 'POST') {
    const { username, password } = JSON.parse(body || '{}');
    if (username === 'coord' && password === 'pass') return json(res, 200, { user: USER, csrf_token: CSRF }, { 'Set-Cookie': 'sid=s1; HttpOnly; Path=/' });
    return json(res, 401, { error: 'bad credentials' });
  }
  if (!url.pathname.startsWith('/api/')) return json(res, 404, { error: 'not found' });
  if (!logged) return json(res, 401, { error: 'login required' });
  if (req.method === 'POST' && req.headers['x-csrf-token'] !== CSRF) { stats.csrfRejected++; return json(res, 403, { error: 'csrf' }); }

  if (url.pathname === '/api/me') return json(res, 200, { user: USER, csrf_token: CSRF });
  if (url.pathname === '/api/logout') return json(res, 200, { ok: true }, { 'Set-Cookie': 'sid=; Max-Age=0; Path=/' });
  if (url.pathname === '/api/devices') return json(res, 200, devices);
  if (url.pathname === '/api/operators') return json(res, 200, operators);
  if (url.pathname === '/api/safe-zones') return json(res, 200, zones);
  if (url.pathname === '/api/health') return json(res, 200, { broker: 'online', bridge: 'online', database: 'online', radio: 'degraded' });
  if (url.pathname === '/api/events') return json(res, 200, page(events, url));
  let m = url.pathname.match(/^\/api\/devices\/([^/]+)\/events$/);
  if (m) return json(res, 200, page(events.filter((e) => e.device_id === decodeURIComponent(m[1])), url));
  m = url.pathname.match(/^\/api\/devices\/([^/]+)\/commands$/);
  if (m && req.method === 'POST') { stats.commands.push({ target: decodeURIComponent(m[1]), body: JSON.parse(body) }); return json(res, 200, { ok: true }); }
  m = url.pathname.match(/^\/api\/alerts\/(\d+)\/(acknowledge|close)$/);
  if (m && req.method === 'POST') {
    stats.alerts.push(`${m[2]} ${m[1]}`);
    const d = devices.find((x) => x.alert?.id === Number(m[1]));
    if (!d) return json(res, 404, { error: 'no alert' });
    if (m[2] === 'acknowledge') {
      d.alert = { ...d.alert, status: 'acknowledged', acknowledged_by: USER.name };
      io.emit('alert_updated', { device_id: d.device_id, alert: d.alert });
      return json(res, 200, d.alert);
    }
    const closed = { id: d.alert.id, status: 'closed' };
    d.alert = null; d.sos = false;
    io.emit('alert_updated', { device_id: d.device_id, alert: null });
    return json(res, 200, closed);
  }
  m = url.pathname.match(/^\/api\/safe-zones\/(\d+)$/);
  if (m && req.method === 'POST') {
    const z = zones.find((x) => x.id === Number(m[1]));
    z.status = JSON.parse(body).status;
    stats.zones.push(`${z.id} ${z.status}`);
    return json(res, 200, z);
  }
  if (url.pathname === '/api/ai/query' && req.method === 'POST') { stats.ai.push(JSON.parse(body)); return json(res, 200, { answer: 'Risposta dal server finto', sql: 'SELECT 1;', rows: [{ uno: 1 }] }); }
  json(res, 404, { error: 'not found' });
});

const io = new Server(server);
let t = 0;
setInterval(() => {
  const e = ev('02', 'TELEMETRY', 'normal', { spo2: 96, heart_rate: 90 + (t++ % 5), mq2_raw: 1900, battery: 69 });
  events.unshift(e);
  devices[1].last_seen = e.received_time;
  io.emit('nisc_event', e);
}, 700);
server.listen(5000, () => console.log('fake flask 2 on :5000'));
