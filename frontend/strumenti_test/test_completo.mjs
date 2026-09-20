import puppeteer from 'puppeteer-core';
import { trovaBrowser } from './edge.mjs';
import { spawn } from 'node:child_process';

const EDGE = trovaBrowser();
const MOCK = 'http://localhost:5175';
const REAL = 'http://localhost:5174';
const STORE = '/src/core/store.ts';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const errors = [];
const R = {};
const browser = await puppeteer.launch({ executablePath: EDGE, headless: true, args: ['--no-first-run', '--autoplay-policy=no-user-gesture-required'] });

async function open(base, path, label, { user = 'demo', pass = 'demo', doLogin = true } = {}) {
  const ctx = await browser.createBrowserContext();
  const page = await ctx.newPage();
  await page.setViewport({ width: 1440, height: 900 });
  await page.evaluateOnNewDocument(() => {
    window.__osc = 0;
    const orig = AudioContext.prototype.createOscillator;
    AudioContext.prototype.createOscillator = function () { window.__osc++; return orig.call(this); };
  });
  page.on('pageerror', (e) => errors.push(`pageerror ${label}: ${e.message}`));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`console ${label}: ${m.text()}`); });
  await page.goto(base + path, { waitUntil: 'networkidle2' });
  if (doLogin) {
    await page.waitForSelector('#login-form');
    await page.type('#login-user', user);
    await page.type('#login-pass', pass);
    await page.click('#login-form button[type="submit"]');
    await page.waitForSelector('#nav', { timeout: 10000 });
    await sleep(500);
  }
  return { ctx, page };
}
const health = (p) => p.evaluate(() => [...document.querySelectorAll('#health .badge')].map((b) => `${b.textContent.trim()}:${b.querySelector('i').className.match(/bg-\w+/)[0]}`));

// ================= DATI FINTI =================

// Login
{
  const { ctx, page } = await open(MOCK, '/', 'L', { doLogin: false });
  await page.waitForSelector('#login-form');
  await page.click('#login-form button[type="submit"]');
  await sleep(200);
  const blocked = !(await page.$('#nav'));
  await page.type('#login-user', 'Paolo Colombo');
  await page.type('#login-pass', 'x');
  await page.click('#login-form button[type="submit"]');
  await page.waitForSelector('#nav');
  R.L_login = { emptyBlocked: blocked, sidebarUser: await page.$eval('#sidebar', (e) => e.textContent.includes('Paolo Colombo')) };
  await ctx.close();
}

// T9 parte 1
const t9 = await open(MOCK, '/', 'T9');
await t9.page.evaluate(async (s) => { const m = await import(s); const t = new Date().toISOString(); m.applyEvent({ schema: 'nisc.event.v1', device_id: '99', sequence: 1, event_type: 'TELEMETRY', event_time: t, received_time: t, priority: 'normal', data: { battery: 50 } }); }, STORE);
const t9start = Date.now();

// T2 navigazione rapida
{
  const { ctx, page } = await open(MOCK, '/', 'T2');
  await page.click('#nav a[data-path="/mappa"]');
  await page.click('#nav a[data-path="/eventi"]');
  await sleep(4000);
  R.T2_race = await page.evaluate(() => ({ url: location.pathname, title: document.getElementById('title').textContent, hasTable: !!document.querySelector('#view table'), hasMap: !!document.querySelector('#view .leaflet-container') }));
  await ctx.close();
}

