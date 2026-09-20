// Lancia i test senza preparare niente a mano: avvia i due server di Vite (dati simulati sulla
// 5175, versione reale sulla 5174), esegue i test e alla fine spegne tutto.
//
//   cd frontend
//   npm test                  tutti i test
//   npm test -- allarmi       solo soglie e falso allarme
//
import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';

const QUI = import.meta.dirname;
const FRONTEND = path.resolve(QUI, '..');
const npm = process.platform === 'win32' ? 'npm.cmd' : 'npm';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const processi = [];

// shell serve solo per npm su Windows (npm.cmd); per node si evita, altrimenti gli argomenti
// vengono concatenati invece che passati uno per uno.
function avvia(comando, argomenti, cwd, shell = false) {
  const p = spawn(comando, argomenti, { cwd, stdio: 'ignore', shell });
  processi.push(p);
  return p;
}
const avviaNpm = (argomenti, cwd) => avvia(npm, argomenti, cwd, process.platform === 'win32');

async function attendi(url, nome) {
  for (let i = 0; i < 100; i++) {
    try {
      await fetch(url);
      return;
    } catch {
      await sleep(300);
    }
  }
  throw new Error(`${nome} non è partito (${url})`);
}

function spegni() {
  for (const p of processi) {
    try {
      p.kill();
    } catch {}
  }
}
process.on('SIGINT', () => { spegni(); process.exit(130); });

const scelta = (process.argv[2] ?? 'tutto').toLowerCase();
const DA_ESEGUIRE = {
  tutto: ['test_completo.mjs', 'test_allarmi.mjs'],
  completo: ['test_completo.mjs'],
  allarmi: ['test_allarmi.mjs'],
  requisiti: ['test_requisiti.mjs'],
  scenario: ['test_scenario.mjs'],
}[scelta];

if (!DA_ESEGUIRE) {
  console.error(`Test sconosciuto: "${scelta}". Usa: tutto | completo | allarmi | requisiti | scenario`);
  process.exit(1);
}

let uscita = 0;
try {
  if (!existsSync(path.join(FRONTEND, 'node_modules'))) {
    console.error('Manca frontend/node_modules: esegui prima "npm install" in frontend.');
    process.exit(1);
  }
  if (!existsSync(path.join(QUI, 'node_modules'))) {
    console.log('Installo gli strumenti di test (solo la prima volta)…');
    await new Promise((r) => avviaNpm(['install'], QUI).on('exit', r));
  }
  if (!existsSync(path.join(FRONTEND, 'dist', 'index.html'))) {
    console.log('Creo la versione reale (npm run build)…');
    await new Promise((r) => avviaNpm(['run', 'build'], FRONTEND).on('exit', r));
  }

  console.log('Avvio i server: dati simulati sulla 5175, versione reale sulla 5174…');
  // Vite si avvia direttamente e non tramite npm: altrimenti alla fine si spegnerebbe npm
  // lasciando acceso Vite, che è il processo figlio.
  const vite = path.join(FRONTEND, 'node_modules', 'vite', 'bin', 'vite.js');
  avvia(process.execPath, [vite, '--port', '5175'], FRONTEND);
  avvia(process.execPath, [vite, 'preview', '--port', '5174'], FRONTEND);
  await Promise.all([
    attendi('http://localhost:5175/', 'il server con i dati simulati'),
    attendi('http://localhost:5174/', 'il server con la versione reale'),
  ]);

  for (const test of DA_ESEGUIRE) {
    console.log(`\n=== ${test} ===`);
    const codice = await new Promise((r) => spawn('node', [test], { cwd: QUI, stdio: 'inherit' }).on('exit', r));
    if (codice !== 0) uscita = 1;
  }
} catch (err) {
  console.error(err.message);
  uscita = 1;
} finally {
  spegni();
}
process.exit(uscita);
