// מוזרק רק לעמוד החסומים ב-groups.google.com ושואב את הכתובות בדיוק
(() => {
  const sleep = ms => new Promise(r => setTimeout(r, ms));
  const norm = e => String(e || '').trim().toLowerCase();
  const FULL = /^[<(\[]?\s*([A-Za-z0-9._%+'\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,})\s*[>)\]]?$/;
  const EMAIL_RE = /[A-Za-z0-9._%+'\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}/g;
  const visible = el => { const r = el.getBoundingClientRect(); return r.width > 0 && r.height > 0; };
  const btnText = el => ((el.getAttribute('aria-label') || '') + ' ' + (el.getAttribute('data-tooltip') || '') + ' ' + (el.getAttribute('title') || '') + ' ' + (el.innerText || '')).replace(/\s+/g, ' ').trim();
  const isDisabled = el => el.disabled || el.getAttribute('aria-disabled') === 'true';
  const CHROME = 'header,nav,[role=banner],[role=navigation],[role=dialog],[role=menu],[role=tooltip],[aria-hidden=true]';
  const getRoot = () => document.querySelector('[role=main],main') || document.body;
  const loading = () => [...document.querySelectorAll('[role=progressbar],[aria-busy=true]')].some(visible);
  const ROWS = 'tr,[role=row],[role=listitem],li';

  // שיטה א': כל טקסט שהוא כתובת מייל בלבד (תא מייל בטבלה)
  function leafEmails() {
    const out = [], w = document.createTreeWalker(getRoot(), NodeFilter.SHOW_TEXT);
    while (w.nextNode()) {
      const n = w.currentNode, m = n.nodeValue.trim().match(FULL);
      if (!m) continue;
      const el = n.parentElement;
      if (!el || el.closest(CHROME) || !visible(el)) continue;
      out.push(m[1]);
    }
    return out;
  }
  // שיטה ב': שורות שיש בהן בדיוק מייל אחד (טקסט + מאפיינים כמו aria-label / title / mailto)
  function rowEmails() {
    const out = [];
    getRoot().querySelectorAll(ROWS).forEach(el => {
      if (el.closest(CHROME) || !visible(el)) return;
      const all = new Map();
      (el.innerText.match(EMAIL_RE) || []).forEach(x => all.set(norm(x), x));
      el.querySelectorAll('[aria-label],[title],[data-email],[data-member-email],[data-identifier],a[href^="mailto:"]').forEach(c => {
        ['aria-label', 'title', 'data-email', 'data-member-email', 'data-identifier', 'href'].forEach(a => {
          const v = c.getAttribute(a); if (v) (v.match(EMAIL_RE) || []).forEach(x => all.set(norm(x), x));
        });
      });
      if (all.size === 1) out.push([...all.values()][0]);
    });
    return out;
  }
  function mainScroller() {
    const c = [...document.querySelectorAll('*')].filter(el => el.scrollHeight > el.clientHeight + 30 &&
      el.clientHeight > 150 && /(auto|scroll)/.test(getComputedStyle(el).overflowY));
    const se = document.scrollingElement;
    if (se && se.scrollHeight > se.clientHeight + 30) c.push(se);
    c.sort((a, b) => b.scrollHeight - a.scrollHeight);
    return c[0] || se;
  }
  function clickLoadMore() {
    const b = [...document.querySelectorAll('button,[role=button]')].find(x => visible(x) && !isDisabled(x) &&
      /load more|show more|הצג עוד|טען עוד|עוד תוצאות/i.test(btnText(x)));
    if (b) { b.click(); return true; } return false;
  }
  function nextPageBtn() {
    const ok = b => visible(b) && !isDisabled(b) && !b.closest('[role=menu],[role=dialog]') &&
      /next|הבא|הלאה/i.test(btnText(b)) && !/prev|קודם/i.test(btnText(b));
    const sel = 'button,[role=button],a';
    const inRoot = [...getRoot().querySelectorAll(sel)].filter(ok);
    return inRoot[inRoot.length - 1] || [...document.querySelectorAll(sel)].filter(ok).filter(b => !b.closest('header,nav,[role=banner],[role=navigation]')).pop();
  }
  // מגדיל את מספר השורות בעמוד (אם קיים בורר כזה) – פחות עמודים, פחות סיכון לפספוס
  async function maxPageSize() {
    const re = /rows per page|items per page|results per page|per page|שורות בעמוד|פריטים בעמוד|תוצאות בעמוד|בעמוד/i;
    const numOf = s => { const m = String(s).match(/\d+/); return m ? +m[0] : 0; };
    try {
      const sel = [...document.querySelectorAll('select')].find(s => visible(s) && re.test(btnText(s) + ' ' + (s.getAttribute('aria-label') || '')));
      if (sel) { const best = [...sel.options].sort((a, b) => numOf(b.text) - numOf(a.text))[0]; if (best && sel.value !== best.value) { sel.value = best.value; sel.dispatchEvent(new Event('change', { bubbles: true })); await sleep(2000); } return; }
      const cb = [...document.querySelectorAll('[role=combobox],[role=button],[aria-haspopup]')].find(x => visible(x) && !isDisabled(x) && re.test(btnText(x)));
      if (!cb) return;
      cb.click(); await sleep(600);
      const opts = [...document.querySelectorAll('[role=option],[role=menuitem],[role=listbox] li')].filter(o => visible(o) && numOf(o.innerText) > 0).sort((a, b) => numOf(b.innerText) - numOf(a.innerText));
      if (opts[0]) { opts[0].click(); await sleep(2000); } else cb.click();
    } catch {}
  }
  // גלילה בצעדים, איסוף בכל צעד (תומך ברשימות וירטואליות), עד סוף אמיתי (מחכה לטעינה)
  async function scrollPass(tick, count) {
    const sc = mainScroller();
    sc.scrollTop = 0; await sleep(500); tick();
    let lastProgress = Date.now();
    for (let i = 0; i < 2000; i++) {
      const n0 = count(), h0 = sc.scrollHeight, y0 = sc.scrollTop;
      sc.scrollTop = Math.min(y0 + Math.max(sc.clientHeight * 0.6, 120), sc.scrollHeight);
      clickLoadMore();
      await sleep(250); tick();
      if (count() > n0 || sc.scrollHeight > h0 || sc.scrollTop > y0 + 1) lastProgress = Date.now();
      if (sc.scrollTop + sc.clientHeight >= sc.scrollHeight - 4) {
        if (Date.now() - lastProgress > (loading() ? 7000 : 2500)) break;
        await sleep(200);
      }
    }
    sc.scrollTop = 0;
  }
  function declaredCandidates() {
    const t = document.body.innerText, found = new Set(), n = s => +String(s).replace(/[^\d]/g, '');
    const res = [/(?:banned|חסומים|חסום)[^\d\n]{0,14}\(?(\d[\d,.]*)\)?/gi, /(\d[\d,.]*)\s*(?:banned|חסומים|חסום)/gi, /(?:of|מתוך)\s+(\d[\d,.]*)/gi];
    res.forEach(re => { let m; while ((m = re.exec(t))) { const v = n(m[1]); if (v > 0 && v < 1e6) found.add(v); } });
    return [...found];
  }
  function detectMe() {
    const el = [...document.querySelectorAll('[aria-label*="@"]')].find(e => /google account|חשבון google|חשבון גוגל/i.test(e.getAttribute('aria-label')));
    return el ? norm((el.getAttribute('aria-label').match(EMAIL_RE) || [])[0]) : '';
  }

  async function scrapeBanned() {
    const leaf = new Map(), rows = new Map();
    const tick = () => {
      leafEmails().forEach(e => { const k = norm(e); if (!leaf.has(k)) leaf.set(k, e); });
      rowEmails().forEach(e => { const k = norm(e); if (!rows.has(k)) rows.set(k, e); });
    };
    const sig = () => leafEmails().slice(0, 6).map(norm).join('|');
    await maxPageSize();
    let pages = 0;
    for (let p = 0; p < 500; p++) {
      pages++;
      const before = new Set([...leaf.keys(), ...rows.keys()]).size;
      await scrollPass(tick, () => leaf.size + rows.size);
      if (p > 0 && new Set([...leaf.keys(), ...rows.keys()]).size === before) break;     // עמוד שלא הוסיף כלום – סוף
      const nb = nextPageBtn();
      if (!nb) break;
      const s0 = sig();
      nb.click();
      for (let w = 0; w < 40 && sig() === s0; w++) await sleep(200);   // מחכים שהעמוד באמת התחלף
      await sleep(700);
    }
    const gname = (location.pathname.match(/\/g\/([^\/]+)/) || [])[1] || '';
    const union = new Map([...leaf, ...rows]);
    return {
      leaf: [...leaf].map(([k, v]) => ({ email: k, raw: v })),
      rows: [...rows].map(([k, v]) => ({ email: k, raw: v })),
      union: [...union].map(([k, v]) => ({ email: k, raw: v })),
      declared: declaredCandidates(), pages, url: location.href, gname, me: detectMe()
    };
  }
  window.__GB = { scrapeBanned, detectMe };
})();
