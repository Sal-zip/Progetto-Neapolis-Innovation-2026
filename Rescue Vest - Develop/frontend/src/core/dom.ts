// Aggiorna il contenuto di `target` modificando solo ciò che è cambiato. Gli elementi già
// presenti restano gli stessi: un click iniziato durante un aggiornamento non va perso e il
// focus da tastiera resta dov'era. Gli elementi con `data-key` diversi vengono sostituiti.
export function patch(target: Element, html: string): void {
  const next = target.cloneNode(false) as Element;
  next.innerHTML = html;
  morphChildren(target, next);
}

function sameNode(a: Node, b: Node): boolean {
  if (a.nodeType !== b.nodeType) return false;
  if (a.nodeType !== Node.ELEMENT_NODE) return true;
  const ea = a as Element;
  const eb = b as Element;
  return ea.tagName === eb.tagName && ea.getAttribute('data-key') === eb.getAttribute('data-key');
}

function morphNode(a: Node, b: Node): void {
  if (a.nodeType !== Node.ELEMENT_NODE) {
    if (a.nodeValue !== b.nodeValue) a.nodeValue = b.nodeValue;
    return;
  }
  const ea = a as Element;
  const eb = b as Element;
  for (const { name } of Array.from(ea.attributes)) {
    if (!eb.hasAttribute(name)) ea.removeAttribute(name);
  }
  for (const { name, value } of Array.from(eb.attributes)) {
    if (ea.getAttribute(name) !== value) ea.setAttribute(name, value);
  }
  morphChildren(ea, eb);
}

function morphChildren(from: Node, to: Node): void {
  let a = from.firstChild;
  let b = to.firstChild;
  while (b) {
    const nextB = b.nextSibling;
    if (!a) {
      from.appendChild(b);
    } else if (sameNode(a, b)) {
      morphNode(a, b);
      a = a.nextSibling;
    } else {
      const nextA = a.nextSibling;
      from.replaceChild(b, a);
      a = nextA;
    }
    b = nextB;
  }
  while (a) {
    const nextA = a.nextSibling;
    from.removeChild(a);
    a = nextA;
  }
}
