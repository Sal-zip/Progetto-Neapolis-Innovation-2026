import puppeteer from 'puppeteer-core';
import { trovaBrowser } from './edge.mjs';
const EDGE = trovaBrowser();
const OUT = import.meta.dirname + '\\..\\';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const errors = [];
const browser = await puppeteer.launch({ executablePath: EDGE, headless: true, args: ['--no-first-run'] });
const page = await browser.newPage();
page.on('pageerror', (e) => errors.push(e.message));
page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
await page.setViewport({ width: 1440, height: 900 });
await page.goto('http://localhost:5175/', { waitUntil: 'networkidle2' });
await page.type('#login-user', 'demo'); await page.type('#login-pass', 'demo');
await page.click('#login-form button[type="submit"]');
await page.waitForSelector('#nav');
const t0 = Date.now();
await page.click('#nav a[data-path="/mappa"]');
await sleep(1500);

// Fluidità: posizione del marker del gilet 05 ogni 100 ms per 6 s.
const samples = [];
for (let i = 0; i < 60; i++) {
  samples.push(await page.evaluate(() => {
    const tip = [...document.querySelectorAll('.leaflet-tooltip')].find((t) => t.textContent.startsWith('05'));
    const r = tip?.getBoundingClientRect();
    return r ? [Math.round(r.x * 10) / 10, Math.round(r.y * 10) / 10] : null;
  }));
  await sleep(100);
}
const distinct = new Set(samples.map((s) => s.join(','))).size;
const steps = samples.slice(1).map((s, i) => Math.hypot(s[0] - samples[i][0], s[1] - samples[i][1]));
const R = { smooth: { distinctPositionsIn6s: distinct, maxStepPx: Math.max(...steps).toFixed(2) } };

const snap = async (label) => {
  const s = await page.evaluate(async () => {
    const m = await import('/src/core/store.ts');
    const d = Object.fromEntries([...m.store.devices.values()].map((x) => [x.device_id, { online: x.online, sos: x.sos, cause: x.sos_cause, alert: x.alert?.status, hr: x.heart_rate, mq2: x.mq2_raw }]));
    return { d, zones: m.store.safeZones.map((z) => `${z.id}:${z.status}`), events: m.store.events.length };
  });
  R[label] = { t: Math.round((Date.now() - t0) / 1000), ...s };
};
await sleep(Math.max(0, 25000 - (Date.now() - t0))); await snap('at25s');
await sleep(Math.max(0, 45000 - (Date.now() - t0))); await snap('at45s');
await page.screenshot({ path: `${OUT}scen_mappa.png` });
await sleep(Math.max(0, 100000 - (Date.now() - t0))); await snap('at100s');
await sleep(Math.max(0, 118000 - (Date.now() - t0))); await snap('at118s');
await browser.close();
console.log(JSON.stringify({ R, errors }, null, 1));
