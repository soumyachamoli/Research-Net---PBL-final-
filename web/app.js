'use strict';

(() => {
  const main = document.getElementById('main');
  const nav = document.getElementById('main-nav');
  const topSearch = document.getElementById('topbar-search');
  const state = { user: null, stats: null, filterOptions: null };
  let renderToken = 0;


  function h(tag, attrs, ...children) {
    const el = document.createElement(tag);
    for (const [key, value] of Object.entries(attrs || {})) {
      if (value == null || value === false) continue;
      if (key === 'class') el.className = value;
      else if (key === 'text') el.textContent = value;
      else if (key.startsWith('on')) el.addEventListener(key.slice(2), value);
      else if (['value', 'checked', 'hidden', 'disabled', 'selected'].includes(key)) el[key] = value;
      else el.setAttribute(key, value === true ? '' : value);
    }
    append(el, children);
    return el;
  }

  function append(el, children) {
    for (const child of [children].flat(Infinity)) {
      if (child == null || child === false) continue;
      el.append(child instanceof Node ? child : String(child));
    }
    return el;
  }

  function clear(el) {
    while (el.firstChild) el.removeChild(el.firstChild);
    return el;
  }

  const ICONS = {
    search: ['M21 21l-4.3-4.3', 'M11 18a7 7 0 1 1 0-14 7 7 0 0 1 0 14z'],
    bookmark: ['M6 3h12v18l-6-4.2L6 21z'],
    user: ['M12 12a4 4 0 1 0 0-8 4 4 0 0 0 0 8z', 'M4 21c0-4 3.6-6 8-6s8 2 8 6'],
    clock: ['M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18z', 'M12 7v5l3 2'],
    spark: ['M12 3l2.4 5.4 5.6.7-4.2 3.9 1.1 5.6L12 15.8 7.1 18.6l1.1-5.6L4 9.1l5.6-.7z'],
    filter: ['M3 5h18l-7 8v6l-4-2v-4z'],
    list: ['M4 6h16', 'M4 12h11', 'M4 18h7'],
    layers: ['M12 3l9 5-9 5-9-5z', 'M3 13l9 5 9-5'],
    external: ['M14 4h6v6', 'M20 4l-9 9', 'M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5'],
    trash: ['M4 7h16', 'M9 7V4h6v3', 'M6 7l1 13h10l1-13'],
    logout: ['M15 4h4a1 1 0 0 1 1 1v14a1 1 0 0 1-1 1h-4', 'M10 17l5-5-5-5', 'M15 12H3'],
    grid: ['M4 4h7v7H4z', 'M13 4h7v7h-7z', 'M4 13h7v7H4z', 'M13 13h7v7h-7z'],
  };

  function icon(name, cls) {
    const ns = 'http://www.w3.org/2000/svg';
    const svg = document.createElementNS(ns, 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.setAttribute('fill', 'none');
    svg.setAttribute('stroke', 'currentColor');
    svg.setAttribute('stroke-width', '2');
    svg.setAttribute('stroke-linecap', 'round');
    svg.setAttribute('stroke-linejoin', 'round');
    svg.setAttribute('aria-hidden', 'true');
    if (cls) svg.setAttribute('class', cls);
    for (const d of ICONS[name]) {
      const path = document.createElementNS(ns, 'path');
      path.setAttribute('d', d);
      svg.append(path);
    }
    return svg;
  }

  function safeUrl(url) {
    return typeof url === 'string' && /^https?:\/\//i.test(url) ? url : null;
  }

  function fmtNumber(n) {
    return Number(n).toLocaleString('en-US');
  }

  function fmtDate(text) {
    if (!text) return '';
    const iso = text.replace(' ', 'T').replace(/(\.\d{3})\d+/, '$1');
    const date = new Date(iso);
    if (isNaN(date)) return text.slice(0, 16);
    return date.toLocaleString('en-GB', { day: 'numeric', month: 'short', year: 'numeric', hour: '2-digit', minute: '2-digit' });
  }

  function toast(message) {
    const el = document.getElementById('toast');
    el.textContent = message;
    el.hidden = false;
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => { el.hidden = true; }, 2600);
  }


  async function api(path, options = {}) {
    const init = { method: options.method || 'GET', credentials: 'same-origin', headers: {} };
    if (options.body !== undefined) {
      init.headers['Content-Type'] = 'application/json';
      init.body = JSON.stringify(options.body);
    }
    let response;
    try {
      response = await fetch(path, init);
    } catch (err) {
      return { ok: false, status: 0, data: { error: 'Cannot reach the server. Check your connection.' } };
    }
    let data = null;
    try { data = await response.json(); } catch (err) { data = {}; }
    if (response.status === 401 && state.user && path !== '/api/login') {
      state.user = null;
      renderNav();
    }
    return { ok: response.ok, status: response.status, data };
  }

  function errorText(result) {
    return (result.data && result.data.error) || 'Something went wrong. Please try again.';
  }


  function parseHash() {
    const raw = location.hash.replace(/^#/, '') || '/';
    const [path, queryString] = raw.split('?');
    return { path: path || '/', query: new URLSearchParams(queryString || '') };
  }

  function link(path, params) {
    const query = new URLSearchParams();
    for (const [key, value] of Object.entries(params || {})) {
      if (value !== undefined && value !== null && value !== '') query.set(key, value);
    }
    const qs = query.toString();
    return '#' + path + (qs ? '?' + qs : '');
  }

  function go(path, params) {
    location.hash = link(path, params);
  }

  const routes = [
    { pattern: /^\/$/, page: landingPage },
    { pattern: /^\/login$/, page: (m, q) => authPage('login', q) },
    { pattern: /^\/register$/, page: (m, q) => authPage('register', q) },
    { pattern: /^\/dashboard$/, page: dashboardPage, auth: true },
    { pattern: /^\/search$/, page: searchPage },
    { pattern: /^\/paper\/(\d+)$/, page: paperPage },
    { pattern: /^\/recommendations$/, page: recommendationsPage, auth: true },
    { pattern: /^\/saved$/, page: savedPage, auth: true },
    { pattern: /^\/history$/, page: historyPage, auth: true },
    { pattern: /^\/profile$/, page: profilePage, auth: true },
  ];

  function render() {
    const { path, query } = parseHash();
    const token = ++renderToken;
    const route = routes.find(r => r.pattern.test(path));
    nav.classList.remove('open');
    document.getElementById('menu-button').setAttribute('aria-expanded', 'false');
    renderNav(path);
    clear(topSearch);
    if (path !== '/') topSearch.append(searchBox({ value: path === '/search' ? query.get('q') || '' : '' }));
    clear(main);
    window.scrollTo(0, 0);

    if (!route) {
      main.append(emptyState('Page not found', 'This page does not exist.', h('a', { class: 'btn btn-primary', href: '#/' }, 'Go home')));
      return;
    }
    if (route.auth && !state.user) {
      go('/login', { next: location.hash.slice(1) });
      return;
    }
    const match = path.match(route.pattern);
    route.page(match, query, () => token === renderToken);
  }


  function renderNav(path = parseHash().path) {
    clear(nav);
    const item = (href, label) => h('a', { class: 'nav-link' + (path === href.slice(1) ? ' active' : ''), href }, label);
    if (state.user) {
      const initials = state.user.name.split(/\s+/).filter(Boolean).slice(0, 2).map(w => w[0].toUpperCase()).join('');
      append(nav, [
        item('#/dashboard', 'Dashboard'),
        item('#/search', 'Search'),
        item('#/recommendations', 'For you'),
        item('#/saved', 'Saved'),
        item('#/history', 'History'),
        h('a', { class: 'nav-user nav-link' + (path === '/profile' ? ' active' : ''), href: '#/profile', title: 'Profile' },
          h('span', { class: 'avatar', 'aria-hidden': 'true' }, initials), h('span', null, 'Profile')),
      ]);
    } else {
      append(nav, [
        item('#/search', 'Search'),
        item('#/login', 'Log in'),
        h('a', { class: 'btn btn-primary btn-sm', href: '#/register' }, 'Sign up'),
      ]);
    }
  }

  document.getElementById('menu-button').addEventListener('click', e => {
    const open = nav.classList.toggle('open');
    e.currentTarget.setAttribute('aria-expanded', String(open));
  });


  let searchBoxCount = 0;

  function searchBox({ large = false, value = '', placeholder = 'Search titles, keywords, abstracts, authors, journals…' } = {}) {
    const listId = 'suggestions-' + (++searchBoxCount);
    const input = h('input', {
      type: 'search', name: 'q', value, placeholder, autocomplete: 'off', spellcheck: 'false',
      role: 'combobox', 'aria-label': 'Search papers', 'aria-autocomplete': 'list',
      'aria-expanded': 'false', 'aria-controls': listId,
    });
    const list = h('ul', { class: 'suggestions', role: 'listbox', id: listId, hidden: true });
    let items = [];
    let active = -1;
    let timer = null;
    let requestId = 0;

    function close() {
      list.hidden = true;
      input.setAttribute('aria-expanded', 'false');
      active = -1;
    }

    function choose(s) {
      close();
      if (s.kind === 'paper' && s.paperId) go('/paper/' + s.paperId);
      else if (s.kind === 'author') go('/search', { author: s.text });
      else if (s.kind === 'category') go('/search', { category: s.text });
      else go('/search', { q: s.text });
    }

    function highlight(index) {
      active = index;
      [...list.children].forEach((li, i) => li.setAttribute('aria-selected', String(i === index)));
      if (index >= 0) input.setAttribute('aria-activedescendant', list.children[index].id);
      else input.removeAttribute('aria-activedescendant');
    }

    function show(suggestions) {
      items = suggestions;
      clear(list);
      if (!items.length) { close(); return; }
      const typed = input.value.trim().toLowerCase();
      items.forEach((s, i) => {
        const starts = s.text.toLowerCase().startsWith(typed);
        const text = h('span', { class: 's-text' },
          starts ? [h('mark', null, s.text.slice(0, typed.length)), s.text.slice(typed.length)] : s.text);
        list.append(h('li', {
          id: listId + '-' + i, role: 'option', 'aria-selected': 'false',
          onmousedown: e => { e.preventDefault(); choose(s); },
        }, text, h('span', { class: 'kind' }, s.kind)));
      });
      list.hidden = false;
      input.setAttribute('aria-expanded', 'true');
      active = -1;
    }

    input.addEventListener('input', () => {
      clearTimeout(timer);
      const prefix = input.value;
      if (!prefix.trim()) { close(); return; }
      timer = setTimeout(async () => {
        const id = ++requestId;
        const result = await api('/api/autocomplete?' + new URLSearchParams({ prefix, limit: 8 }));
        if (id === requestId && result.ok) show(result.data);
      }, 120);
    });

    input.addEventListener('keydown', e => {
      if (list.hidden) return;
      if (e.key === 'ArrowDown') { e.preventDefault(); highlight((active + 1) % items.length); }
      else if (e.key === 'ArrowUp') { e.preventDefault(); highlight(active <= 0 ? items.length - 1 : active - 1); }
      else if (e.key === 'Escape') close();
      else if (e.key === 'Enter' && active >= 0) { e.preventDefault(); choose(items[active]); }
    });
    input.addEventListener('blur', () => setTimeout(close, 120));

    const form = h('form', {
      role: 'search',
      onsubmit: e => {
        e.preventDefault();
        close();
        const current = parseHash();
        const params = current.path === '/search' ? Object.fromEntries(current.query) : {};
        params.q = input.value.trim();
        delete params.page;
        go('/search', params);
      },
    }, icon('search', 'search-icon'), input, h('button', { class: 'btn btn-primary', type: 'submit' }, 'Search'));

    return h('div', { class: 'searchbox' + (large ? ' large' : '') }, form, list);
  }


  function impactBadge(paper) {
    if (paper.impactFactor == null) {
      return h('span', { class: 'badge', title: 'No journal impact value is available for this venue' }, 'Impact n/a');
    }
    return h('span', {
      class: 'badge impact',
      title: `Journal 2-year mean citedness from ${paper.impactFactorSource || 'OpenAlex'}` +
        (paper.impactFactorYear ? ` (${paper.impactFactorYear})` : '') + '. An impact-factor-like metric, not the Clarivate JIF.',
    }, 'Impact ' + Number(paper.impactFactor).toFixed(2));
  }

  function authorLine(paper) {
    const names = paper.authors || [];
    const more = (paper.authorCount || names.length) - names.length;
    return names.join(', ') + (more > 0 ? ` +${more} more` : '');
  }

  function saveButton(paper) {
    const button = h('button', { class: 'save-btn', type: 'button' }, icon('bookmark'));
    const update = () => {
      button.classList.toggle('saved', !!paper.bookmarked);
      const label = paper.bookmarked ? 'Remove from saved papers' : 'Save paper';
      button.setAttribute('aria-label', label);
      button.title = label;
      button.setAttribute('aria-pressed', String(!!paper.bookmarked));
    };
    update();
    button.addEventListener('click', async () => {
      if (!state.user) { go('/login', { next: location.hash.slice(1) }); return; }
      button.disabled = true;
      const result = paper.bookmarked
        ? await api('/api/bookmarks/' + paper.id, { method: 'DELETE' })
        : await api('/api/bookmarks', { method: 'POST', body: { paperId: paper.id } });
      button.disabled = false;
      if (!result.ok && result.status !== 404) { toast(errorText(result)); return; }
      paper.bookmarked = !paper.bookmarked;
      update();
      toast(paper.bookmarked ? 'Saved to your papers' : 'Removed from saved papers');
      button.dispatchEvent(new CustomEvent('bookmark-change', { bubbles: true, detail: paper }));
    });
    return button;
  }

  function paperCard(paper, { extra = null, actions = null } = {}) {
    const doi = paper.doi ? safeUrl('https://doi.org/' + paper.doi) : null;
    const abstract = paper.abstractPreview || '';
    return h('article', { class: 'paper-card' },
      h('div', { class: 'paper-main' },
        h('h3', { class: 'paper-title' }, h('a', { href: '#/paper/' + paper.id }, paper.title)),
        authorLine(paper) && h('div', { class: 'paper-authors' }, authorLine(paper)),
        h('div', { class: 'paper-meta' },
          paper.journal && h('span', { class: 'journal' }, paper.journal),
          paper.year && h('span', null, paper.year),
          paper.type && h('span', null, paper.type),
          h('span', null, 'Cited by ' + fmtNumber(paper.citedBy || 0))),
        abstract && h('p', { class: 'paper-abstract' }, abstract + (abstract.length >= 320 ? '…' : '')),
        h('div', { class: 'paper-footer' },
          paper.relevance != null && h('span', { class: 'badge rel', title: 'TF-IDF cosine similarity to your query' },
            'Relevance ' + Math.round(paper.relevance * 100) + '%'),
          impactBadge(paper),
          paper.category && h('a', { class: 'badge cat', href: link('/search', { category: paper.category }) }, paper.category),
          doi && h('a', { class: 'btn btn-ghost btn-sm', href: doi, target: '_blank', rel: 'noopener' }, 'DOI ', icon('external')),
          actions),
        extra),
      saveButton(paper));
  }

  const COMPONENT_LABELS = [['content', 'Content'], ['keywords', 'Keywords'], ['authors', 'Authors'], ['recency', 'Recency'], ['impact', 'Impact']];

  function scoreBars(rec) {
    const grid = h('div', { class: 'score-bars', 'aria-label': 'Score breakdown' });
    for (const [key, label] of COMPONENT_LABELS) {
      const value = rec.components[key];
      const fill = h('i');
      fill.style.width = Math.round((value || 0) * 100) + '%';
      append(grid, [h('span', null, label), h('div', { class: 'bar' }, fill),
        h('span', null, value == null ? 'n/a' : Math.round(value * 100) + '%')]);
    }
    return grid;
  }

  function recommendationCard(rec) {
    return paperCard(rec.paper, {
      extra: [
        h('ul', { class: 'reasons' }, rec.reasons.map(r => h('li', null, r))),
        h('details', null, h('summary', { class: 'hint' }, `Match score ${Math.round(rec.score * 100)}% · why?`), scoreBars(rec)),
      ],
    });
  }

  function emptyState(title, text, action) {
    return h('div', { class: 'empty' }, h('h3', null, title), text && h('p', null, text), action);
  }

  function loading(text = 'Loading…') {
    return h('div', { class: 'loading', role: 'status' }, text);
  }


  async function landingPage(match, query, isCurrent) {
    const statsLine = h('div', { class: 'stats' });
    const categories = h('div', { class: 'grid-3' }, loading());
    const featured = h('div', { class: 'grid-2 compact' }, loading());

    append(main, [
      h('section', { class: 'hero' },
        h('h1', null, 'Find the research that matters'),
        h('p', { class: 'lead' }, 'Search thousands of genuine research papers, ranked by relevance. Filter by author, year and journal impact, and get recommendations that explain themselves.'),
        searchBox({ large: true }),
        h('div', { class: 'hero-actions' }, state.user
          ? h('a', { class: 'btn btn-primary', href: '#/dashboard' }, 'Go to your dashboard')
          : [h('a', { class: 'btn btn-primary', href: '#/register' }, 'Create a free account'),
             h('a', { class: 'btn btn-secondary', href: '#/login' }, 'Log in')]),
        statsLine),
      h('section', { class: 'section' },
        h('div', { class: 'section-title' }, h('h2', null, 'How ResearchNet helps')),
        h('div', { class: 'grid-2' },
          feature('list', 'Instant autocomplete', 'Suggestions for titles, keywords, topics and authors appear as you type, served by a Trie (prefix tree).'),
          feature('layers', 'Ranked keyword search', 'An inverted index with TF-IDF weights ranks papers by how well they match, across titles, abstracts, keywords, authors and journals.'),
          feature('filter', 'Advanced filters', 'Combine author, year range, journal, research area, keyword, publication type and journal impact range, then sort the way you need.'),
          feature('spark', 'Explainable recommendations', 'Similar papers and personal picks from TF-IDF cosine similarity, keywords and authors, each with the reasons behind it.'))),
      h('section', { class: 'section' },
        h('div', { class: 'section-title' }, h('h2', null, 'Popular research categories'), h('a', { href: '#/search' }, 'Browse all papers →')),
        categories),
      h('section', { class: 'section' },
        h('div', { class: 'section-title' }, h('h2', null, 'Featured recent papers'), h('span', { class: 'muted' }, 'Most cited of the last five years')),
        featured),
    ]);

    const [stats, cats, feat] = await Promise.all([api('/api/stats'), api('/api/categories'), api('/api/featured?limit=6')]);
    if (!isCurrent()) return;
    if (stats.ok) {
      append(statsLine, [
        h('div', { class: 'stat' }, h('strong', null, fmtNumber(stats.data.papers)), h('span', null, 'research papers')),
        h('div', { class: 'stat' }, h('strong', null, fmtNumber(stats.data.indexedTerms)), h('span', null, 'indexed terms')),
        h('div', { class: 'stat' }, h('strong', null, `${stats.data.yearFrom}–${stats.data.yearTo}`), h('span', null, 'publication years')),
      ]);
    }
    clear(categories);
    if (cats.ok) {
      for (const c of cats.data) {
        categories.append(h('a', { class: 'category-card', href: link('/search', { category: c.name }) },
          c.name, h('span', null, fmtNumber(c.papers) + ' papers')));
      }
    }
    clear(featured);
    if (feat.ok) feat.data.forEach(p => featured.append(paperCard(p)));
  }

  function feature(iconName, title, text) {
    return h('div', { class: 'feature' }, h('div', { class: 'icon' }, icon(iconName)), h('h3', null, title), h('p', null, text));
  }


  function authPage(mode, query) {
    if (state.user) { go('/dashboard'); return; }
    const isRegister = mode === 'register';
    const error = h('div', { class: 'form-error', role: 'alert', hidden: true });
    const field = (label, attrs) => {
      const id = 'f-' + attrs.name;
      return h('div', { class: 'field' }, h('label', { for: id }, label), h('input', Object.assign({ id, class: 'input', required: true }, attrs)));
    };
    const submit = h('button', { class: 'btn btn-primary', type: 'submit' }, isRegister ? 'Create account' : 'Log in');

    const form = h('form', { novalidate: true },
      error,
      isRegister && field('Full name', { name: 'name', autocomplete: 'name', maxlength: 100 }),
      field('Email address', { name: 'email', type: 'email', autocomplete: 'email' }),
      field('Password', { name: 'password', type: 'password', autocomplete: isRegister ? 'new-password' : 'current-password', minlength: isRegister ? 8 : null, maxlength: 72 }),
      isRegister && field('Confirm password', { name: 'confirmPassword', type: 'password', autocomplete: 'new-password' }),
      isRegister && h('p', { class: 'hint' }, 'At least 8 characters. Only your name and email are stored, with the password securely hashed.'),
      submit);

    form.addEventListener('submit', async e => {
      e.preventDefault();
      const data = Object.fromEntries(new FormData(form));
      let problem = '';
      if (isRegister && !data.name.trim()) problem = 'Please enter your full name.';
      else if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(data.email.trim())) problem = 'Please enter a valid email address.';
      else if (isRegister && data.password.length < 8) problem = 'Password must be at least 8 characters.';
      else if (isRegister && data.password !== data.confirmPassword) problem = 'Passwords do not match.';
      else if (!data.password) problem = 'Please enter your password.';
      if (problem) { error.textContent = problem; error.hidden = false; return; }

      submit.disabled = true;
      const result = await api(isRegister ? '/api/register' : '/api/login', { method: 'POST', body: data });
      submit.disabled = false;
      if (!result.ok) { error.textContent = errorText(result); error.hidden = false; return; }
      state.user = result.data.user;
      toast(isRegister ? 'Welcome to ResearchNet!' : 'Welcome back!');
      const next = query.get('next');
      location.hash = next && next.startsWith('/') ? '#' + next : '#/dashboard';
    });

    main.append(h('div', { class: 'auth-wrap' },
      h('div', { class: 'panel' },
        h('h1', null, isRegister ? 'Create your account' : 'Log in to ResearchNet'),
        h('p', { class: 'muted' }, isRegister ? 'Save papers, keep your search history and get personal recommendations.' : 'Continue to your research dashboard.'),
        form),
      h('p', { class: 'auth-switch' }, isRegister
        ? ['Already have an account? ', h('a', { href: link('/login', { next: query.get('next') }) }, 'Log in')]
        : ['New to ResearchNet? ', h('a', { href: link('/register', { next: query.get('next') }) }, 'Create an account')])));
    form.querySelector('input').focus();
  }


  async function dashboardPage(match, query, isCurrent) {
    const firstName = state.user.name.split(/\s+/)[0];
    const recs = h('div', { class: 'stack compact' }, loading());
    const searches = h('div', { class: 'chips' }, loading());
    const viewed = h('div', null, loading());
    const saved = h('div', null, loading());
    const categories = h('div', { class: 'chips' });

    append(main, [
      h('div', { class: 'welcome' },
        h('div', null, h('h1', null, `Welcome back, ${firstName}`), h('p', { class: 'muted' }, 'Pick up where you left off, or start a new search.'))),
      searchBox({ large: true }),
      h('div', { class: 'quick-links' },
        quickLink('#/search', 'search', 'Search papers', 'With advanced filters'),
        quickLink('#/recommendations', 'spark', 'For you', 'Personal recommendations'),
        quickLink('#/saved', 'bookmark', 'Saved papers', 'Your reading list'),
        quickLink('#/history', 'clock', 'History', 'Searches and viewed papers')),
      h('div', { class: 'search-layout section' },
        h('div', { class: 'stack' },
          h('div', { class: 'panel' }, h('h2', null, 'Recent searches'), searches),
          h('div', { class: 'panel' }, h('div', { class: 'section-title' }, h('h2', null, 'Recently viewed'), h('a', { href: '#/history' }, 'All')), viewed),
          h('div', { class: 'panel' }, h('div', { class: 'section-title' }, h('h2', null, 'Saved papers'), h('a', { href: '#/saved' }, 'All')), saved)),
        h('div', null,
          h('div', { class: 'section-title' }, h('h2', null, 'Recommended for you'), h('a', { href: '#/recommendations' }, 'See all →')),
          recs)),
      h('section', { class: 'section' }, h('h2', null, 'Research categories'), categories),
    ]);

    const [r, hist, marks, cats] = await Promise.all([
      api('/api/recommendations?limit=4'), api('/api/history?limit=6'), api('/api/bookmarks?limit=5'), api('/api/categories')]);
    if (!isCurrent()) return;

    clear(recs);
    if (r.ok && r.data.results.length) r.data.results.forEach(rec => recs.append(recommendationCard(rec)));
    else recs.append(emptyState('No recommendations yet', 'Search, open and save papers, or add interests to your profile.'));

    clear(searches);
    clear(viewed);
    if (hist.ok) {
      const queries = hist.data.searches.filter(s => s.query);
      if (queries.length) queries.forEach(s => searches.append(h('a', { class: 'chip', href: link('/search', Object.assign({ q: s.query }, s.filters)) }, s.query)));
      else searches.append(h('span', { class: 'muted' }, 'Your searches will appear here.'));
      if (hist.data.viewed.length) hist.data.viewed.forEach(p => viewed.append(listRow(p, fmtDate(p.viewedAt))));
      else viewed.append(h('span', { class: 'muted' }, 'Papers you open will appear here.'));
    }
    clear(saved);
    if (marks.ok && marks.data.length) marks.data.forEach(p => saved.append(listRow(p, fmtDate(p.savedAt))));
    else saved.append(h('span', { class: 'muted' }, 'Use the bookmark button on any paper to save it.'));
    if (cats.ok) cats.data.forEach(c => categories.append(h('a', { class: 'chip brand', href: link('/search', { category: c.name }) }, c.name)));
  }

  function quickLink(href, iconName, title, subtitle) {
    return h('a', { class: 'quick-link', href }, icon(iconName), h('span', null, title, h('small', null, subtitle)));
  }

  function listRow(paper, note) {
    return h('div', { class: 'list-row' },
      h('div', { class: 'grow' },
        h('a', { class: 'title', href: '#/paper/' + paper.id, title: paper.title }, paper.title),
        h('span', { class: 'hint' }, [paper.journal, paper.year, note].filter(Boolean).join(' · '))));
  }


  const SORTS = [
    ['relevance', 'Relevance'], ['newest', 'Newest first'], ['oldest', 'Oldest first'],
    ['impact_desc', 'Impact: high to low'], ['impact_asc', 'Impact: low to high'],
    ['citations', 'Most cited'], ['author', 'First author A–Z'],
  ];
  const FILTER_KEYS = ['author', 'journal', 'category', 'keyword', 'type', 'year', 'year_from', 'year_to', 'min_impact', 'max_impact'];
  const FILTER_LABELS = {
    author: 'Author', journal: 'Journal', category: 'Category', keyword: 'Keyword', type: 'Type', year: 'Year',
    year_from: 'From', year_to: 'To', min_impact: 'Impact ≥', max_impact: 'Impact ≤',
  };

  async function searchPage(match, query, isCurrent) {
    const params = Object.fromEntries(query);
    if (!state.filterOptions) {
      const options = await api('/api/filters');
      if (!isCurrent()) return;
      state.filterOptions = options.ok ? options.data : { categories: [], types: [], years: {}, impactFactor: {} };
    }
    const opts = state.filterOptions;

    const input = (name, label, attrs = {}) => h('div', { class: 'field' },
      h('label', { for: 'flt-' + name }, label),
      h('input', Object.assign({ id: 'flt-' + name, name, class: 'input', value: params[name] || '' }, attrs)));
    const select = (name, label, options) => h('div', { class: 'field' },
      h('label', { for: 'flt-' + name }, label),
      h('select', { id: 'flt-' + name, name, class: 'select' },
        h('option', { value: '' }, 'Any'),
        options.map(o => h('option', { value: o.name, selected: params[name] === o.name }, `${o.name} (${fmtNumber(o.papers)})`))));
    const yearAttrs = { type: 'number', min: opts.years.min || 1900, max: opts.years.max || 2100, inputmode: 'numeric' };
    const impactAttrs = { type: 'number', min: 0, step: '0.1', inputmode: 'decimal' };

    const filterForm = h('form', { class: 'filters collapsed', 'aria-label': 'Filters' },
      h('h2', null, 'Filters', h('button', { class: 'btn btn-ghost btn-sm', type: 'button', onclick: () => go('/search', { q: params.q }) }, 'Clear')),
      input('author', 'Author', { placeholder: 'e.g. Kaiming He' }),
      input('journal', 'Journal', { placeholder: 'e.g. IEEE Access' }),
      select('category', 'Research category', opts.categories),
      input('keyword', 'Keyword', { placeholder: 'e.g. deep learning' }),
      select('type', 'Publication type', opts.types),
      h('div', { class: 'field' }, h('label', { for: 'flt-year' }, 'Exact year'),
        h('input', Object.assign({ id: 'flt-year', name: 'year', class: 'input', value: params.year || '' }, yearAttrs))),
      h('div', { class: 'field' }, h('label', null, 'Year range'),
        h('div', { class: 'input-row' },
          h('input', Object.assign({ name: 'year_from', class: 'input', placeholder: 'From', 'aria-label': 'Year from', value: params.year_from || '' }, yearAttrs)),
          h('input', Object.assign({ name: 'year_to', class: 'input', placeholder: 'To', 'aria-label': 'Year to', value: params.year_to || '' }, yearAttrs)))),
      h('div', { class: 'field' }, h('label', null, 'Journal impact'),
        h('div', { class: 'input-row' },
          h('input', Object.assign({ name: 'min_impact', class: 'input', placeholder: 'Min', 'aria-label': 'Minimum impact', value: params.min_impact || '' }, impactAttrs)),
          h('input', Object.assign({ name: 'max_impact', class: 'input', placeholder: 'Max', 'aria-label': 'Maximum impact', value: params.max_impact || '' }, impactAttrs))),
        h('span', { class: 'hint' }, `Journal 2-year mean citedness (${opts.impactFactor.source || 'OpenAlex'}, ${opts.impactFactor.year || 'n/a'}). Papers without a value are excluded when this is set.`)),
      h('button', { class: 'btn btn-primary', type: 'submit' }, 'Apply filters'));

    filterForm.addEventListener('submit', e => {
      e.preventDefault();
      const next = { q: params.q, sort: params.sort };
      for (const [key, value] of new FormData(filterForm)) if (String(value).trim()) next[key] = String(value).trim();
      go('/search', next);
    });

    const toggle = h('button', { class: 'btn btn-secondary btn-sm filters-toggle', type: 'button', 'aria-expanded': 'false',
      onclick: e => {
        const collapsed = filterForm.classList.toggle('collapsed');
        e.currentTarget.setAttribute('aria-expanded', String(!collapsed));
      } }, icon('filter'), 'Filters');

    const sortSelect = h('select', { class: 'select', 'aria-label': 'Sort results',
      onchange: e => go('/search', Object.assign({}, params, { sort: e.target.value, page: '' })) },
      SORTS.map(([value, label]) => h('option', { value, selected: (params.sort || 'relevance') === value }, label)));

    const active = FILTER_KEYS.filter(k => params[k]);
    const results = h('div', { class: 'stack' }, loading('Searching…'));
    const head = h('div', { class: 'results-head' }, h('span', { class: 'count' }, ''), h('div', { class: 'input-row' }, toggle, sortSelect));

    append(main, h('div', { class: 'search-layout' },
      filterForm,
      h('div', null,
        h('h1', null, params.q ? `Results for “${params.q}”` : 'Browse research papers'),
        head,
        active.length > 0 && h('div', { class: 'chips active-filters' }, active.map(k => {
          const rest = Object.assign({}, params);
          delete rest[k];
          delete rest.page;
          return h('span', { class: 'chip brand' }, `${FILTER_LABELS[k]}: ${params[k]}`,
            h('button', { type: 'button', 'aria-label': `Remove ${FILTER_LABELS[k]} filter`, onclick: () => go('/search', rest) }, '×'));
        })),
        results)));

    const result = await api('/api/search?' + new URLSearchParams(Object.assign({ page_size: 10 }, params)));
    if (!isCurrent()) return;
    clear(results);
    if (!result.ok) { results.append(h('div', { class: 'form-error' }, errorText(result))); return; }

    const data = result.data;
    const pages = Math.max(1, Math.ceil(data.total / data.pageSize));
    head.firstChild.textContent = `${fmtNumber(data.total)} paper${data.total === 1 ? '' : 's'} found`;
    if (data.relaxed) results.append(h('div', { class: 'notice' }, 'No paper contains all of your words, so papers containing any of them are shown.'));
    if (!data.results.length) {
      results.append(emptyState('No papers found', 'Try fewer words, check the spelling, or remove some filters.',
        active.length > 0 && h('button', { class: 'btn btn-secondary', onclick: () => go('/search', { q: params.q }) }, 'Clear filters')));
      return;
    }
    data.results.forEach(p => results.append(paperCard(p)));
    if (pages > 1) {
      const page = data.page;
      const to = n => link('/search', Object.assign({}, params, { page: n }));
      results.append(h('nav', { class: 'pager', 'aria-label': 'Pages' },
        page > 1 ? h('a', { class: 'btn btn-secondary', href: to(page - 1) }, '← Previous') : null,
        h('span', null, `Page ${page} of ${fmtNumber(pages)}`),
        page < pages ? h('a', { class: 'btn btn-secondary', href: to(page + 1) }, 'Next →') : null));
    }
  }


  async function paperPage(match, query, isCurrent) {
    const id = match[1];
    main.append(loading());
    const [result, similar] = await Promise.all([api('/api/papers/' + id), api(`/api/papers/${id}/similar?limit=6`)]);
    if (!isCurrent()) return;
    clear(main);
    if (!result.ok) {
      main.append(emptyState(result.status === 404 ? 'Paper not found' : 'Could not load this paper', errorText(result),
        h('a', { class: 'btn btn-primary', href: '#/search' }, 'Search papers')));
      return;
    }
    const p = result.data;
    document.title = p.title + ' · ResearchNet';
    const j = p.journal;
    const doi = p.doi ? safeUrl('https://doi.org/' + p.doi) : null;
    const url = safeUrl(p.url);
    const fact = (label, value) => value != null && value !== '' ? [h('dt', null, label), h('dd', null, value)] : null;

    const impact = j && j.impactFactor != null
      ? [Number(j.impactFactor).toFixed(2), h('div', { class: 'disclaimer' },
          `${j.impactFactorSource}${j.impactFactorYear ? ', retrieved ' + j.impactFactorYear : ''}. This is OpenAlex's 2-year mean citedness of the venue, an impact-factor-like journal metric, not Clarivate's Journal Impact Factor.`)]
      : 'Not available';

    append(main, h('div', { class: 'paper-page' },
      h('article', null,
        p.category && h('a', { class: 'chip brand', href: link('/search', { category: p.category }) }, p.category),
        h('h1', null, p.title),
        h('p', { class: 'author-list' }, p.authors.map((a, i) =>
          [i > 0 ? ', ' : '', h('a', { href: link('/search', { author: a.name }) }, a.name)])),
        h('div', { class: 'paper-meta' },
          j && h('span', { class: 'journal' }, j.name), p.year && h('span', null, p.year), p.type && h('span', null, p.type),
          h('span', null, 'Cited by ' + fmtNumber(p.citedBy))),
        h('div', { class: 'action-row' },
          saveButton(p),
          doi && h('a', { class: 'btn btn-primary', href: doi, target: '_blank', rel: 'noopener' }, 'Open via DOI ', icon('external')),
          url && url !== doi && h('a', { class: 'btn btn-secondary', href: url, target: '_blank', rel: 'noopener' }, 'Publisher page ', icon('external'))),
        h('h2', null, 'Abstract'),
        h('p', { class: 'abstract' }, p.abstract || 'No abstract available.'),
        p.keywords.length > 0 && [h('h2', null, 'Keywords'),
          h('div', { class: 'chips' }, p.keywords.map(k => h('a', { class: 'chip', href: link('/search', { keyword: k }) }, k)))]),
      h('aside', { class: 'panel' },
        h('h2', null, 'Publication details'),
        h('dl', { class: 'facts' },
          fact('Journal', j && j.name), fact('Publisher', j && j.publisher), fact('ISSN', j && j.issn),
          fact('Published', p.publicationDate || p.year), fact('Type', p.type), fact('Category', p.category),
          fact('Topic', p.topic), fact('Cited by', fmtNumber(p.citedBy)), fact('Journal impact', impact),
          fact('DOI', p.doi), fact('Source', `${p.dataSource}${p.retrievedAt ? ', retrieved ' + p.retrievedAt : ''}`)))));

    const similarSection = h('section', { class: 'section' },
      h('div', { class: 'section-title' }, h('h2', null, 'Similar papers'), h('span', { class: 'muted' }, 'TF-IDF cosine similarity, keywords, authors, recency and impact')));
    if (similar.ok && similar.data.length) similarSection.append(h('div', { class: 'stack' }, similar.data.map(recommendationCard)));
    else similarSection.append(emptyState('No similar papers found', null));
    main.append(similarSection);
  }


  async function recommendationsPage(match, query, isCurrent) {
    const list = h('div', { class: 'stack' }, loading('Finding papers for you…'));
    const basis = h('p', { class: 'muted' });
    append(main, [
      h('div', { class: 'page-header' }, h('div', null, h('h1', null, 'Recommended for you'), basis)),
      h('details', { class: 'panel' },
        h('summary', null, h('strong', null, 'How are these chosen?')),
        h('p', null, 'Your saved papers, the papers you opened, your recent searches and the interests on your profile are turned into one TF-IDF interest vector. Every other paper is compared with it by cosine similarity using the inverted index.'),
        h('p', null, 'Each candidate gets five normalized scores (0–100%): content similarity, shared keywords, shared authors, recency and journal impact. The final score is a weighted average: 50% content, 20% keywords, 15% authors, 10% recency and 5% impact. Impact is left out when it is not known, so it never outweighs relevance. Papers you already saved or opened are not shown again.')),
      h('section', { class: 'section' }, list),
    ]);

    const result = await api('/api/recommendations?limit=20');
    if (!isCurrent()) return;
    clear(list);
    if (!result.ok) { list.append(h('div', { class: 'form-error' }, errorText(result))); return; }
    const b = result.data.basedOn;
    const parts = [];
    if (b.saved) parts.push(`${b.saved} saved paper${b.saved === 1 ? '' : 's'}`);
    if (b.viewed) parts.push(`${b.viewed} viewed paper${b.viewed === 1 ? '' : 's'}`);
    if (b.searches) parts.push(`${b.searches} recent search${b.searches === 1 ? '' : 'es'}`);
    if (b.interests.length) parts.push('your interests (' + b.interests.join(', ') + ')');
    basis.textContent = parts.length ? 'Based on ' + parts.join(', ') + '.'
      : 'You are new here, so these are highly cited recent papers. Save papers or add interests on your profile to personalize them.';
    if (!parts.length) list.append(h('div', { class: 'notice' }, h('a', { href: '#/profile' }, 'Add research interests'), ' to get personal recommendations.'));
    result.data.results.forEach(rec => list.append(recommendationCard(rec)));
  }


  async function savedPage(match, query, isCurrent) {
    const list = h('div', { class: 'stack' }, loading());
    append(main, [h('div', { class: 'page-header' }, h('div', null, h('h1', null, 'Saved papers'), h('p', null, 'Your personal reading list.'))), list]);
    const result = await api('/api/bookmarks');
    if (!isCurrent()) return;
    clear(list);
    if (!result.ok) { list.append(h('div', { class: 'form-error' }, errorText(result))); return; }
    if (!result.data.length) {
      list.append(emptyState('No saved papers yet', 'Use the bookmark button on any paper to save it here.', h('a', { class: 'btn btn-primary', href: '#/search' }, 'Find papers')));
      return;
    }
    for (const p of result.data) {
      const card = paperCard(p, { extra: h('p', { class: 'hint' }, 'Saved ' + fmtDate(p.savedAt)) });
      card.addEventListener('bookmark-change', e => { if (!e.detail.bookmarked) card.remove(); });
      list.append(card);
    }
  }


  async function historyPage(match, query, isCurrent) {
    const tab = query.get('tab') === 'viewed' ? 'viewed' : 'searches';
    const body = h('div', null, loading());
    const clearArea = h('div', { class: 'confirm-row' });

    function showClearButton() {
      clear(clearArea).append(h('button', { class: 'btn btn-danger btn-sm', onclick: askClear }, icon('trash'), 'Clear history'));
    }
    function askClear() {
      clear(clearArea).append(h('span', null, 'Delete all searches and viewed papers?'),
        h('button', { class: 'btn btn-danger btn-sm', onclick: async () => {
          const r = await api('/api/history', { method: 'DELETE' });
          if (r.ok) { toast('History cleared'); render(); } else toast(errorText(r));
        } }, 'Yes, delete'),
        h('button', { class: 'btn btn-ghost btn-sm', onclick: showClearButton }, 'Cancel'));
    }
    showClearButton();

    const tabButton = (name, label) => h('button', { class: 'tab', role: 'tab', 'aria-selected': String(tab === name),
      onclick: () => go('/history', { tab: name }) }, label);
    append(main, [
      h('div', { class: 'page-header' }, h('div', null, h('h1', null, 'History'), h('p', null, 'Your searches and the papers you opened.')), clearArea),
      h('div', { class: 'tabs', role: 'tablist' }, tabButton('searches', 'Searches'), tabButton('viewed', 'Viewed papers')),
      body,
    ]);

    const result = await api('/api/history?limit=100');
    if (!isCurrent()) return;
    clear(body);
    if (!result.ok) { body.append(h('div', { class: 'form-error' }, errorText(result))); return; }

    if (tab === 'searches') {
      if (!result.data.searches.length) { body.append(emptyState('No searches yet', 'Searches you make while logged in are kept here.')); return; }
      const panel = h('div', { class: 'panel' });
      for (const s of result.data.searches) {
        const filters = Object.entries(s.filters).filter(([k]) => k !== 'sort').map(([k, v]) => `${FILTER_LABELS[k] || k}: ${v}`);
        const row = h('div', { class: 'list-row' },
          h('div', { class: 'grow' },
            h('a', { class: 'title', href: link('/search', Object.assign({ q: s.query }, s.filters)) }, s.query || '(filters only)'),
            h('span', { class: 'hint' }, [filters.join(', '), `${fmtNumber(s.resultCount)} results`, fmtDate(s.searchedAt)].filter(Boolean).join(' · '))),
          h('button', { class: 'btn btn-ghost btn-sm', 'aria-label': 'Delete this search', title: 'Delete', onclick: async () => {
            const r = await api('/api/history/searches/' + s.id, { method: 'DELETE' });
            if (r.ok) row.remove(); else toast(errorText(r));
          } }, icon('trash')));
        panel.append(row);
      }
      body.append(panel);
    } else {
      if (!result.data.viewed.length) { body.append(emptyState('No viewed papers yet', 'Papers you open while logged in are kept here.')); return; }
      const list = h('div', { class: 'stack compact' });
      for (const p of result.data.viewed) {
        const card = paperCard(p, {
          extra: h('p', { class: 'hint' }, 'Viewed ' + fmtDate(p.viewedAt)),
          actions: h('button', { class: 'btn btn-ghost btn-sm', onclick: async () => {
            const r = await api('/api/history/views/' + p.id, { method: 'DELETE' });
            if (r.ok) card.remove(); else toast(errorText(r));
          } }, icon('trash'), 'Remove'),
        });
        list.append(card);
      }
      body.append(list);
    }
  }


  async function profilePage(match, query, isCurrent) {
    main.append(loading());
    const [result, cats] = await Promise.all([api('/api/profile'), api('/api/categories')]);
    if (!isCurrent()) return;
    clear(main);
    if (!result.ok) { main.append(h('div', { class: 'form-error' }, errorText(result))); return; }
    const profile = result.data;
    let interests = [...profile.interests];

    const nameMsg = h('div', { role: 'status' });
    const nameInput = h('input', { id: 'p-name', class: 'input', value: profile.user.name, maxlength: 100, required: true });
    const nameForm = h('form', null, nameMsg,
      h('div', { class: 'field' }, h('label', { for: 'p-name' }, 'Full name'), nameInput),
      h('div', { class: 'field' }, h('label', { for: 'p-email' }, 'Email address'), h('input', { id: 'p-email', class: 'input', value: profile.user.email, disabled: true })),
      h('button', { class: 'btn btn-primary', type: 'submit' }, 'Save name'));
    nameForm.addEventListener('submit', async e => {
      e.preventDefault();
      const r = await api('/api/profile', { method: 'PUT', body: { name: nameInput.value } });
      clear(nameMsg).append(h('div', { class: r.ok ? 'form-ok' : 'form-error' }, r.ok ? 'Name updated.' : errorText(r)));
      if (r.ok) { state.user = r.data.user; renderNav(); }
    });

    const chips = h('div', { class: 'chips' });
    const interestMsg = h('div', { role: 'status' });
    const interestInput = h('input', { class: 'input', placeholder: 'e.g. graph neural networks', maxlength: 60, 'aria-label': 'New interest' });
    function drawInterests() {
      clear(chips);
      if (!interests.length) chips.append(h('span', { class: 'muted' }, 'No interests yet.'));
      interests.forEach(i => chips.append(h('span', { class: 'chip brand' }, i,
        h('button', { type: 'button', 'aria-label': 'Remove ' + i, onclick: () => { interests = interests.filter(x => x !== i); drawInterests(); } }, '×'))));
    }
    function addInterest(text) {
      const value = text.trim();
      if (value && !interests.some(i => i.toLowerCase() === value.toLowerCase()) && interests.length < 20) interests.push(value);
      drawInterests();
    }
    drawInterests();
    const suggestions = h('div', { class: 'chips' }, (cats.ok ? cats.data : []).map(c =>
      h('button', { class: 'chip', type: 'button', onclick: () => addInterest(c.name) }, '+ ' + c.name)));
    const interestForm = h('form', null, interestMsg, chips,
      h('div', { class: 'input-row section' }, interestInput, h('button', { class: 'btn btn-secondary', type: 'button', onclick: () => { addInterest(interestInput.value); interestInput.value = ''; } }, 'Add')),
      h('p', { class: 'hint' }, 'Or pick a research area:'), suggestions,
      h('div', { class: 'section' }, h('button', { class: 'btn btn-primary', type: 'submit' }, 'Save interests')));
    interestInput.addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); addInterest(interestInput.value); interestInput.value = ''; } });
    interestForm.addEventListener('submit', async e => {
      e.preventDefault();
      const r = await api('/api/profile', { method: 'PUT', body: { interests } });
      clear(interestMsg).append(h('div', { class: r.ok ? 'form-ok' : 'form-error' }, r.ok ? 'Interests saved. Your recommendations now use them.' : errorText(r)));
      if (r.ok) { interests = r.data.interests; drawInterests(); }
    });

    const pwMsg = h('div', { role: 'status' });
    const pwForm = h('form', null, pwMsg,
      h('div', { class: 'field' }, h('label', { for: 'p-cur' }, 'Current password'), h('input', { id: 'p-cur', name: 'currentPassword', type: 'password', class: 'input', autocomplete: 'current-password', required: true })),
      h('div', { class: 'field' }, h('label', { for: 'p-new' }, 'New password'), h('input', { id: 'p-new', name: 'newPassword', type: 'password', class: 'input', autocomplete: 'new-password', minlength: 8, maxlength: 72, required: true })),
      h('button', { class: 'btn btn-secondary', type: 'submit' }, 'Change password'));
    pwForm.addEventListener('submit', async e => {
      e.preventDefault();
      const data = Object.fromEntries(new FormData(pwForm));
      if (data.newPassword.length < 8) { clear(pwMsg).append(h('div', { class: 'form-error' }, 'New password must be at least 8 characters.')); return; }
      const r = await api('/api/profile/password', { method: 'PUT', body: data });
      clear(pwMsg).append(h('div', { class: r.ok ? 'form-ok' : 'form-error' }, r.ok ? 'Password changed. Other devices were logged out.' : errorText(r)));
      if (r.ok) pwForm.reset();
    });

    const counts = profile.counts;
    append(main, [
      h('div', { class: 'page-header' },
        h('div', null, h('h1', null, 'Your profile'), h('p', null, 'Member since ' + fmtDate(profile.memberSince))),
        h('button', { class: 'btn btn-secondary', onclick: async () => {
          await api('/api/logout', { method: 'POST' });
          state.user = null;
          toast('You are logged out');
          go('/');
        } }, icon('logout'), 'Log out')),
      h('div', { class: 'grid-3' },
        h('a', { class: 'quick-link', href: '#/saved' }, icon('bookmark'), h('span', null, fmtNumber(counts.bookmarks), h('small', null, 'saved papers'))),
        h('a', { class: 'quick-link', href: '#/history' }, icon('search'), h('span', null, fmtNumber(counts.searches), h('small', null, 'searches'))),
        h('a', { class: 'quick-link', href: '#/history?tab=viewed' }, icon('clock'), h('span', null, fmtNumber(counts.views), h('small', null, 'papers viewed')))),
      h('div', { class: 'grid-2 section' },
        h('div', { class: 'stack' },
          h('div', { class: 'panel' }, h('h2', null, 'Account'), nameForm),
          h('div', { class: 'panel' }, h('h2', null, 'Change password'), pwForm)),
        h('div', { class: 'panel' }, h('h2', null, 'Research interests'),
          h('p', { class: 'muted' }, 'Interests guide your recommendations together with your saved papers and searches.'),
          interestForm)),
    ]);
  }


  async function start() {
    const me = await api('/api/me');
    state.user = me.ok ? me.data.user : null;
    window.addEventListener('hashchange', () => { document.title = 'ResearchNet'; render(); });
    render();
  }

  start();
})();
