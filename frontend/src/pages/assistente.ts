import { askAi } from '../core/api';
import { esc } from '../core/util';

const EXAMPLES = ['Quanti SOS ci sono stati?', 'Qual è la batteria media della flotta?'];
const WELCOME = `<div class="chat chat-start"><div class="chat-bubble bg-base-200 text-base-content">
  Ciao, puoi farmi domande sui dati in linguaggio naturale. Mostrerò anche la query SQL usata.
</div></div>`;

// La conversazione vive fuori dalla pagina: cambiando pagina e tornando indietro non si perde,
// e una risposta arrivata mentre si era altrove compare al ritorno.
const messages: string[] = [WELCOME];
let pending = 0;
let thread: HTMLElement | null = null;

function renderThread(): void {
  if (!thread) return;
  const loader = pending
    ? '<div class="chat chat-start"><div class="chat-bubble bg-base-200 text-base-content"><span class="loading loading-dots loading-sm"></span></div></div>'
    : '';
  thread.innerHTML = messages.join('') + loader;
  thread.scrollTop = thread.scrollHeight;
}

function table(rows: Record<string, unknown>[]): string {
  if (!rows.length) return '';
  const cols = Object.keys(rows[0]);
  return `<div class="overflow-x-auto mt-2"><table class="table table-xs"><thead><tr>${cols.map((c) => `<th>${esc(c)}</th>`).join('')}</tr></thead><tbody>${rows
    .slice(0, 20)
    .map((r) => `<tr>${cols.map((c) => `<td>${esc(r[c])}</td>`).join('')}</tr>`)
    .join('')}</tbody></table></div>`;
}

async function ask(question: string): Promise<void> {
  messages.push(`<div class="chat chat-end"><div class="chat-bubble chat-bubble-primary">${esc(question)}</div></div>`);
  pending++;
  renderThread();
  try {
    const res = await askAi(question);
    messages.push(`<div class="chat chat-start"><div class="chat-bubble bg-base-200 text-base-content max-w-[85%]">
      ${esc(res.answer)}
      ${res.sql ? `<details class="mt-2"><summary class="cursor-pointer text-xs opacity-70">Query SQL</summary><pre class="text-xs mt-1 whitespace-pre-wrap font-mono">${esc(res.sql)}</pre></details>` : ''}
      ${table(res.rows)}
    </div></div>`);
  } catch (err) {
    messages.push(`<div class="chat chat-start"><div class="chat-bubble chat-bubble-error">Errore: ${esc((err as Error).message)}</div></div>`);
  } finally {
    pending--;
    renderThread();
  }
}

export function mount(el: HTMLElement): () => void {
  const root = document.createElement('div');
  root.className = 'card bg-base-100 shadow-sm border border-base-300 h-[calc(100vh-9rem)] min-h-[28rem]';
  root.innerHTML = `
    <div class="p-5 flex flex-col gap-3 h-full min-h-0">
      <div id="thread" class="flex-1 overflow-y-auto flex flex-col gap-3 pr-1"></div>
      <div class="flex flex-wrap gap-2">
        ${EXAMPLES.map((q) => `<button type="button" class="btn btn-xs btn-outline rounded-full" data-example="${esc(q)}">${esc(q)}</button>`).join('')}
      </div>
      <form id="ask" class="flex gap-2">
        <input id="q" class="input input-bordered flex-1 rounded-full" aria-label="Domanda" placeholder='Chiedi qualcosa sui dati… es. "quali gilet hanno batteria sotto il 20%?"' autocomplete="off" />
        <button class="btn btn-primary rounded-full" type="submit">Invia</button>
      </form>
      <p class="text-xs text-base-content/50">Le risposte sono generate da un modello Text-to-SQL in sola lettura sul database.</p>
    </div>`;
  el.appendChild(root);

  thread = root.querySelector<HTMLElement>('#thread')!;
  const form = root.querySelector<HTMLFormElement>('#ask')!;
  const input = root.querySelector<HTMLInputElement>('#q')!;
  renderThread();

  form.addEventListener('submit', (e) => {
    e.preventDefault();
    const q = input.value.trim();
    if (!q) return;
    input.value = '';
    void ask(q);
  });

  root.addEventListener('click', (e) => {
    const ex = (e.target as Element).closest<HTMLElement>('[data-example]');
    if (ex) void ask(ex.dataset.example!);
  });

  return () => {
    thread = null;
  };
}
