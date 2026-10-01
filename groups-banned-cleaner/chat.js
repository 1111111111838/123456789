// מוזרק לדפי Google Chat (mail.google.com/chat, chat.google.com)
(() => {
  if (window.__CB) return;
  const sleep = ms => new Promise(r => setTimeout(r, ms));
  const norm = e => String(e || '').trim().toLowerCase();
  const EMAIL_RE = /[A-Za-z0-9._%+'\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}/g;
  const FULL = /^[<(\[]?\s*([A-Za-z0-9._%+'\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,})\s*[>)\]]?$/;
  const MGR = /(space manager|manager|owner|מנהל)/i;
  const visible = el => { const r = el.getBoundingClientRect(); return r.width > 0 && r.height > 0; };
  const txt = el => ((el.getAttribute('aria-label') || '') + ' ' + (el.innerText || '')).replace(/\s+/g, ' ').trim();
  const dis = el => el.disabled || el.getAttribute('aria-disabled') === 'true';
  const CLICK = 'button,[role=button],[role=tab],[role=menuitem],[role=option],a,[role=link]';
  const emailsIn = t => (String(t || '').match(EMAIL_RE) || []).map(norm);
  const findBtn = (re, root = document) => [...root.querySelectorAll(CLICK)].find(b => visible(b) && !dis(b) && re.test(txt(b)));
  const realClick = el => { ['pointerdown', 'mousedown', 'pointerup', 'mouseup'].forEach(t => el.dispatchEvent(new MouseEvent(t, { bubbles: true, cancelable: true, view: window }))); el.click(); };
  const hover = el => ['pointerover', 'mouseover', 'mouseenter', 'mousemove'].forEach(t => el.dispatchEvent(new MouseEvent(t, { bubbles: true, view: window })));
  async function waitFor(fn, ms = 4000, step = 100) { const t0 = Date.now(); while (Date.now() - t0 < ms) { const v = fn(); if (v) return v; await sleep(step); } return null; }
  const pressEsc = () => ['keydown', 'keyup'].forEach(t => document.dispatchEvent(new KeyboardEvent(t, { key: 'Escape', code: 'Escape', keyCode: 27, bubbles: true })));

  // ---------- זהות: השוואת כתובות (Gmail: נקודות ו-googlemail זהים) ----------
  const canon = e => {
    e = norm(e); const i = e.lastIndexOf('@'); if (i < 1) return e;
    let l = e.slice(0, i), d = e.slice(i + 1);
    if (d === 'googlemail.com') d = 'gmail.com';
    if (d === 'gmail.com') l = l.replace(/\./g, '');
    return l + '@' + d;
  };
  const noPlus = e => { e = norm(e); const i = e.lastIndexOf('@'); return i > 0 ? e.slice(0, i).split('+')[0] + e.slice(i) : e; };
  // קבוצת מוגנים: מוגן גם אם יש +תגית, נקודות, או googlemail
  const protSet = arr => { const s = new Set(); (arr || []).forEach(e => { if (e && String(e).includes('@')) { s.add(canon(e)); s.add(canon(noPlus(e))); } }); return s; };
  const isProt = (ps, e) => ps.has(canon(e)) || ps.has(canon(noPlus(e)));

  let STOP = false;
  let PROG = { phase: '', found: 0, total: 0 };
  const getProgress = () => PROG;
  const requestStop = () => { STOP = true; return true; };

  function detectMe() {
    const lab = e => e.getAttribute('aria-label') || '';
    let el = [...document.querySelectorAll('[aria-label*="@"]')].find(e => /google account|חשבון google|חשבון גוגל/i.test(lab(e)));
    if (!el) el = document.querySelector('a[href*="SignOutOptions"][aria-label*="@"],a[aria-label*="@"][href*="accounts.google.com"]');
    return el ? norm((lab(el).match(EMAIL_RE) || [])[0]) : '';
  }
  const isScrollable = el => el.scrollHeight > el.clientHeight + 20 && /(auto|scroll)/.test(getComputedStyle(el).overflowY);
  function scrollables() {
    return [...document.querySelectorAll('*')].filter(el => el.scrollHeight > el.clientHeight + 30 && el.clientHeight > 80 && /(auto|scroll)/.test(getComputedStyle(el).overflowY));
  }

  // ---------- רשימת מרחבים מהסרגל הצדדי ----------
  async function listSpaces() {
    const map = new Map();
    const grab = () => {
      document.querySelectorAll('[data-group-id]').forEach(el => {
        const m = (el.getAttribute('data-group-id') || '').match(/^space\/(.+)$/); if (!m) return;
        const name = (el.innerText || '').split('\n').map(s => s.trim()).filter(Boolean)[0] || (el.getAttribute('aria-label') || '').split(',')[0].trim();
        const p = map.get(m[1]); if (!p || (!p.name && name)) map.set(m[1], { id: m[1], name });
      });
      document.querySelectorAll('a[href*="/space/"],a[href*="/room/"]').forEach(a => {
        const m = a.href.match(/\/(?:space|room)\/([A-Za-z0-9_\-]+)/); if (!m) return;
        const name = (a.innerText || '').split('\n').map(s => s.trim()).filter(Boolean)[0] || '';
        const p = map.get(m[1]); if (!p || (!p.name && name)) map.set(m[1], { id: m[1], name });
      });
      return map.size;
    };
    // פתיחת קטע "מרחבים" אם סגור + כפתורי "הצג עוד" בתוך הסרגל
    const expand = () => {
      document.querySelectorAll('[aria-expanded="false"]').forEach(el => { if (/^(spaces|מרחבים)/i.test((el.innerText || el.getAttribute('aria-label') || '').trim())) realClick(el); });
      document.querySelectorAll('[role=navigation] ' + CLICK.split(',').join(',[role=navigation] ')).forEach(b => {
        if (visible(b) && !dis(b) && /^(show more|more spaces|הצג עוד|עוד מרחבים)$/i.test((b.innerText || b.getAttribute('aria-label') || '').trim())) realClick(b);
      });
    };
    expand(); await sleep(800);
    let stable = 0, last = -1;
    for (let i = 0; i < 100 && stable < 4; i++) {
      scrollables().forEach(el => { el.scrollTop += el.clientHeight * 0.8; });
      expand();
      await sleep(300);
      const n = grab(); if (n === last) stable++; else { stable = 0; last = n; }
    }
    scrollables().forEach(el => { el.scrollTop = 0; });
    return [...map.values()];
  }

  // ---------- פאנל חברים ----------
  function scope() {
    const dl = [...document.querySelectorAll('[role=dialog]')].filter(visible).filter(x => emailsIn(x.innerText).length);
    if (dl.length) return dl[dl.length - 1];
    const side = [...document.querySelectorAll('[role=complementary],aside')].filter(visible).find(x => emailsIn(x.innerText).length);
    if (side) return side;
    // פאנל חברים פתוח אבל מסונן לתוצאה ריקה: עדיין מזהים אותו לפי שדה החיפוש שבתוכו
    const withInput = [...document.querySelectorAll('[role=dialog],[role=complementary],aside')].filter(visible).find(x => x.querySelector('input'));
    return withInput || document.body;
  }
  function leafNodes(sc) {
    const out = [], w = document.createTreeWalker(sc, NodeFilter.SHOW_TEXT);
    while (w.nextNode()) {
      const n = w.currentNode, m = n.nodeValue.trim().match(FULL); if (!m) continue;
      const el = n.parentElement; if (!el || el.closest('[role=banner],header') || !visible(el)) continue;
      out.push({ el, email: norm(m[1]), raw: m[1] });
    }
    return out;
  }
  function rowOf(el, sc) {
    let cur = el, best = el;
    for (let i = 0; i < 8 && cur && cur !== sc && cur !== document.body; i++) {
      if (new Set(emailsIn(cur.innerText)).size > 1) break;
      best = cur; cur = cur.parentElement;
    }
    return best;
  }
  function rowFor(email) {
    const sc = scope(); const c = canon(email); const hit = leafNodes(sc).find(x => canon(x.email) === c);
    return hit ? { row: rowOf(hit.el, sc), el: hit.el } : null;
  }
  const inPopup = el => !!el.closest('[role=listbox],[role=menu],[role=tooltip]');
  const loading = () => { const sc = scope(); return [...sc.querySelectorAll('[role=progressbar],[aria-busy=true]')].some(visible); };
  const panelSig = () => { const sc = scope(); return sc === document.body ? '' : leafNodes(sc).slice(0, 12).map(x => x.email).join('|'); };

  // מספר החברים שהפאנל מצהיר עליו (למשל "חברים (1,234)")
  function declaredTotal() {
    const sc = scope(); if (sc === document.body) return 0;
    const t = (sc.innerText || '').slice(0, 700), n = s => +String(s).replace(/[^\d]/g, '');
    let m = t.match(/(?:members|people|חברים|משתתפים|אנשים)\s*[(\-:·]?\s*(\d[\d,.]*)/i); if (m) return n(m[1]);
    m = t.match(/(\d[\d,.]*)\s*(?:members|people|חברים|משתתפים|אנשים)/i); if (m) return n(m[1]);
    return 0;
  }

  async function goSpace(id, name) {
    const prev = panelSig();
    pressEsc(); await sleep(250);
    document.querySelectorAll('[role=dialog] [aria-label*="Close"],[role=dialog] [aria-label*="סגור"]').forEach(b => { try { b.click(); } catch {} });
    const prev2 = panelSig() || prev;
    location.hash = '#chat/space/' + id;
    await waitFor(() => location.hash.includes(id), 4000);
    const head = (name || '').slice(0, 18);
    // מחכים שהפאנל הישן ייעלם / יתחלף כדי לא לקרוא בטעות את המרחב הקודם
    if (prev2) await waitFor(() => panelSig() !== prev2, 4500);
    else await waitFor(() => head.length < 3 || document.body.innerText.includes(head), 4500);
    await sleep(450);
    return { url: location.href, titleOk: !head || document.body.innerText.includes(head) };
  }
  async function openMembers() {
    const ready = () => leafNodes(scope()).length >= 1;
    if (ready() && scope() !== document.body) return { ok: true, how: 'כבר פתוח' };
    const top = b => b.getBoundingClientRect().top < 220 && !b.closest('[role=navigation]');
    const tries = [
      async () => { const b = [...document.querySelectorAll(CLICK)].find(b => visible(b) && !dis(b) && top(b) && /(members|people|חברים|אנשים|משתתפים)/i.test(txt(b))); if (b) { realClick(b); return 'כפתור חברים'; } },
      async () => {
        const h = [...document.querySelectorAll(CLICK)].find(b => visible(b) && !dis(b) && top(b) && /(space (menu|options|settings|name)|more|עוד|אפשרויות|הגדרות|מרחב|▾)/i.test(txt(b)) || (b.getAttribute('aria-haspopup') && top(b)));
        if (!h) return; realClick(h); await sleep(700);
        const mi = [...document.querySelectorAll('[role=menuitem],[role=option],[role=menu] *')].find(x => visible(x) && /(members|חברים|manage members|ניהול חברים)/i.test(txt(x)));
        if (mi) { realClick(mi); return 'תפריט מרחב'; }
      }
    ];
    for (const t of tries) { const how = await t(); if (how && await waitFor(ready, 4000)) return { ok: true, how }; pressEsc(); await sleep(250); }
    return { ok: false, how: 'לא נמצא כפתור חברים' };
  }

  // ---------- קריאה מלאה של הרשימה (גלילה בצעדים, מתמיד עד הסוף האמיתי) ----------
  async function readAll(maxMs = 25 * 60 * 1000) {
    const found = new Map(), total = declaredTotal(), t0 = Date.now();
    PROG = { phase: 'קורא רשימה', found: 0, total };
    const tick = () => {
      const sc = scope();
      leafNodes(sc).forEach(({ el, email, raw }) => {
        const mgr = MGR.test(rowOf(el, sc).innerText), p = found.get(email);
        found.set(email, { email, raw, mgr: (p && p.mgr) || mgr });
      });
      PROG.found = found.size; return found.size;
    };
    const findScroller = () => {
      const first = leafNodes(scope())[0]; let el = first && first.el;
      while (el && el !== document.body && !isScrollable(el)) el = el.parentElement;
      if (el && el !== document.body) return el;
      const sc = scope();
      return scrollables().filter(s => sc.contains(s) || s.contains(sc)).sort((a, b) => b.scrollHeight - a.scrollHeight)[0] || null;
    };
    tick();
    let sc = findScroller();
    if (sc) {
      sc.scrollTop = 0; await sleep(250); tick();
      let lastProgress = Date.now(), it = 0;
      while (!STOP && Date.now() - t0 < maxMs) {
        if (++it % 12 === 0 || !sc.isConnected) sc = findScroller() || sc;
        const n0 = found.size, h0 = sc.scrollHeight, y0 = sc.scrollTop;
        sc.scrollTop = Math.min(y0 + Math.max(sc.clientHeight * 0.8, 120), sc.scrollHeight);
        const more = findBtn(/load more|show more|הצג עוד|טען עוד/i, scope()); if (more) realClick(more);
        await sleep(110); tick();
        if (found.size > n0 || sc.scrollHeight > h0 || sc.scrollTop > y0 + 1) lastProgress = Date.now();
        if (sc.scrollTop + sc.clientHeight >= sc.scrollHeight - 4) {
          const need = (total && found.size < total * 0.98) ? 8000 : 2500;
          if (Date.now() - lastProgress > (loading() ? need * 2 : need)) break;
          await sleep(150);
        }
      }
      sc.scrollTop = 0;
    }
    return { members: [...found.values()], total };
  }

  // ---------- חיפוש בהדבקה: מדביקים כל כתובת בשדה הסינון של החברים ----------
  function findFilter() {
    const sc = scope(); if (sc === document.body) return null;
    const cand = [...sc.querySelectorAll('input,[role=searchbox],[role=combobox],[role=textbox][contenteditable=true]')].filter(el => visible(el) && !dis(el) && (el.type || '') !== 'checkbox' && el.type !== 'hidden');
    const lab = el => ((el.getAttribute('aria-label') || '') + ' ' + (el.getAttribute('placeholder') || '') + ' ' + (el.name || '')).toLowerCase();
    const ok = cand.filter(el => !/(add|invite|הוסף|הוספת|הזמן|הזמנ|message|הודעה|reply|תגובה)/i.test(lab(el)));
    return ok.find(el => /(search|filter|find|חיפוש|חפש|סינון|סנן)/i.test(lab(el))) || ok[0] || null;
  }
  function setText(inp, text) {
    inp.focus();
    let done = false;
    if (text) {
      try { const dt = new DataTransfer(); dt.setData('text/plain', text); inp.dispatchEvent(new ClipboardEvent('paste', { clipboardData: dt, bubbles: true, cancelable: true })); } catch {}
    }
    try { if (inp.select) inp.select(); document.execCommand('selectAll'); done = text ? document.execCommand('insertText', false, text) : document.execCommand('delete'); } catch {}
    const cur = inp.isContentEditable ? inp.innerText : inp.value;
    if (!done || norm(cur) !== norm(text)) {
      if (inp.isContentEditable) inp.textContent = text;
      else { const d = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(inp), 'value'); if (d && d.set) d.set.call(inp, text); else inp.value = text; }
      inp.dispatchEvent(new InputEvent('input', { bubbles: true, inputType: text ? 'insertFromPaste' : 'deleteContentBackward', data: text || null }));
      inp.dispatchEvent(new Event('change', { bubbles: true }));
    }
  }
  // מדביק כתובת ומחזיר את השורה אם היא חבר במרחב
  async function probe(getInp, email, popupOk) {
    const c = canon(email);
    let inp = getInp(); if (!inp) return { err: true };
    setText(inp, email);
    const t0 = Date.now(); let last = '', stable = 0;
    while (Date.now() - t0 < 3500) {
      await sleep(80);
      const sc = scope();
      const hit = leafNodes(sc).find(x => canon(x.email) === c && (popupOk || !inPopup(x.el)));
      if (hit) { const row = rowOf(hit.el, sc); return { hit: true, mgr: MGR.test(row.innerText), popup: inPopup(hit.el), shown: hit.email }; }
      const sig = leafNodes(sc).map(x => x.email).join('|');
      if (sig === last) stable++; else { stable = 0; last = sig; }
      // תשובה שלילית נחשבת רק אחרי שהרשימה התייצבה ולא טוענת (מניעת פספוסים)
      if (Date.now() - t0 >= 600 && stable >= 4 && !loading()) return { hit: false };
    }
    return { hit: false };
  }
  async function searchScan(want, protList, notes) {
    const cur = { el: findFilter() };
    const getInp = () => { if (!cur.el || !cur.el.isConnected || !visible(cur.el)) cur.el = findFilter(); return cur.el; };
    if (!getInp()) { notes.push('לא נמצא שדה סינון'); return null; }
    PROG = { phase: 'בדיקת שדה חיפוש', found: 0, total: want.size };
    // בדיקה עצמית: המייל שלי חייב להימצא, וכתובת בדויה לא
    let me = null, mePop = false; const seen = new Set();
    for (const p of protList) {
      const c = canon(p); if (seen.has(c) || !p.includes('@')) continue; seen.add(c); if (seen.size > 4) break;
      const r = await probe(getInp, p, true);
      if (r.err) { notes.push('שדה חיפוש נעלם'); return null; }
      if (r.hit) { me = { mgr: r.mgr }; mePop = r.popup; break; }
    }
    if (!me) { notes.push('בדיקה עצמית נכשלה (המייל שלי לא נמצא בחיפוש)'); setText(getInp() || cur.el, ''); return null; }
    const ctl = await probe(getInp, `zz${Math.random().toString(36).slice(2, 8)}-nomember@example.invalid`, mePop);
    if (ctl.hit) { notes.push('בדיקת ביקורת נכשלה – החיפוש לא אמין'); setText(getInp() || cur.el, ''); return null; }
    const found = []; let i = 0;
    for (const [c, orig] of want) {
      if (STOP) break;
      PROG = { phase: 'מדביק ובודק', found: found.length, total: want.size, i: ++i };
      const r = await probe(getInp, orig, mePop);
      if (r.err) { notes.push('שדה חיפוש נעלם באמצע'); return null; }
      if (r.hit) found.push({ email: orig, shown: r.shown, mgr: r.mgr });
    }
    const inp = getInp(); if (inp) { setText(inp, ''); await sleep(300); }
    return { found, me };
  }

  // סריקה של מרחב אחד: בוחר שיטה, מחזיר את החסומים שנמצאו
  async function scanMembers(opt) {
    STOP = false;
    const want = new Map(); (opt.banned || []).forEach(e => { const c = canon(e); if (e && !want.has(c)) want.set(c, norm(e)); });
    const protList = opt.prot || [], ps = protSet(protList), mode = opt.mode || 'auto';
    const total = declaredTotal(), notes = []; let method = 'full', found = [], meSeen = false, meMgr = null, seen = 0, complete = true;
    const toFound = (m) => ({ email: want.get(canon(m.email)) || m.email, shown: m.shown || m.email, mgr: !!m.mgr });
    if (want.size && findFilter() && (mode === 'search' || (mode === 'auto' && total >= 200 && total > want.size * 15))) {
      const r = await searchScan(want, protList, notes);
      if (r) { method = 'search'; found = r.found.map(toFound); meSeen = true; meMgr = r.me.mgr; seen = total; }
    }
    if (method === 'full') {
      const r = await readAll(); seen = r.members.length;
      const me = r.members.find(m => isProt(ps, m.email));
      meSeen = !!me; meMgr = me ? !!me.mgr : null;
      found = r.members.filter(m => want.has(canon(m.email)) && !isProt(ps, m.email)).map(toFound);
      complete = !total || seen >= total * 0.97;
      // רשימה חלקית – משלימים בהדבקה אם אפשר
      if (!complete && want.size && mode !== 'full' && findFilter()) {
        const s = await searchScan(want, protList, notes);
        if (s) { const have = new Set(found.map(f => canon(f.email))); s.found.forEach(f => { if (!have.has(canon(f.email))) found.push(toFound(f)); }); complete = true; method = 'full+search'; if (!meSeen) { meSeen = true; meMgr = s.me.mgr; } }
      }
    }
    const addBtn = !!findBtn(/add (people|members)|הוספת (אנשים|חברים)|הוסף (אנשים|חברים)/i);
    PROG = { phase: 'סיום', found: found.length, total };
    return { total, seen, found, meSeen, meMgr, addBtn, method, complete, notes };
  }
  // תאימות לאחור
  async function readMembers() { const r = await readAll(); return { members: r.members, addBtn: !!findBtn(/add (people|members)|הוספת (אנשים|חברים)|הוסף (אנשים|חברים)/i) }; }

  async function removeMember(email, protList) {
    email = norm(email);
    const ps = protSet(Array.isArray(protList) ? protList : [protList]);
    if (!ps.size) return { error: 'חסר מייל שלי – ביטול' };
    if (isProt(ps, email)) return { error: 'כתובת מוגנת (המייל שלי) – חסום מטעמי בטיחות' };
    let info = rowFor(email);
    if (!info) { // בשדה הסינון: הדבקת הכתובת מציגה אותה מיד גם במרחב ענק
      const inp = findFilter();
      if (inp) { setText(inp, email); info = await waitFor(() => rowFor(email), 3500, 120); }
    }
    if (!info) { // גלישה: אולי דורש גלילה
      const sc = scrollables().find(s => s.contains(scope()) || scope().contains(s));
      if (sc) { sc.scrollTop = 0; for (let i = 0; i < 400 && !info; i++) { sc.scrollTop += sc.clientHeight * 0.8; await sleep(120); info = rowFor(email); if (sc.scrollTop + sc.clientHeight >= sc.scrollHeight - 4 && !info) { await sleep(500); info = rowFor(email); break; } } }
    }
    if (!info) return { error: 'לא נמצאה שורה עם הכתובת בפאנל' };
    const { row } = info;
    if (emailsIn(row.innerText).some(e => isProt(ps, e))) return { error: 'השורה כוללת כתובת מוגנת – בוטל' };
    row.scrollIntoView({ block: 'center' }); hover(row); hover(info.el); await sleep(350);
    let btn = findBtn(/remove|הסר|הסרה/i, row);
    if (!btn) {
      const more = findBtn(/more|עוד|options|אפשרויות|actions|פעולות/i, row);
      if (more) { realClick(more); await sleep(500); btn = [...document.querySelectorAll('[role=menuitem],[role=option]')].find(x => visible(x) && /remove|הסר|הסרה/i.test(txt(x))); }
    }
    if (!btn) return { error: 'כפתור הסרה לא נמצא בשורה (ייתכן שאינך מנהל כאן)' };
    realClick(btn); await sleep(800);
    const dl = [...document.querySelectorAll('[role=alertdialog],[role=dialog]')].filter(visible);
    const last = dl[dl.length - 1];
    if (last && !last.contains(row)) {
      if (emailsIn(last.innerText).some(e => isProt(ps, e))) { const c = findBtn(/cancel|ביטול/i, last); if (c) realClick(c); return { error: 'כתובת מוגנת הופיעה באישור – בוטל' }; }
      const ok = findBtn(/^(remove|הסר|הסרה|remove member|הסר חבר)$/i, last) || findBtn(/remove|הסר|confirm|אישור|yes|כן/i, last);
      if (!ok) return { error: 'כפתור אישור לא נמצא' };
      realClick(ok);
    }
    await sleep(1200);
    const gone = await waitFor(() => !rowFor(email), 5000, 150);
    const inp = findFilter(); if (inp && (inp.value || '')) { setText(inp, ''); await sleep(250); await waitFor(() => leafNodes(scope()).length > 0, 3000, 120); }
    return gone ? { ok: true } : { error: 'נלחץ אישור אך הכתובת עדיין מופיעה' };
  }

  // אבחון – אוסף מידע על המסך כדי לכוון את התוסף
  function diagnose() {
    const L = []; L.push('URL: ' + location.href);
    L.push('data-group-id items: ' + document.querySelectorAll('[data-group-id]').length + ' | spaces: ' + [...document.querySelectorAll('[data-group-id]')].filter(e => /^space\//.test(e.getAttribute('data-group-id'))).length);
    L.push('me: ' + detectMe());
    const sc = scope(); L.push('scope: ' + sc.tagName + ' role=' + sc.getAttribute('role') + ' emails=' + leafNodes(sc).length + ' declaredTotal=' + declaredTotal());
    const f = findFilter(); L.push('filter: ' + (f ? (f.tagName + ' aria=' + f.getAttribute('aria-label') + ' ph=' + f.getAttribute('placeholder')) : 'none'));
    leafNodes(sc).slice(0, 3).forEach(x => { const r = rowOf(x.el, sc); L.push('row sample: ' + r.innerText.replace(/\s+/g, ' ').slice(0, 160) + ' | buttons: ' + [...r.querySelectorAll(CLICK)].map(txt).join(' / ').slice(0, 160)); });
    const labels = [...new Set([...document.querySelectorAll(CLICK)].filter(visible).map(b => txt(b).slice(0, 60)).filter(Boolean))];
    L.push('buttons(' + labels.length + '): ' + labels.slice(0, 80).join(' | '));
    L.push('dialogs: ' + [...document.querySelectorAll('[role=dialog],[role=alertdialog]')].filter(visible).map(d => d.innerText.replace(/\s+/g, ' ').slice(0, 200)).join(' || '));
    return L.join('\n');
  }
  window.__CB = { detectMe, listSpaces, goSpace, openMembers, readMembers, scanMembers, removeMember, diagnose, getProgress, requestStop };
})();
