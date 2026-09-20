import puppeteer from 'puppeteer-core';
import { trovaBrowser } from './edge.mjs';
import { spawn } from 'node:child_process';
const EDGE = trovaBrowser();
const OUT = import.meta.dirname + '\\..\\';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const errors = []; const R = {};
const browser = await puppeteer.launch({ executablePath: EDGE, headless: true, args: ['--no-first-run'] });

async function open(base, user, pass, label) {
  const ctx = await browser.createBrowserContext();
  const page = await ctx.newPage();
  await page.setViewport({ width: 1440, height: 900 });
  page.on('pageerror', (e) => errors.push(`${label}: ${e.message}`));
  page.on('console', (m) => { if (m.type() === 'error' && !m.text().includes('401')) errors.push(`${label} console: ${m.text()}`); });
  await page.goto(base, { waitUntil: 'networkidle2' });
  await page.type('#login-user', user); await page.type('#login-pass', pass);
  await page.click('#login-form button[type="submit"]');
  await page.waitForSelector('#nav');
  await sleep(800);
  return { ctx, page };
}

// ---- dati finti ----
{
  const { ctx, page } = await open('http://localhost:5175/', 'demo', 'demo', 'mock');
  R.badge_dati_simulati = await page.evaluate(() => document.querySelector('header').textContent.includes('Dati simulati'));
  R.dashboard_sensor_fault = await page.evaluate(() => document.querySelector('#fleet [data-select-device="03"]')?.textContent.includes('Sensore guasto: gas'));
  await page.waitForSelector('[data-ack]', { timeout: 20000 });
  R.sos_cause_banner = await page.$eval('#sos-banner', (e) => e.textContent.includes('gesto SOS'));
  R.sos_cause_toast = await page.$eval('#toasts', (e) => e.textContent.includes('gesto SOS'));

  await page.click('#nav a[data-path="/mappa"]');
  await sleep(2500);
  R.map = await page.evaluate(() => ({
    tracks: document.querySelectorAll('.leaflet-overlay-pane path:not(.leaflet-interactive)').length,
    tooltips: [...document.querySelectorAll('.leaflet-tooltip')].map((t) => t.textContent),
    listFresh: [...document.querySelectorAll('#map-list [data-focus]')].map((b) => b.textContent.replace(/\s+/g, ' ').trim()),
  }));
  await page.screenshot({ path: `${OUT}req_mappa.png` });
  await page.click('#show-tracks');
  await sleep(300);
  R.tracks_hidden = await page.evaluate(() => document.querySelectorAll('.leaflet-overlay-pane path:not(.leaflet-interactive)').length);
  await page.click('#show-tracks');

  await page.click('#nav a[data-path="/dispositivi"]');
  await sleep(600);
  await page.click('[data-select="03"]');
  await sleep(600);
  R.dispositivi_sensors = await page.$eval('#detail', (e) => e.textContent.replace(/\s+/g, ' ').match(/GPS: \w+.*?gas: \w+/)?.[0]);

  await page.click('#nav a[data-path="/operatori"]');
  await sleep(600);
  await page.click('[data-select="3"]');
  await sleep(400);
  await page.click('[data-track]');
  await sleep(1500);
  R.operator_to_map = await page.evaluate(() => ({ url: location.pathname, selected: document.querySelector('#map-list .ring-2')?.dataset.focus }));
  await ctx.close();
}

// ---- server finto ----
const flask = spawn('node', ['finto_flask.mjs'], { cwd: import.meta.dirname, stdio: 'ignore' });
await sleep(1500);
{
  const { ctx, page } = await open('http://localhost:5174/', 'coord', 'pass', 'real');
  R.real_no_badge = await page.evaluate(() => !document.querySelector('header').textContent.includes('Dati simulati'));
  await page.click('#nav a[data-path="/mappa"]');
  await sleep(2500);
  R.real_track_requests = (await (await fetch('http://localhost:5000/__stats')).json()).requests['GET /api/devices/{id}/events'];
  R.real_tracks = await page.evaluate(() => document.querySelectorAll('.leaflet-overlay-pane path:not(.leaflet-interactive)').length);
  await ctx.close();
}
flask.kill();
await browser.close();
console.log(JSON.stringify({ R, errors }, null, 2));
