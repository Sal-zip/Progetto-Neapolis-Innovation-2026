interface Page {
  mount(el: HTMLElement): () => void;
}

interface Route {
  title: string;
  load: () => Promise<Page>;
}

export const ROUTES: Record<string, Route> = {
  '/': { title: 'Dashboard operativa', load: () => import('../pages/dashboard') },
  '/mappa': { title: 'Mappa live', load: () => import('../pages/mappa') },
  '/eventi': { title: 'Storico eventi', load: () => import('../pages/eventi') },
  '/dispositivi': { title: 'Dispositivi', load: () => import('../pages/dispositivi') },
  '/operatori': { title: 'Operatori', load: () => import('../pages/operatori') },
  '/assistente': { title: 'Assistente AI', load: () => import('../pages/assistente') },
};

let view: HTMLElement;
let cleanup: (() => void) | null = null;
let onRoute: (path: string, title: string) => void = () => {};
// Ogni navigazione riceve un numero: se nel frattempo ne è partita un'altra, la vecchia viene
// scartata anche se la sua pagina finisce di caricarsi dopo (es. Mappa e subito Eventi).
let navToken = 0;

async function render(path: string): Promise<void> {
  const token = ++navToken;
  const known = Object.hasOwn(ROUTES, path);
  const target = known ? path : '/';
  if (!known) history.replaceState({}, '', target);
  const route = ROUTES[target];

  let page: Page;
  try {
    page = await route.load();
  } catch (err) {
    console.error('Caricamento pagina fallito', err);
    if (token !== navToken) return;
    cleanup?.();
    cleanup = null;
    view.innerHTML = '<div role="alert" class="alert alert-error">Impossibile caricare la pagina. Ricarica il browser.</div>';
    return;
  }
  if (token !== navToken) return;

  cleanup?.();
  cleanup = null;
  view.innerHTML = '';
  cleanup = page.mount(view);
  onRoute(target, route.title);
}

export function navigate(path: string): void {
  if (path === location.pathname) return;
  history.pushState({}, '', path);
  void render(path);
}

export function startRouter(target: HTMLElement, routeChanged: (path: string, title: string) => void): void {
  view = target;
  onRoute = routeChanged;

  document.addEventListener('click', (e) => {
    if (e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
    const link = (e.target as Element).closest<HTMLAnchorElement>('a[data-link]');
    if (!link) return;
    e.preventDefault();
    navigate(link.getAttribute('href') ?? '/');
  });

  window.addEventListener('popstate', () => void render(location.pathname));
  void render(location.pathname);
}
