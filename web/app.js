// ResearchNet web page. It only shows data: searching, filtering, sorting, history and
// recommendations all happen in the C++ server. Text is always inserted with textContent.

const queryInput = document.getElementById("query");
const suggestionList = document.getElementById("suggestions");
let typingTimer = null;
let filterTimer = null;
let currentQuartile = "";

// ---------- small helpers ----------
function make(tag, className, text) {
  const element = document.createElement(tag);
  if (className) element.className = className;
  if (text !== undefined && text !== null) element.textContent = text;
  return element;
}

async function api(url, method) {
  const response = await fetch(url, { method: method || "GET" });
  return response.json();
}

// "A. Name, B. Name, C. Name" -> "A. Name et al."
function shortAuthors(authors) {
  const names = authors.split(",");
  return names.length > 2 ? names[0].trim() + " et al." : authors;
}

// The "2024 | Q1 | IF: 9.80 | Journal" line used on every card.
function metaLine(paper) {
  const meta = make("div", "meta");
  meta.appendChild(make("span", "", String(paper.year)));
  if (paper.quartile) meta.appendChild(make("span", "badge " + paper.quartile, paper.quartile));
  if (paper.impactFactor > 0) meta.appendChild(make("span", "impact", "IF: " + paper.impactFactor.toFixed(2)));
  else meta.appendChild(make("span", "", "No impact factor"));
  meta.appendChild(make("span", "journal", paper.journal));
  return meta;
}

function paperCard(paper) {
  const card = make("li", "card clickable");
  card.appendChild(make("h3", "card-title", paper.title));
  card.appendChild(make("p", "authors", shortAuthors(paper.authors)));
  card.appendChild(metaLine(paper));
  card.onclick = function () { location.hash = "#/paper/" + paper.id; };
  return card;
}

// ---------- page switching (#/search, #/paper/12, #/history) ----------
function showPage(name) {
  for (const page of ["search", "paper", "history"]) {
    document.getElementById("page-" + page).hidden = page !== name;
  }
  document.getElementById("nav-search").classList.toggle("active", name === "search");
  document.getElementById("nav-history").classList.toggle("active", name === "history");
  window.scrollTo(0, 0);
}

function route() {
  const hash = location.hash || "#/search";
  if (hash.startsWith("#/paper/")) {
    showPage("paper");
    openPaper(parseInt(hash.substring(8)));
  } else if (hash === "#/history") {
    showPage("history");
    loadHistory();
  } else {
    showPage("search");
  }
}

// ---------- search + filters ----------
function filterParams() {
  return "&sort=" + document.getElementById("f-sort").value +
         "&quartile=" + currentQuartile +
         "&minImpact=" + document.getElementById("f-impact").value +
         "&yearFrom=" + encodeURIComponent(document.getElementById("f-from").value) +
         "&yearTo=" + encodeURIComponent(document.getElementById("f-to").value) +
         "&author=" + encodeURIComponent(document.getElementById("f-author").value);
}

async function search() {
  suggestionList.hidden = true;
  const text = queryInput.value.trim();
  const data = await api("/api/search?q=" + encodeURIComponent(text) + filterParams());
  const what = text ? " for \"" + text + "\"" : "";
  document.getElementById("status").textContent = data.count + " paper(s)" + what;
  const results = document.getElementById("results");
  results.replaceChildren();
  if (data.count === 0) {
    results.appendChild(make("li", "empty", "No papers match. Try other words or remove some filters."));
  }
  for (const paper of data.results) results.appendChild(paperCard(paper));
}

function searchSoon() {               // used by filter inputs: wait until the user stops typing
  clearTimeout(filterTimer);
  filterTimer = setTimeout(search, 300);
}

// ---------- autocomplete (Trie) ----------
async function autocomplete() {
  const words = queryInput.value.split(" ");
  const lastWord = words[words.length - 1];
  if (lastWord.length < 2) { suggestionList.hidden = true; return; }

  const suggestions = await api("/api/autocomplete?prefix=" + encodeURIComponent(lastWord));
  suggestionList.replaceChildren();
  for (const suggestion of suggestions) {
    const item = make("li", "", suggestion);
    item.onmousedown = function () {
      words[words.length - 1] = suggestion;      // replace the word being typed
      queryInput.value = words.join(" ");
      search();
    };
    suggestionList.appendChild(item);
  }
  suggestionList.hidden = suggestions.length === 0;
}

