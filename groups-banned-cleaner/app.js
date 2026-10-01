const $ = s => document.querySelector(s);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const norm = e => String(e || '').trim().toLowerCase();
const EMAIL_RE = /[A-Za-z0-9._%+'\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}/g;
const VALID = /^[^@\s]+@[^@\s]+\.[a-z]{2,}$/;
const CHAT = 'https://mail.google.com/chat/u/0/';
const S = { me: '', meAuto: true, extra: [], cands: [], actor: '', mode: 'auto', banned: [], bannedMeta: null, spaces: [], results: [], stop: false, busy: false, tabId: null };

// ---------- זהות כתובות: Gmail – נקודות ו-googlemail זהים; מוגן גם בלי +תגית ----------
const canon = e => {
  e = norm(e); const i = e.lastIndexOf('@'); if (i < 1) return e;
  let l = e.slice(0, i), d = e.slice(i + 1);
  if (d === 'googlemail.com') d = 'gmail.com';
  if (d === 'gmail.com') l = l.replace(/\./g, '');
  return l + '@' + d;
};
const noPlus = e => { e = norm(e); const i = e.lastIndexOf('@'); return i > 0 ? e.slice(0, i).split('+')[0] + e.slice(i) : e; };
const uniq = arr => [...new Set(arr)];
// כל הכתובות המוגנות: הראשית + זו שמחוברת ב-Chat + נוספות + כל מה שזוהה אוטומטית
const protList = () => uniq([S.me, S.actor, ...S.extra, ...S.cands.map(c => c.email)].map(norm).filter(e => VALID.test(e)));
const isProt = e => { const l = protList(); return l.some(p => canon(p) === canon(e) || canon(noPlus(p)) === canon(noPlus(e))); };
function purgeProtected() {
  const n = S.banned.length; S.banned = S.banned.filter(b => !isProt(b.email)); return n - S.banned.length;
}

function log(m) { const t = new Date().toLocaleTimeString('he-IL'); const p = $('#log'); p.textContent += `[${t}] ${m}\n`; p.scrollTop = 1e9; }
const save = () => chrome.storage.local.set({ S: { me: S.me, meAuto: S.meAuto, extra: S.extra, mode: S.mode, banned: S.banned, bannedMeta: S.bannedMeta } });
async function load() {
  const { S: s } = await chrome.storage.local.get('S');
  if (s) { S.me = s.me || ''; S.meAuto = s.meAuto !== false; S.extra = s.extra || []; S.mode = s.mode || 'auto'; S.banned = s.banned || []; S.bannedMeta = s.bannedMeta || null; }
  $('#extra').value = S.extra.join(', '); $('#mode').value = S.mode; $('#meAuto').checked = S.meAuto;
  renderMe(); render(); renderVerify();
  if (S.meAuto || !S.me) await autoDetect(true);
}
function setBusy(b) { S.busy = b; S.stop = false; ['#scan', '#remove', '#scrape', '#diag', '#detect'].forEach(s => $(s).disabled = b); $('#stop').disabled = !b; }
const progress = (i, n) => { $('#bar i').style.width = (n ? Math.round(i / n * 100) : 0) + '%'; };
$('#stop').onclick = () => {
  S.stop = true; log('⏹ עצירה התבקשה…');
  if (S.tabId) chrome.scripting.executeScript({ target: { tabId: S.tabId }, func: () => window.__CB && window.__CB.requestStop() }).catch(() => {});
};
$('#copyLog').onclick = () => { navigator.clipboard.writeText($('#log').textContent); };

// ---------- המייל שלי: זיהוי אוטומטי + בחירה ----------
// פונקציה שרצה בתוך טאב של Google ומחזירה את החשבון המחובר
function pageDetectMe() {
  const re = /[A-Za-z0-9._%+'\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}/;
  const el = [...document.querySelectorAll('[aria-label*="@"]')].find(e => /google account|חשבון google|חשבון גוגל/i.test(e.getAttribute('aria-label')));
  const m = el && el.getAttribute('aria-label').match(re);
  return m ? m[0].toLowerCase() : '';
}
async function detectCandidates() {
  const c = new Map(), add = (e, src) => { e = norm(e); if (VALID.test(e) && !c.has(e)) c.set(e, src); };
  try { const u = await chrome.identity.getProfileUserInfo({ accountStatus: 'ANY' }); if (u && u.email) add(u.email, 'פרופיל Chrome'); } catch {}
  let tabs = []; try { tabs = await chrome.tabs.query({}); } catch {}
  for (const t of tabs) {
    if (!/^https:\/\/(mail|chat|groups|myaccount|accounts|drive|docs|calendar)\.google\.com\//.test(t.url || '')) continue;
    if (/^https:\/\/mail\.google\.com\/mail\//.test(t.url) || / - Gmail$/.test(t.title || '')) { const m = (t.title || '').match(EMAIL_RE); if (m) add(m[0], 'טאב Gmail'); }
    if (/^https:\/\/(mail|chat|groups)\.google\.com\//.test(t.url)) {
      try { const [r] = await chrome.scripting.executeScript({ target: { tabId: t.id }, func: pageDetectMe }); if (r && r.result) add(r.result, /groups/.test(t.url) ? 'טאב Groups' : 'טאב Chat/Gmail'); } catch {}
    }
  }
  return [...c].map(([email, src]) => ({ email, src }));
}
async function autoDetect(silent) {
  const cands = await detectCandidates();
  S.cands = cands;
  if (cands.length && (S.meAuto || !S.me)) { S.me = cands[0].email; }
  purgeProtected(); save(); renderMe(); render();
  if (cands.length) { if (!silent) log(`זוהו ${cands.length} חשבונות: ${cands.map(c => c.email + ' (' + c.src + ')').join(', ')}`); }
  else if (!silent) log('לא זוהה אף חשבון אוטומטית. הקלד את המייל ידנית (או התחבר ל-Gmail/Chat בטאב ולחץ שוב).');
}
function renderMe() {
  const sel = $('#meSel'); sel.innerHTML = '';
  const add = (v, t) => { const o = document.createElement('option'); o.value = v; o.textContent = t; sel.append(o); };
  add('', S.cands.length ? '— בחירה מהחשבונות שזוהו —' : '— לא זוהו חשבונות —');
  S.cands.forEach(c => add(c.email, `${c.email}  (${c.src})`));
  sel.value = S.cands.some(c => c.email === S.me) ? S.me : '';
  $('#me').value = S.me; $('#meAuto').checked = S.meAuto;
  const p = protList();
  $('#meInfo').textContent = p.length ? `🔒 מוגנים (${p.length}): ${p.join(', ')}` : '⚠ עדיין אין מייל מוגן – זהה אוטומטית או הקלד.';
}
$('#detect').onclick = async () => { S.meAuto = true; $('#meAuto').checked = true; $('#detect').disabled = true; await autoDetect(false); $('#detect').disabled = false; };
$('#meSel').onchange = e => { if (!e.target.value) return; S.me = e.target.value; S.meAuto = false; purgeProtected(); save(); renderMe(); render(); };
$('#me').addEventListener('change', () => { S.me = norm($('#me').value); S.meAuto = false; purgeProtected(); save(); renderMe(); render(); });
$('#meAuto').onchange = async e => { S.meAuto = e.target.checked; save(); if (S.meAuto) await autoDetect(false); };
$('#extra').addEventListener('change', () => { S.extra = uniq(($('#extra').value.match(EMAIL_RE) || []).map(norm)); purgeProtected(); save(); renderMe(); render(); });
$('#mode').onchange = e => { S.mode = e.target.value; save(); };

// ---------- חלון עבודה של Chat ----------
async function ensureWorker() {
  if (S.tabId) { try { await chrome.tabs.get(S.tabId); return; } catch {} }
  const w = await chrome.windows.create({ url: 'about:blank', type: 'normal', width: 1300, height: 850, focused: false });
  S.tabId = w.tabs[0].id;
}
async function runOn(tabId, fn, args = []) {
  await chrome.scripting.executeScript({ target: { tabId }, files: ['chat.js'] });
  const [r] = await chrome.scripting.executeScript({ target: { tabId }, func: (n, a) => window.__CB[n](...a), args: [fn, args] });
  return r.result;
}
const run = (fn, args) => runOn(S.tabId, fn, args);
function navigate(url) {
  return new Promise(async res => {
    const t = setTimeout(() => { chrome.tabs.onUpdated.removeListener(l); res(); }, 40000);
    function l(id, info) { if (id === S.tabId && info.status === 'complete') { chrome.tabs.onUpdated.removeListener(l); clearTimeout(t); res(); } }
    chrome.tabs.onUpdated.addListener(l);
    await chrome.tabs.update(S.tabId, { url });
  }).then(() => sleep(5000));
}
async function openChat() {
  await ensureWorker(); log('פותח את Google Chat…');
  await navigate(CHAT);
  const u = (await chrome.tabs.get(S.tabId)).url || '';
  if (/accounts\.google\.com/.test(u)) throw new Error('לא מחובר ל-Google בחלון העבודה – התחבר שם ולחץ שוב');
  const who = await run('detectMe');
  if (who) {
    S.actor = who;
    if (!S.me) { S.me = who; }
    if (who !== S.me) log(`ℹ החשבון המחובר ב-Chat (${who}) שונה מהמייל הראשי (${S.me}) – שניהם מוגנים ולא יוסרו.`);
    $('#meInfo').textContent = '✓ חשבון Chat: ' + who;
  }
  purgeProtected(); save(); renderMe();
  if (!protList().length) throw new Error('לא זוהה המייל שלך – הקלד אותו בשדה למעלה');
}

// ---------- שלב 1: חסומים ----------
async function pickGroupsTab() {
  const tabs = await chrome.tabs.query({ url: 'https://groups.google.com/*' });
  if (!tabs.length) return null;
  tabs.sort((a, b) => (b.lastAccessed || 0) - (a.lastAccessed || 0));
  return tabs.find(t => /banned|חסומ/i.test(t.url)) || tabs[0];
}
$('#scrape').onclick = async () => {
  const t = await pickGroupsTab();
  if (!t) return alert('לא נמצא טאב פתוח של groups.google.com. פתח את עמוד החסומים ונסה שוב.');
  setBusy(true); $('#scrapeInfo').textContent = 'שואב… (גולל בצעדים ומעבר עמודים, אל תסגור את הטאב)'; log('שואב חסומים מ: ' + t.url);
  try {
    if (S.meAuto) await autoDetect(true);
    await chrome.scripting.executeScript({ target: { tabId: t.id }, files: ['content.js'] });
    const [r] = await chrome.scripting.executeScript({ target: { tabId: t.id }, func: () => window.__GB.scrapeBanned() });
    const d = r.result;
    if (d.me && VALID.test(d.me) && !S.cands.some(c => c.email === d.me)) { S.cands.push({ email: d.me, src: 'טאב Groups' }); if (!S.me) S.me = d.me; renderMe(); }
    const clean = arr => arr.filter(x => !isProt(x.email) && !(d.gname && x.email.startsWith(d.gname.toLowerCase() + '@')));
    const L = clean(d.leaf), R = clean(d.rows), U = clean(d.union || []);
    // בוחרים את הקבוצה שתואמת למספר שהאתר מציג; אם אין התאמה – האיחוד (הכי שלם), והמשתמש מאמת
    let pick, method;
    const cand = [[L, 'תאי מייל'], [R, 'שורות'], [U, 'איחוד']];
    const hit = cand.find(([a]) => a.length && d.declared.includes(a.length));
    if (hit) { [pick, method] = hit; }
    else { [pick, method] = cand.reduce((b, c) => (c[0].length > b[0].length ? c : b)); }
    const seen = new Set(); pick = pick.filter(x => { const k = canon(x.email); if (seen.has(k)) return false; seen.add(k); return true; });
    const match = d.declared.includes(pick.length);
    S.banned = pick.map(x => ({ email: x.email, raw: x.raw, selected: true }));
    S.bannedMeta = { count: pick.length, declared: d.declared, match, method, pages: d.pages, url: d.url, at: new Date().toLocaleString('he-IL') };
    log(`נשאבו ${pick.length} כתובות (${method}, ${d.pages} עמודים; תאים ${L.length} · שורות ${R.length} · איחוד ${U.length}). האתר מציג מספרים: [${d.declared.join(', ') || '—'}]. ${match ? '✓ תואם' : '⚠ אין התאמה למספר שהאתר מציג'}`);
  } catch (e) { log('⛔ שגיאה בשאיבה: ' + e.message); }
  $('#scrapeInfo').textContent = ''; setBusy(false); save(); render(); renderVerify();
};
function renderVerify() {
  const m = S.bannedMeta, el = $('#verify'); if (!m) { el.innerHTML = ''; return; }
  el.innerHTML = `<div class="vbox ${m.match ? 'good' : 'warn'}">נשאבו <b>${m.count}</b> כתובות ייחודיות · האתר מציג: ${m.declared.join(', ') || 'לא זוהה'} · ${m.match ? '✓ התאמה מלאה' : '⚠ אין התאמה – ייצא לאקסל והשווה, או הוסף/הסר ידנית'} · ${m.at}</div>`;
}
$('#addManual').onclick = () => {
  let n = 0, skipped = 0;
  ($('#manual').value.match(EMAIL_RE) || []).forEach(raw => {
    const e = norm(raw);
    if (isProt(e)) { skipped++; return; }
    if (!S.banned.find(b => canon(b.email) === canon(e))) { S.banned.push({ email: e, raw, selected: true }); n++; }
  });
  $('#manual').value = ''; log(`נוספו ${n} ידנית${skipped ? ` (${skipped} מוגנות דולגו)` : ''}`); S.bannedMeta = null; save(); render(); renderVerify();
};
$('#bAll').onclick = () => { S.banned.forEach(b => b.selected = true); save(); render(); };
$('#bNone').onclick = () => { S.banned.forEach(b => b.selected = false); save(); render(); };
$('#bClear').onclick = () => { if (confirm('לאפס את רשימת החסומים?')) { S.banned = []; S.bannedMeta = null; save(); render(); renderVerify(); } };

// ---------- שלב 2: סריקת מרחבים ----------
async function openSpace(g) {
  const r = await run('goSpace', [g.id, g.name]);
  if (!r.titleOk) log(`⚠ ייתכן שהמרחב "${g.name}" לא נטען במלואו`);
  const o = await run('openMembers');
  if (!o.ok) throw new Error(o.how);
}
const METHOD = { full: 'רשימה מלאה', search: 'הדבקה', 'full+search': 'רשימה+הדבקה' };
$('#scan').onclick = async () => {
  const n0 = purgeProtected(); if (n0) log(`🔒 ${n0} כתובות מוגנות הוסרו מרשימת החסומים`);
  const bannedList = S.banned.map(b => b.email);
  if (!bannedList.length) return alert('רשימת החסומים ריקה. שאב קודם חסומים.');
  setBusy(true); S.spaces = []; render(); progress(0, 1);
  try {
    await openChat();
    const bl = S.banned.filter(b => !isProt(b.email)).map(b => b.email);
    const list = await run('listSpaces');
    S.spaces = list.map(s => ({ id: s.id, name: s.name || s.id, displayName: s.name || s.id, role: '', manage: null, members: 0, seen: 0, method: '', found: [], status: 'ממתין', selected: false }));
    log(`זוהו ${S.spaces.length} מרחבים בסרגל: ${S.spaces.map(s => s.name).join(' | ')}`); render();
    if (!S.spaces.length) { log('⛔ לא זוהו מרחבים. לחץ "🔧 אבחון" ושלח לי את היומן.'); setBusy(false); return; }
    for (let i = 0; i < S.spaces.length && !S.stop; i++) {
      const g = S.spaces[i]; g.status = 'סורק…'; $('#scanInfo').textContent = `${i + 1}/${S.spaces.length}: ${g.name}`; progress(i, S.spaces.length); render();
      let poll;
      try {
        await openSpace(g);
        poll = setInterval(async () => {
          try {
            const [pr] = await chrome.scripting.executeScript({ target: { tabId: S.tabId }, func: () => window.__CB && window.__CB.getProgress() });
            const p = pr && pr.result; if (p && p.phase) $('#scanInfo').textContent = `${i + 1}/${S.spaces.length}: ${g.name} — ${p.phase} ${p.found}${p.total ? '/' + p.total : ''}`;
          } catch {}
        }, 1000);
        const r = await run('scanMembers', [{ banned: bl, prot: protList(), mode: S.mode }]);
        clearInterval(poll);
        g.members = r.total || r.seen; g.seen = r.seen; g.method = r.method;
        g.manage = r.meSeen ? !!r.meMgr : null;
        g.role = r.meSeen ? (r.meMgr ? 'מנהל' : 'חבר') : 'לא ידוע';
        g.found = r.found.map(f => ({ email: f.email, shown: f.shown, sel: true }));
        // ברירת מחדל: נבחר לכל מרחב שנמצאו בו חסומים (אם אינני מנהל – ההסרה תדווח כשלון ולא תזיק)
        g.selected = g.found.length > 0;
        g.status = g.found.length ? `נמצאו ${g.found.length}` : 'נקי ✓';
        if (g.found.length && g.manage === false) g.status += ' (נראה שאני רק חבר – ההסרה עלולה להיכשל)';
        if (!r.complete) g.status += ' ⚠ נקרא חלקית';
        log(`${g.name}: ${r.total || '?'} חברים (נקראו ${r.seen}) · שיטה: ${METHOD[r.method] || r.method} · תפקידי: ${g.role} · חסומים: ${g.found.length}${r.notes.length ? ' · ' + r.notes.join('; ') : ''}${r.complete ? '' : ' · ⚠ הרשימה לא נקראה במלואה'}`);
      } catch (e) { clearInterval(poll); g.status = 'שגיאה'; log(`⚠ ${g.name}: ${e.message}`); }
      save(); render();
    }
    const fg = S.spaces.filter(g => g.found.length), uniqF = new Set(fg.flatMap(g => g.found.map(f => canon(f.email))));
    const missing = bl.filter(e => !uniqF.has(canon(e)));
    log(`${S.stop ? 'נעצר. ' : ''}סיום סריקה: ${S.spaces.length} מרחבים, עם חסומים: ${fg.length}, חסומים ייחודיים שנמצאו: ${uniqF.size} מתוך ${bl.length}, חברויות להסרה: ${fg.reduce((a, g) => a + g.found.length, 0)}.`);
    if (missing.length) log(`ℹ ${missing.length} חסומים לא נמצאו באף מרחב (כנראה לא חברים באף מרחב שלך): ${missing.slice(0, 15).join(', ')}${missing.length > 15 ? '…' : ''}`);
  } catch (e) { log('⛔ ' + e.message); }
  $('#scanInfo').textContent = ''; progress(0, 1); setBusy(false); render();
};
$('#diag').onclick = async () => {
  setBusy(true);
  try {
    const tabs = (await chrome.tabs.query({ url: ['https://mail.google.com/chat/*', 'https://chat.google.com/*'] })).filter(t => t.id !== S.tabId);
    let tid;
    if (tabs.length) { tabs.sort((a, b) => (b.lastAccessed || 0) - (a.lastAccessed || 0)); tid = tabs[0].id; log('אבחון על הטאב: ' + tabs[0].url); }
    else { await ensureWorker(); await navigate(CHAT); tid = S.tabId; log('אבחון על חלון העבודה'); }
    log('===== אבחון =====\n' + await runOn(tid, 'diagnose') + '\n===== סוף =====');
  } catch (e) { log('⛔ ' + e.message); }
  setBusy(false);
};

// ---------- שלב 3: הסרה ----------
$('#remove').onclick = async () => {
  const bsel = new Set(S.banned.filter(b => b.selected).map(b => canon(b.email)));
  const byS = S.spaces.filter(g => g.selected).map(g => ({ g, t: g.found.filter(f => f.sel && !f.removed && bsel.has(canon(f.email)) && !isProt(f.email)) })).filter(x => x.t.length);
  const total = byS.reduce((a, x) => a + x.t.length, 0);
  if (!total) return alert('אין מה להסיר – בדוק את הבחירות.');
  if (!confirm(`להסיר ${total} חברויות מ-${byS.length} מרחבים?\nמוגנים (לא יוסרו): ${protList().join(', ')}\nהפעולה אינה הפיכה.`)) return;
  setBusy(true); S.results = []; let ok = 0, bad = 0; const reasons = {};
  const upd = () => { $('#removeInfo').innerHTML = `<span class="ok">✓ הצליחו: ${ok}</span> · <span class="bad">✗ נכשלו: ${bad}</span> · נותרו: ${total - ok - bad}`; progress(ok + bad, total); };
  upd();
  try {
    await openChat();
    const prot = protList();
    outer: for (const { g, t } of byS) {
      if (S.stop) break;
      g.status = 'מסיר…'; render(); log(`▶ ${g.name}: ${t.length} להסרה`);
      let opened = true;
      try { await openSpace(g); } catch (e) { opened = false; t.forEach(f => { bad++; reasons[e.message] = (reasons[e.message] || 0) + 1; S.results.push({ space: g.name, spaceId: g.id, email: f.email, ok: false, detail: e.message }); log(`✗ ${g.name} ← ${f.email} | ${e.message}`); }); upd(); }
      if (!opened) continue;
      for (const f of t) {
        if (S.stop) break outer;
        const res = { space: g.name, spaceId: g.id, email: f.email, ok: false, detail: '' };
        try { const r = await run('removeMember', [f.email, prot]); if (r.ok) { res.ok = true; res.detail = 'הוסר ואומת'; } else res.detail = r.error || 'שגיאה לא ידועה'; }
        catch (e) { res.detail = e.message; }
        if (res.ok) { ok++; f.removed = true; log(`✓ ${g.name} ← ${f.email}`); } else { bad++; reasons[res.detail] = (reasons[res.detail] || 0) + 1; log(`✗ ${g.name} ← ${f.email} | ${res.detail}`); }
        S.results.push(res); upd(); await sleep(400);
      }
      const left = g.found.filter(f => !f.removed).length; g.status = left ? `הוסרו ${g.found.length - left}, נותרו ${left}` : `✓ הוסרו ${g.found.length}`; render();
    }
    log(`══ סיכום הסרה ══ הצלחות: ${ok} | כשלונות: ${bad} | לא בוצעו: ${total - ok - bad}`);
    Object.entries(reasons).forEach(([k, v]) => log(`   • ${v} × ${k}`));
    if (bad) log('טיפ: לחץ "🔧 אבחון" כשהפאנל פתוח ושלח לי את היומן כדי שאכוון את התוסף.');
  } catch (e) { log('⛔ ' + e.message); }
  progress(0, 1); setBusy(false); render();
};

// ---------- תצוגה ----------
function render() {
  const sel = S.banned.filter(b => b.selected).length;
  $('#bannedCount').textContent = `${S.banned.length} חסומים (${sel} מסומנים)`;
  const box = $('#banned'); box.innerHTML = '';
  S.banned.forEach((b, i) => {
    const d = document.createElement('label'); d.className = 'chip';
    d.innerHTML = `<input type="checkbox" ${b.selected ? 'checked' : ''}><span></span>`; d.querySelector('span').textContent = `${i + 1}. ${b.raw || b.email}`;
    d.querySelector('input').onchange = e => { b.selected = e.target.checked; save(); render(); }; box.append(d);
  });
  const mg = S.spaces.filter(g => g.manage === true), fg = S.spaces.filter(g => g.found.length), uniqF = new Set(fg.flatMap(g => g.found.map(f => canon(f.email))));
  const notFound = S.spaces.length ? S.banned.filter(b => !uniqF.has(canon(b.email))).length : 0;
  $('#stats').innerHTML = [['מרחבים שזוהו', S.spaces.length], ['מרחבים שאני מנהל', mg.length], ['מרחבים עם חסומים', fg.length], ['חסומים ייחודיים שנמצאו', uniqF.size], ['חסומים שלא נמצאו באף מרחב', notFound], ['חברויות להסרה', fg.reduce((a, g) => a + g.found.filter(f => !f.removed).length, 0)]]
    .map(([k, v]) => `<div class="stat"><b>${v}</b>${k}</div>`).join('');
  $('#allCount').textContent = S.spaces.length;
  $('#allSpaces').innerHTML = '';
  [...S.spaces].sort((a, b) => ((b.manage === true) - (a.manage === true)) || a.displayName.localeCompare(b.displayName, 'he')).forEach(g => {
    const d = document.createElement('div'); d.textContent = g.displayName + (g.members ? ` (${g.members} חברים)` : '');
    const p = document.createElement('span'); p.className = 'pill' + (g.manage === true ? ' mgr' : ''); p.textContent = g.role || '…';
    d.append(p); $('#allSpaces').append(d);
  });
  const bsel = new Set(S.banned.filter(b => b.selected).map(b => canon(b.email))), tb = $('#groups'); tb.innerHTML = '';
  S.spaces.filter(g => g.found.length).forEach(g => {
    const tr = document.createElement('tr');
    tr.innerHTML = `<td><input type="checkbox" ${g.selected ? 'checked' : ''}></td><td></td><td>${g.role}</td><td class="emails"></td><td></td>`;
    tr.children[1].textContent = g.displayName + (g.members ? ` (${g.members})` : '');
    const em = tr.children[3];
    g.found.forEach(f => {
      const l = document.createElement('label'); l.style.display = 'block';
      l.innerHTML = `<input type="checkbox" ${f.sel && !f.removed ? 'checked' : ''} ${f.removed ? 'disabled' : ''}> <span></span>`;
      l.querySelector('span').textContent = f.email + (f.removed ? ' ✓' : '') + (bsel.has(canon(f.email)) ? '' : ' (לא נבחר בחסומים)');
      l.querySelector('input').onchange = e => { f.sel = e.target.checked; }; em.append(l);
    });
    tr.children[4].textContent = g.status; tr.children[4].className = /✓/.test(g.status) ? 'ok' : /נותרו|שגיאה|⚠/.test(g.status) ? 'bad' : '';
    tr.querySelector('input').onchange = e => { g.selected = e.target.checked; };
    tb.append(tr);
  });
}
$('#gAll').onclick = () => { S.spaces.forEach(g => g.selected = g.found.length > 0); render(); };
$('#gNone').onclick = () => { S.spaces.forEach(g => g.selected = false); render(); };

// ---------- ייצוא לאקסל ----------
function sheet(rows, cols) { const ws = XLSX.utils.aoa_to_sheet(rows); ws['!cols'] = cols.map(w => ({ wch: w })); ws['!views'] = [{ rightToLeft: true }]; return ws; }
function download(wb, name) { wb.Workbook = { Views: [{ RTL: true }] }; XLSX.writeFile(wb, `${name}-${new Date().toISOString().slice(0, 10)}.xlsx`); }
$('#xlsBanned').onclick = () => {
  if (!S.banned.length) return alert('הרשימה ריקה.');
  const wb = XLSX.utils.book_new();
  XLSX.utils.book_append_sheet(wb, sheet([['#', 'כתובת מייל (כפי שנשאבה)', 'כתובת מנורמלת', 'מסומן להסרה'], ...S.banned.map((b, i) => [i + 1, b.raw || b.email, b.email, b.selected ? 'כן' : 'לא'])], [6, 42, 42, 14]), 'חסומים');
  const m = S.bannedMeta || {};
  XLSX.utils.book_append_sheet(wb, sheet([['נשאבו', S.banned.length], ['מספרים שהאתר הציג', (m.declared || []).join(', ')], ['התאמה', m.match ? 'כן' : 'לא'], ['מקור', m.url || ''], ['תאריך', m.at || '']], [24, 60]), 'סיכום');
  download(wb, 'banned-list');
};
$('#xlsReport').onclick = () => {
  const wb = XLSX.utils.book_new();
  XLSX.utils.book_append_sheet(wb, sheet([['שם מרחב', 'מזהה', 'תפקידי', 'מנהל?', 'חברים (הצהרה)', 'נקראו', 'שיטה', 'חסומים שנמצאו', 'כתובות', 'סטטוס'], ...S.spaces.map(g => [g.displayName, g.id, g.role, g.manage === true ? 'כן' : g.manage === false ? 'לא' : 'לא ידוע', g.members, g.seen, METHOD[g.method] || g.method, g.found.length, g.found.map(f => f.email).join('; '), g.status])], [34, 22, 12, 10, 12, 8, 14, 12, 60, 28]), 'מרחבים');
  XLSX.utils.book_append_sheet(wb, sheet([['מרחב', 'מזהה', 'כתובת', 'תוצאה', 'פירוט / באג'], ...S.results.map(r => [r.space, r.spaceId, r.email, r.ok ? 'הצלחה' : 'כשלון', r.detail])], [34, 22, 40, 10, 70]), 'דוח הסרה');
  const uf = new Set(S.spaces.flatMap(g => g.found.map(f => canon(f.email))));
  XLSX.utils.book_append_sheet(wb, sheet([['חסומים שלא נמצאו באף מרחב'], ...S.banned.filter(b => !uf.has(canon(b.email))).map(b => [b.email])], [44]), 'לא נמצאו');
  download(wb, 'chat-cleanup-report');
};
load();
