// Allarmi automatici su soglia e falso allarme (dati simulati su http://localhost:5175).
// Avviare prima: cd frontend && npm run dev -- --port 5175
import puppeteer from 'puppeteer-core';
import { trovaBrowser } from './edge.mjs';

const EDGE = trovaBrowser();
const MOCK = 'http://localhost:5175/';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const errors = [];

const browser = await puppeteer.launch({ executablePath: EDGE, headless: true, args: ['--no-first-run'] });
const page = await browser.newPage();
page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text()}`); });
await page.setViewport({ width: 1440, height: 1000 });
await page.goto(MOCK, { waitUntil: 'networkidle2' });
await page.type('#login-user', 'demo');
await page.type('#login-pass', 'demo');
await page.click('#login-form button[type="submit"]');
await page.waitForSelector('#nav');
const t0 = Date.now();

const banners = () => page.evaluate(() =>
  [...document.querySelectorAll('#sos-banner > div')].map((d) => d.textContent.replace(/\s+/g, ' ').trim()));
const at = async (sec) => sleep(Math.max(0, sec * 1000 - (Date.now() - t0)));

const R = {};
await at(46);
const b46 = await banners();
R.falsoAllarme = b46.some((b) => b.startsWith('FALSO ALLARME') && b.includes('Gilet 02') && b.includes('Chiudi allarme'));
R.sosGasAutomatico = b46.some((b) => b.includes('Gilet 03') && b.includes('gas oltre la soglia'));
R.fuoriSoglia = await page.evaluate(() =>
  [...document.querySelectorAll('#fleet div')].some((d) => d.textContent.startsWith('Fuori soglia')));

await at(125);
const b125 = await banners();
R.sosVitaliAutomatico = b125.some((b) => b.includes('Gilet 05') && b.includes('ossigenazione troppo bassa'));

// L'evento di annullamento è nello storico.
await page.click('#nav a[data-path="/eventi"]');
await page.waitForSelector('#rows tr');
await page.select('#f-type', 'SOS_CANCEL');
await sleep(1200);
R.eventoSosCancel = await page.evaluate(() =>
  [...document.querySelectorAll('#rows tr')].some((r) => r.textContent.includes('falso allarme')));

await browser.close();
const ok = Object.values(R).every(Boolean) && errors.length === 0;
console.log(JSON.stringify({ R, errors, esito: ok ? 'OK' : 'FALLITO' }, null, 1));
process.exit(ok ? 0 : 1);