// F1 + F4 + T1 + T3 + F3 + T4 + T15 + T13
{
  const { ctx, page } = await open(MOCK, '/', 'F1');
  R.F3_radio_badge = (await health(page)).some((h) => h.startsWith('Radio'));
  await page.waitForSelector('[data-ack]', { timeout: 20000 });
  const o1 = await page.evaluate(() => window.__osc);
  await sleep(3500);
  const o2 = await page.evaluate(() => window.__osc);
  R.F1_siren_while_open = { oscillatorsIn3_5s: o2 - o1, audioButtonHidden: await page.$eval('#audio-unlock', (e) => e.hidden) };

  const box = await (await page.$('[data-ack]')).boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.evaluate(async (s) => (await import(s)).notify(), STORE);
  await page.mouse.up();
  await sleep(400);
  R.T1_F4_ack = await page.evaluate(() => ({ ackButtons: document.querySelectorAll('[data-ack]').length, closeButtons: document.querySelectorAll('[data-close]').length, text: document.querySelector('#sos-banner').textContent.replace(/\s+/g, ' ').trim().slice(0, 80) }));
  const o3 = await page.evaluate(() => window.__osc);
  await sleep(3500);
  R.F1_silent_after_ack = (await page.evaluate(() => window.__osc)) - o3;

  await page.focus('[data-close]');
  await page.evaluate(async (s) => (await import(s)).notify(), STORE);
  R.T3_focus = await page.evaluate(() => !!document.activeElement?.matches?.('[data-close]'));
  await page.click('[data-close]');
  await sleep(300);
  R.F4_closed = await page.evaluate(() => document.querySelectorAll('#sos-banner > *').length === 0);

  R.T4_malformed = await page.evaluate(async (s) => {
    const m = await import(s); const now = new Date().toISOString(); const out = {};
    const run = (n, e) => { try { m.applyEvent(e); out[n] = 'ok'; } catch (x) { out[n] = 'THROW ' + x.message; } };
    run('unknownPriority', { schema: 'nisc.event.v1', device_id: '02', sequence: 99001, event_type: 'TELEMETRY', event_time: now, received_time: now, priority: 'info', data: { spo2: 90 } });
    run('missingData', { schema: 'nisc.event.v1', device_id: '02', sequence: 99002, event_type: 'TELEMETRY', event_time: now, received_time: now, priority: 'normal' });
    run('valid', { schema: 'nisc.event.v1', device_id: '02', sequence: 99005, event_type: 'TELEMETRY', event_time: now, received_time: now, priority: 'normal', data: { spo2: 91, heart_rate: 123 } });
    return out;
  }, STORE);
  R.T4_ui_still_updates = await page.evaluate(() => document.querySelector('#recent').textContent.includes('HR 123'));

  await page.click('#fleet [data-select-device="03"]');
  await sleep(2500);
  R.T15_T13 = await page.evaluate(() => ({ url: location.pathname, title: document.querySelector('#detail h2')?.textContent, history: document.querySelector('#detail').textContent.includes('TELEMETRY') }));
  await ctx.close();
}

// F2 mappe offline + F5 zone + T8
{
  const { ctx, page } = await open(MOCK, '/mappa', 'F2');
  await page.waitForSelector('.leaflet-container');
  await sleep(1000);
  R.F2_notice_online = await page.$eval('#map-notice', (e) => e.hidden);
  await page.setOfflineMode(true);
  await sleep(300);
  R.F2_notice_offline_no_tiles = await page.$eval('#map-notice', (e) => !e.hidden);
  await page.setOfflineMode(false);

  await page.waitForSelector('[data-zone-status="validated"]', { timeout: 25000 });
  await page.click('[data-zone-status="validated"]');
  await sleep(400);
  R.F5_validate = await page.evaluate(async (s) => { const m = await import(s); return { status: m.store.safeZones.find((z) => z.id === 2)?.status, proposedLeft: document.querySelectorAll('[data-zone-id]').length }; }, STORE);
  const count = () => page.evaluate(() => document.querySelectorAll('path.leaflet-interactive').length);
  const before = await count();
  await page.evaluate(async (s) => { const m = await import(s); m.store.safeZones.forEach((z) => (z.status = 'invalidated')); m.notify(); }, STORE);
  await sleep(400);
  R.T8_invalidated_removed = { before, after: await count() };

  await page.click('#toasts .alert a').catch(() => {});
  await ctx.close();
}