// ---------- paper page (PostgreSQL + recommendations) ----------
async function openPaper(id) {
  const box = document.getElementById("paper");
  box.replaceChildren(make("p", "muted", "Loading…"));
  const paper = await api("/api/paper?id=" + id);         // also adds the paper to the history
  if (paper.error) { box.replaceChildren(make("p", "empty", paper.error)); return; }
  const recs = await api("/api/recommend?id=" + id);
  updateHistoryCount();

  const main = make("div", "paper-main");
  main.appendChild(make("h1", "", paper.title));
  main.appendChild(make("p", "authors", paper.authors));
  main.appendChild(metaLine(paper));
  main.appendChild(make("h2", "", "ABSTRACT"));
  main.appendChild(make("p", "abstract", paper.abstract || "No abstract available."));
  main.appendChild(make("h2", "", "KEYWORDS"));
  const chips = make("div", "chips");
  for (const keyword of paper.keywords.split(",")) {
    const chip = make("button", "chip", keyword.trim());
    chip.onclick = function () { queryInput.value = keyword.trim(); location.hash = "#/search"; search(); };
    chips.appendChild(chip);
  }
  main.appendChild(chips);
  if (paper.url) {
    const link = make("a", "button", "Open paper");
    link.href = paper.url;
    link.target = "_blank";
    link.rel = "noopener";
    main.appendChild(link);
  }

  const recBox = make("div", "recs");
  recBox.appendChild(make("h2", "", "Recommended papers"));
  const list = make("ul", "cards");
  if (recs.length === 0) list.appendChild(make("li", "empty", "No similar papers found."));
  for (const rec of recs) {
    const card = paperCard(rec.paper);
    const reasons = [];
    if (rec.commonKeywords > 0) reasons.push(rec.commonKeywords + " shared keyword word(s)");
    if (rec.commonAuthors > 0) reasons.push(rec.commonAuthors + " shared author(s)");
    if (rec.paper.impactFactor > 0) reasons.push("IF " + rec.paper.impactFactor.toFixed(2));
    if (rec.paper.quartile) reasons.push(rec.paper.quartile);
    card.appendChild(make("div", "reason", "Score " + rec.score + ": " + reasons.join(" · ")));
    list.appendChild(card);
  }
  recBox.appendChild(list);
  box.replaceChildren(main, recBox);
}

// ---------- history page (linked list) ----------
async function updateHistoryCount() {
  const items = await api("/api/history");
  document.getElementById("history-count").textContent = items.length > 0 ? items.length : "";
  return items;
}

async function loadHistory() {
  const items = await updateHistoryCount();
  const list = document.getElementById("history");
  list.replaceChildren();
  if (items.length === 0) {
    list.appendChild(make("li", "empty", "You haven't opened any papers yet."));
    return;
  }
  for (const paper of items) {
    const card = make("li", "card history-item");
    const info = make("div", "info");
    info.appendChild(make("h3", "card-title", paper.title));
    info.appendChild(make("p", "authors", shortAuthors(paper.authors)));
    info.appendChild(metaLine(paper));
    info.onclick = function () { location.hash = "#/paper/" + paper.id; };
    const remove = make("button", "remove", "×");
    remove.title = "Remove from history";
    remove.onclick = async function () {
      await api("/api/history?id=" + paper.id, "DELETE");     // deleteHistoryItem()
      loadHistory();
    };
    card.appendChild(info);
    card.appendChild(remove);
    list.appendChild(card);
  }
}

// ---------- events ----------
document.getElementById("search-form").onsubmit = function (event) { event.preventDefault(); search(); };
queryInput.oninput = function () { clearTimeout(typingTimer); typingTimer = setTimeout(autocomplete, 200); };
queryInput.onblur = function () { setTimeout(function () { suggestionList.hidden = true; }, 150); };

for (const id of ["f-sort", "f-impact"]) document.getElementById(id).onchange = search;
for (const id of ["f-from", "f-to", "f-author"]) document.getElementById(id).oninput = searchSoon;
for (const button of document.querySelectorAll("#f-quartile button")) {
  button.onclick = function () {
    currentQuartile = button.dataset.q;
    for (const other of document.querySelectorAll("#f-quartile button")) other.classList.toggle("on", other === button);
    search();
  };
}
document.getElementById("f-reset").onclick = function () {
  document.getElementById("f-sort").value = "0";
  document.getElementById("f-impact").value = "0";
  for (const id of ["f-from", "f-to", "f-author"]) document.getElementById(id).value = "";
  document.querySelector("#f-quartile button").click();       // "All"
};
document.getElementById("clear-history").onclick = async function () {
  if (!confirm("Clear your whole browsing history?")) return;
  await api("/api/history", "DELETE");                        // clearHistory()
  loadHistory();
};

window.onhashchange = route;
api("/api/stats").then(function (stats) {
  document.getElementById("stats").textContent =
    stats.papers + " papers loaded from PostgreSQL · " + stats.words + " words in the inverted index";
});
route();
search();
updateHistoryCount();
