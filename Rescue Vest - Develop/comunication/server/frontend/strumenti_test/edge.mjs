// Dove si trova Microsoft Edge. Si può indicare a mano:  $env:EDGE_PATH = 'C:\...\msedge.exe'
import { existsSync } from 'node:fs';

const CANDIDATI = [
  process.env.EDGE_PATH,
  'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
  process.env.LOCALAPPDATA && `${process.env.LOCALAPPDATA}\\Microsoft\\Edge\\Application\\msedge.exe`,
  'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
  '/usr/bin/microsoft-edge',
  '/usr/bin/google-chrome',
  '/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge',
].filter(Boolean);

export function trovaBrowser() {
  const trovato = CANDIDATI.find((p) => existsSync(p));
  if (trovato) return trovato;
  console.error(
    'Nessun browser trovato. Indica dove si trova Edge o Chrome con:\n' +
      "  PowerShell:  $env:EDGE_PATH = 'C:\\percorso\\msedge.exe'\n" +
      "  bash:        export EDGE_PATH=/percorso/msedge",
  );
  process.exit(1);
}