// F2 tessere locali (manifest e tessere simulati con intercettazione, nessun file nel progetto)
{
  const ctx = await browser.createBrowserContext();
  const page = await ctx.newPage();
  await page.setViewport({ width: 1440, height: 900 });
  await page.setRequestInterception(true);
  const png = Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==', 'base64');
  let localTileRequests = 0;
  page.on('request', (r) => {
    const u = new URL(r.url());
    if (u.pathname === '/tiles/tiles.json') return r.respond({ status: 200, contentType: 'application/json', body: JSON.stringify({ minZoom: 12, maxZoom: 18, bounds: [[40.80, 14.20], [40.90, 14.33]] }) });
    if (u.pathname.startsWith('/tiles/')) { localTileRequests++; return r.respond({ status: 200, contentType: 'image/png', body: png }); }
    r.continue();
  });
  await page.goto(MOCK + '/', { waitUntil: 'networkidle2' });
  await page.type('#login-user', 'a'); await page.type('#login-pass', 'b');
  await page.click('#login-form button[type="submit"]');
  await page.waitForSelector('#nav');
  await page.click('#nav a[data-path="/mappa"]');
  await sleep(2500);
  await page.setOfflineMode(true);
  await sleep(300);
  R.F2_local_tiles = { localTileRequests, noticeHiddenOffline: await page.$eval('#map-notice', (e) => e.hidden) };
  await ctx.close();
}

// T6 chat, T11 percorso sconosciuto, T14 modal
{
  const { ctx, page } = await open(MOCK, '/assistente', 'T6');
  await page.waitForSelector('[data-example]');
  await page.click('[data-example]');
  await page.click('#nav a[data-path="/mappa"]');
  await sleep(1500);
  await page.click('#nav a[data-path="/assistente"]');
  await sleep(1200);
  R.T6_chat = await page.evaluate(() => document.querySelectorAll('#thread .chat').length);
  await ctx.close();
}
{
  const { ctx, page } = await open(MOCK, '/non-esiste', 'T11');
  R.T11 = await page.evaluate(() => ({ url: location.pathname, title: document.getElementById('title').textContent }));
  await page.click('#nav a[data-path="/eventi"]');
  await page.waitForSelector('#rows tr');
  // La finestra con il JSON è stata tolta: la tabella ha 5 colonne e nessun pulsante "Apri".
  R.T14_tabella = await page.evaluate(() => ({
    apri: document.querySelectorAll('[data-open]').length,
    colonne: document.querySelectorAll('#rows tr:first-child td').length,
    tipiFiltro: [...document.querySelectorAll('#f-type option')].map((o) => o.value),
  }));
  await ctx.close();
}

await sleep(Math.max(0, 37000 - (Date.now() - t9start)));
R.T9_offline = await t9.page.evaluate(async (s) => { const m = await import(s); return { d99: m.store.devices.get('99').online, others: ['01', '02', '03', '05'].map((i) => m.store.devices.get(i).online) }; }, STORE);
await t9.ctx.close();

