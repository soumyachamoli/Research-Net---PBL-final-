import { writeFileSync, mkdirSync } from 'node:fs';

const BASE = process.argv[2] || 'http://127.0.0.1:8080';
const SHOTS = process.argv[3] || 'screenshots';
mkdirSync(SHOTS, { recursive: true });

const targets = await (await fetch('http://127.0.0.1:9222/json/list')).json();
const page = targets.find(t => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise(r => ws.addEventListener('open', r, { once: true }));

let nextId = 1;
const pending = new Map();
const problems = [];
ws.addEventListener('message', ev => {
  const msg = JSON.parse(ev.data);
  if (msg.id && pending.has(msg.id)) { pending.get(msg.id)(msg); pending.delete(msg.id); return; }
  if (msg.method === 'Runtime.exceptionThrown') problems.push('exception: ' + msg.params.exceptionDetails.exception?.description);
  if (msg.method === 'Runtime.consoleAPICalled' && msg.params.type === 'error') problems.push('console.error: ' + msg.params.args.map(a => a.value).join(' '));
  if (msg.method === 'Log.entryAdded' && ['error'].includes(msg.params.entry.level) && !/401|404|favicon/.test(msg.params.entry.text + msg.params.entry.url)) problems.push('log: ' + msg.params.entry.text);
});
function send(method, params = {}) {
  const id = nextId++;
  ws.send(JSON.stringify({ id, method, params }));
  return new Promise((resolve, reject) => pending.set(id, m => m.error ? reject(new Error(method + ': ' + m.error.message)) : resolve(m.result)));
}
async function evaluate(expr) {
  const r = await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error('eval failed: ' + expr.slice(0, 80) + ' -> ' + r.exceptionDetails.exception?.description);
  return r.result.value;
}
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function waitFor(expr, what, timeout = 8000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    if (await evaluate(`(() => { try { return !!(${expr}); } catch (e) { return false; } })()`)) return;
    await sleep(100);
  }
  throw new Error('timeout waiting for ' + what);
}
async function shot(name) {
  const r = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false });
  writeFileSync(`${SHOTS}/${name}.png`, Buffer.from(r.data, 'base64'));
}
async function viewport(width, height, mobile = false) {
  await send('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: 1, mobile });
}
async function go(hash) {
  await evaluate(`location.hash = ${JSON.stringify(hash)}`);
  await sleep(150);
  await waitFor(`!document.querySelector('main .loading')`, 'page load ' + hash);
}
const results = [];
function check(name, ok, detail = '') {
  results.push(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? '  (' + detail + ')' : ''}`);
}

await send('Runtime.enable');
await send('Page.enable');
await send('Log.enable');
await send('Network.enable');
await send('Network.clearBrowserCookies');
await viewport(1280, 900);
await send('Page.navigate', { url: 'about:blank' });
await sleep(300);
await send('Page.navigate', { url: BASE + '/#/' });
await waitFor(`document.querySelectorAll('.category-card').length > 0 && document.querySelectorAll('.paper-card').length > 0`, 'landing content');
check('landing: categories + featured papers', true,
  await evaluate(`document.querySelectorAll('.category-card').length + ' categories, ' + document.querySelectorAll('.paper-card').length + ' featured'`));
check('landing: stats shown', await evaluate(`document.querySelector('.stats').textContent.includes('research papers')`));
await shot('01-landing-desktop');

await evaluate(`(() => { const i = document.querySelector('.hero input'); i.focus(); i.value = 'neural'; i.dispatchEvent(new Event('input')); })()`);
await waitFor(`document.querySelector('.hero .suggestions:not([hidden]) li')`, 'autocomplete');
const sugg = await evaluate(`[...document.querySelectorAll('.hero .suggestions li')].map(li => li.textContent)`);
check('autocomplete suggestions', sugg.length > 0, sugg.slice(0, 3).join(' | '));
await shot('02-autocomplete');
await evaluate(`document.querySelector('.hero input').dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}))`);
check('autocomplete keyboard highlight', await evaluate(`document.querySelector('.hero .suggestions li[aria-selected="true"]') !== null`));

await evaluate(`location.hash = '#/saved'`);
await waitFor(`location.hash.startsWith('#/login')`, 'redirect to login');
check('protected page redirects to login', true, await evaluate('location.hash'));

await evaluate(`document.querySelector('.auth-switch a').click()`);
await waitFor(`location.hash.startsWith('#/register?next=')`, 'register link keeps next');
await sleep(200);
await evaluate(`(() => { const f = document.querySelector('main form'); f.name.value = 'UI Tester'; f.email.value = 'bad'; f.password.value = 'pw'; f.confirmPassword.value = 'pw'; f.requestSubmit(); })()`);
check('register: invalid email message', await evaluate(`document.querySelector('.form-error:not([hidden])')?.textContent.includes('valid email')`));
const email = `ui.tester.${Date.now()}@example.com`;
await evaluate(`(() => { const f = document.querySelector('main form'); f.email.value = '${email}'; f.password.value = 'uitester123'; f.confirmPassword.value = 'uitester123'; f.requestSubmit(); })()`);
await waitFor(`location.hash === '#/saved'`, 'redirect back to saved after register');
check('register then return to requested page', true);
await go('#/dashboard');
await waitFor(`document.querySelector('.welcome h1')`, 'dashboard');
check('dashboard greets user', await evaluate(`document.querySelector('.welcome h1').textContent === 'Welcome back, UI'`));
await shot('03-dashboard-new-user');

await go('#/search?q=deep%20learning&year_from=2016&year_to=2020&min_impact=5&sort=impact_desc');
await waitFor(`document.querySelector('.results-head .count').textContent.includes('found')`, 'results');
const count = await evaluate(`document.querySelector('.results-head .count').textContent`);
const years = await evaluate(`[...document.querySelectorAll('.paper-card .paper-meta')].map(m => m.textContent)`);
const impacts = await evaluate(`[...document.querySelectorAll('.badge.impact')].map(b => parseFloat(b.textContent.replace('Impact ', '')))`);
check('search with filters returns results', /\d/.test(count), count);
check('impact sorted high to low and >= 5', impacts.length > 0 && impacts.every((v, i) => v >= 5 && (i === 0 || impacts[i - 1] >= v)), impacts.slice(0, 5).join(','));
check('years within 2016-2020', years.every(m => /201[6-9]|2020/.test(m)));
check('active filter chips', (await evaluate(`document.querySelectorAll('.active-filters .chip').length`)) === 3);
await shot('04-search-filters');

await evaluate(`document.querySelector('.paper-card .save-btn').click()`);
await waitFor(`document.querySelector('.paper-card .save-btn.saved')`, 'saved state');
check('bookmark toggles on', true);
const savedTitle = await evaluate(`document.querySelector('.paper-card .paper-title').textContent`);

const hasNext = await evaluate(`!!document.querySelector('.pager a[href*="page=2"]')`);
if (hasNext) {
  await evaluate(`document.querySelector('.pager a[href*="page=2"]').click()`);
  await sleep(200);
  await waitFor(`document.querySelector('.pager span')?.textContent.startsWith('Page 2')`, 'page 2');
  check('pagination to page 2', true);
}

await evaluate(`document.querySelector('.paper-card .paper-title a').click()`);
await waitFor(`document.querySelector('.paper-page h1') && document.querySelector('.section .stack .paper-card')`, 'paper page');
check('paper page: abstract + details', await evaluate(`document.querySelector('.abstract').textContent.length > 100 && document.querySelector('.facts').textContent.includes('Journal')`));
const reasons = await evaluate(`[...document.querySelectorAll('.reasons li')].slice(0, 3).map(l => l.textContent)`);
check('similar papers with reasons', reasons.length > 0, reasons.join(' | '));
await evaluate(`document.querySelector('.paper-page details, .section details')?.setAttribute('open', '')`);
await shot('05-paper-page');

await go('#/saved');
await waitFor(`document.querySelector('.paper-card') || document.querySelector('.empty')`, 'saved list');
check('saved page lists bookmarked paper', await evaluate(`[...document.querySelectorAll('.paper-title')].some(t => t.textContent === ${JSON.stringify(savedTitle)})`));

await go('#/profile');
await waitFor(`document.querySelector('main h1')?.textContent === 'Your profile'`, 'profile');
await evaluate(`[...document.querySelectorAll('button.chip')].find(b => b.textContent.includes('Artificial Intelligence')).click()`);
await evaluate(`[...document.querySelectorAll('main form')].find(f => f.textContent.includes('Save interests')).requestSubmit()`);
await waitFor(`document.querySelector('.form-ok')`, 'interests saved');
check('profile: interests saved', true);
await shot('06-profile');

await go('#/recommendations');
await waitFor(`document.querySelectorAll('.paper-card').length > 0`, 'recommendations');
check('recommendations shown with basis', await evaluate(`document.querySelector('.page-header p').textContent.startsWith('Based on')`),
  await evaluate(`document.querySelector('.page-header p').textContent`));
await shot('07-recommendations');

await go('#/history');
await waitFor(`document.querySelector('.list-row') || document.querySelector('.empty')`, 'history');
check('history lists the search', await evaluate(`[...document.querySelectorAll('.list-row .title')].some(t => t.textContent === 'deep learning')`));
await go('#/history?tab=viewed');
await waitFor(`document.querySelector('.paper-card') || document.querySelector('.empty')`, 'viewed');
check('history lists the viewed paper', (await evaluate(`document.querySelectorAll('.paper-card').length`)) >= 1);

await go('#/dashboard');
await waitFor(`document.querySelectorAll('.list-row').length > 0`, 'dashboard data');
await shot('08-dashboard');

await viewport(390, 844, true);
await go('#/');
await sleep(300);
await shot('09-landing-mobile');
check('mobile: no horizontal overflow (landing)', await evaluate(`document.documentElement.scrollWidth <= window.innerWidth + 1`),
  await evaluate(`document.documentElement.scrollWidth + ' vs ' + window.innerWidth`));
await go('#/search?q=wireless%20sensor%20network');
await waitFor(`document.querySelector('.results-head .count').textContent.includes('found')`, 'mobile search');
check('mobile: filters collapsed behind a button', await evaluate(`getComputedStyle(document.querySelector('.filters')).display === 'none' && getComputedStyle(document.querySelector('.filters-toggle')).display !== 'none'`));
check('mobile: no horizontal overflow (search)', await evaluate(`document.documentElement.scrollWidth <= window.innerWidth + 1`));
await shot('10-search-mobile');
await evaluate(`document.getElementById('menu-button').click()`);
check('mobile: menu opens', await evaluate(`document.getElementById('main-nav').classList.contains('open')`));
await shot('11-menu-mobile');
await go('#/paper/1');
await waitFor(`document.querySelector('.paper-page h1')`, 'mobile paper');
check('mobile: no horizontal overflow (paper)', await evaluate(`document.documentElement.scrollWidth <= window.innerWidth + 1`));

await viewport(1280, 900);
await go('#/profile');
await waitFor(`document.querySelector('main h1')?.textContent === 'Your profile'`, 'profile again');
await evaluate(`[...document.querySelectorAll('.page-header button')].find(b => b.textContent.includes('Log out')).click()`);
await waitFor(`location.hash === '#/' && document.querySelector('.main-nav a[href="#/login"]')`, 'logged out');
check('logout returns to landing as guest', true);

check('no console errors / exceptions / CSP violations', problems.length === 0, problems.slice(0, 5).join(' || '));
console.log(results.join('\n'));
console.log(`${results.filter(r => r.startsWith('PASS')).length}/${results.length} UI checks passed`);
ws.close();
process.exit(results.some(r => r.startsWith('FAIL')) ? 1 : 0);