// ================= SERVER (finto Flask) =================
let flask;
async function startFlask() {
  flask = spawn('node', ['finto_flask.mjs'], { cwd: import.meta.dirname, stdio: 'ignore' });
  for (let i = 0; i < 50; i++) { try { await fetch('http://localhost:5000/__stats'); return; } catch { await sleep(200); } }
}
const stats = async () => (await fetch('http://localhost:5000/__stats')).json();
await startFlask();
{
  const { ctx, page } = await open(REAL, '/', 'R', { doLogin: false });
  await page.waitForSelector('#login-form');
  await page.type('#login-user', 'coord'); await page.type('#login-pass', 'sbagliata');
  await page.click('#login-form button[type="submit"]');
  await sleep(600);
  R.R_wrong_password = await page.$eval('#login-error', (e) => e.textContent);
  await page.$eval('#login-pass', (e) => (e.value = ''));
  await page.type('#login-pass', 'pass');
  await page.click('#login-form button[type="submit"]');
  await page.waitForFunction(() => document.querySelectorAll('#fleet [data-select-device]').length === 2, { timeout: 15000 });
  await sleep(1500);
  R.R_login_and_load = { health: await health(page), user: await page.$eval('#sidebar', (e) => e.textContent.includes('Coordinatore Test')) };

  await fetch('http://localhost:5000/__sos');
  await sleep(1200);
  R.R_sos = await page.evaluate(() => ({ banners: document.querySelectorAll('#sos-banner > *').length, ack: document.querySelectorAll('[data-ack]').length, toasts: document.querySelectorAll('#toasts .alert').length }));
  await page.click('[data-ack]');
  await sleep(800);
  R.R_ack = { banner: await page.$eval('#sos-banner', (e) => e.textContent.replace(/\s+/g, ' ').trim().slice(0, 90)), server: (await stats()).alerts };
  await page.click('[data-close]');
  await sleep(800);
  R.R_close = { banners: await page.evaluate(() => document.querySelectorAll('#sos-banner > *').length), server: (await stats()).alerts };

  await page.select('#cmd-target', '02');
  await page.select('#cmd-preset', 'custom');
  await page.type('#cmd-text', 'Tutti al punto nord');
  await page.click('#cmd-form button[type="submit"]');
  await sleep(800);
  const s = await stats();
  R.R_command_csrf = { ui: await page.$eval('#cmd-result', (e) => e.textContent), received: s.commands.at(-1), csrfRejected: s.csrfRejected };

  await page.click('#nav a[data-path="/mappa"]');
  await page.waitForSelector('[data-zone-status="validated"]', { timeout: 8000 });
  await page.click('[data-zone-status="validated"]');
  await sleep(800);
  R.R_zone = { server: (await stats()).zones, proposedLeft: await page.evaluate(() => document.querySelectorAll('[data-zone-id]').length) };

  await page.click('#nav a[data-path="/"]');
  flask.kill();
  await sleep(3000);
  R.R_down = await health(page);
  await startFlask();
  await sleep(9000);
  R.R_restart = { health: await health(page), devices: await page.evaluate(() => document.querySelectorAll('#fleet [data-select-device]').length) };

  await page.click('#logout');
  await sleep(1500);
  R.R_logout = !!(await page.$('#login-form'));
  await ctx.close();
}
flask.kill();
await browser.close();

// ================= VERDETTO =================
// Ogni riga è un controllo: descrizione + condizione attesa. Alla fine stampa OK o FALLITO
// e restituisce un codice di uscita, così si capisce l'esito senza leggere tutto il rapporto.
const badge = (lista, nome) => (lista ?? []).find((h) => h.startsWith(nome)) ?? '';
// Gli unici errori ammessi sono quelli provocati apposta: password sbagliata (401) e server
// spento (502). Qualunque altro errore in console fa fallire il test.
const erroriVeri = errors.filter((e) => !/(401 \(Unauthorized\)|502 \(Bad Gateway\))/.test(e));

const controlli = [
  ['login: campi vuoti rifiutati e nome utente mostrato', R.L_login?.emptyBlocked && R.L_login?.sidebarUser],
  ['navigazione veloce: resta sulla pagina giusta', R.T2_race?.url === '/eventi' && R.T2_race?.hasTable && !R.T2_race?.hasMap],
  ['indicatore Radio presente', R.F3_radio_badge],
  ['sirena: suona con SOS aperto', R.F1_siren_while_open?.oscillatorsIn3_5s > 0 && R.F1_siren_while_open?.audioButtonHidden],
  ['presa in carico: cambia il banner e non chiude l\'allarme', R.T1_F4_ack?.ackButtons === 0 && R.T1_F4_ack?.closeButtons === 1 && /PRESO IN CARICO/.test(R.T1_F4_ack?.text ?? '')],
  ['sirena: tace dopo la presa in carico', R.F1_silent_after_ack === 0],
  ['tastiera: il focus resta sul pulsante', R.T3_focus],
  ['chiusura: il banner sparisce', R.F4_closed],
  ['eventi malformati: scartati senza rompere la pagina', Object.values(R.T4_malformed ?? {}).every((v) => v === 'ok')],
  ['la pagina continua ad aggiornarsi dopo un evento rotto', R.T4_ui_still_updates],
  ['dettaglio gilet: storico caricato', R.T15_T13?.history && R.T15_T13?.title === 'Gilet 03'],
  ['mappa: nessun avviso quando la rete c\'è', R.F2_notice_online === true],
  ['mappa: avviso quando manca rete e tessere', R.F2_notice_offline_no_tiles === true],
  ['mappa: tessere locali usate, nessun avviso', R.F2_local_tiles?.localTileRequests > 0 && R.F2_local_tiles?.noticeHiddenOffline],
  ['zone sicure: validazione', R.F5_validate?.status === 'validated' && R.F5_validate?.proposedLeft === 0],
  ['zone sicure: quella invalidata sparisce dalla mappa', R.T8_invalidated_removed?.after < R.T8_invalidated_removed?.before],
  ['assistente: risponde in chat', R.T6_chat >= 2],
  ['indirizzo sconosciuto: riporta alla dashboard', R.T11?.url === '/'],
  ['storico: tabella a 5 colonne, nessun pulsante "Apri", filtro SOS_CANCEL', R.T14_tabella?.apri === 0 && R.T14_tabella?.colonne === 5 && R.T14_tabella?.tipiFiltro?.includes('SOS_CANCEL')],
  ['offline: solo il gilet silenzioso passa offline', R.T9_offline?.d99 === false && R.T9_offline?.others?.every(Boolean)],
  ['server: password sbagliata rifiutata', /errati/.test(R.R_wrong_password ?? '')],
  ['server: login e caricamento iniziale', R.R_login_and_load?.user && badge(R.R_login_and_load?.health, 'Server') === 'Server:bg-success'],
  ['server: SOS mostrato con banner e avviso', R.R_sos?.banners === 1 && R.R_sos?.ack === 1 && R.R_sos?.toasts === 1],
  ['server: presa in carico registrata', /PRESO IN CARICO/.test(R.R_ack?.banner ?? '') && R.R_ack?.server?.includes('acknowledge 7')],
  ['server: chiusura registrata', R.R_close?.banners === 0 && R.R_close?.server?.includes('close 7')],
  ['server: comando ricevuto e token CSRF richiesto', /inviato/.test(R.R_command_csrf?.ui ?? '') && R.R_command_csrf?.received?.body?.payload?.params?.text === 'Tutti al punto nord' && R.R_command_csrf?.csrfRejected === 0],
  ['server: zona validata', R.R_zone?.server?.includes('1 validated') && R.R_zone?.proposedLeft === 0],
  ['server spento: indicatore rosso', badge(R.R_down, 'Server') === 'Server:bg-error'],
  ['server riacceso: dati ricaricati da soli', badge(R.R_restart?.health, 'Server') === 'Server:bg-success' && R.R_restart?.devices === 2],
  ['uscita: si torna al login', R.R_logout],
  ['nessun errore imprevisto nella console del browser', erroriVeri.length === 0],
];

const falliti = controlli.filter(([, ok]) => !ok);
console.log('\n================ VERDETTO ================');
for (const [nome, ok] of controlli) console.log(`${ok ? '  ok  ' : ' FALLITO '} ${nome}`);
console.log(`\n${controlli.length - falliti.length}/${controlli.length} controlli superati`);
if (erroriVeri.length) console.log('Errori in console:\n' + erroriVeri.map((e) => '  ' + e).join('\n'));
console.log(falliti.length === 0 ? 'ESITO: OK' : 'ESITO: FALLITO');
if (falliti.length) console.log('\nRapporto completo:\n' + JSON.stringify(R, null, 2));
process.exit(falliti.length === 0 ? 0 : 1);
