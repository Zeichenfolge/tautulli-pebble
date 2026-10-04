/*
 * Tautulli für Pebble – Handy-Teil (PebbleKit JS)
 *
 * Ruft die Tautulli-API (v2) auf, bereitet alle Texte in der eingestellten
 * Sprache (Deutsch/Englisch) auf und schickt sie per AppMessage an die Uhr.
 * Protokoll siehe src/c/tautulli.c.
 *
 * Genutzte API-Befehle:
 *   server_status, get_activity, get_history, get_recently_added,
 *   get_home_stats, get_users_table, get_libraries_table, get_plays_by_date,
 *   terminate_session
 *
 * Demo-Modus: Steht als Adresse "demo" in den Einstellungen, liefert
 * src/pkjs/demo.js erfundene Beispieldaten (für Screenshots und zum Ausprobieren).
 *
 * Hinweis: Nur ES5 verwenden – der Pebble-JS-Bundler (webpack 1) und
 * JavaScriptCore auf iOS kennen kein modernes JavaScript.
 */
var Clay = require('pebble-clay');
var clayConfig = require('./config');
var demo = require('./demo');

var LIST_HOME = 1, LIST_HISTORY = 2, LIST_RECENT = 3, LIST_STATS = 4, LIST_USERS = 5,
    LIST_LIBRARIES = 6, LIST_USER_HISTORY = 7, LIST_CHART = 8;
var T_LIST_BEGIN = 1, T_ITEM = 2, T_LIST_END = 3, T_ERROR = 4, T_RESULT = 5, T_CHART = 6;
var KIND_NONE = 0, KIND_DETAIL = 1, KIND_STREAM = 2, KIND_USER = 3;
var CMD_LOAD = 1, CMD_TERMINATE = 2;
var MAX_ITEMS = 30;

var DEFAULTS = {
  CfgUrl: '',
  CfgApiKey: '',
  CfgCount: 15,
  CfgLang: 'auto'
};

/* ------------------------------------------------------------------ */
/* Texte                                                               */
/* ------------------------------------------------------------------ */

var TEXT = {
  de: {
    weekdays: ['So', 'Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa'],
    today: 'heute', yesterday: 'gestern',
    hours: 'Std', minutes: 'Min', lessMinute: '< 1 Min',
    decimal: ',', thousands: '.',
    playing: 'Läuft', paused: 'Pausiert', buffering: 'Puffert', pause: 'Pause',
    user: 'Nutzer', device: 'Gerät', state: 'Status', time: 'Zeit', quality: 'Qualität',
    bandwidth: 'Bandbreite', network: 'Netz', start: 'Start', end: 'Ende', stillPlaying: 'Läuft noch',
    duration: 'Dauer', watchedAll: 'Ganz gesehen', watchedPct: '% gesehen', playback: 'Wiedergabe',
    type: 'Typ', library: 'Bibliothek', added: 'Hinzugefügt', year: 'Jahr', genre: 'Genre', length: 'Länge',
    film: 'Film', episode: 'Folge', season: 'Staffel',
    types: { movie: 'Film', show: 'Serie', season: 'Staffel', episode: 'Folge', artist: 'Künstler',
             album: 'Album', track: 'Titel', photo: 'Foto' },
    // Startbildschirm
    plexDown: 'Plex nicht erreichbar', plexOnline: 'Plex online', plexIsOnline: 'Plex ist online',
    tautulliNoPlex: 'Tautulli erreicht Plex nicht', tautulliDown: 'Tautulli nicht erreichbar',
    streamOne: 'Stream', streamMany: 'Streams', nowPlaying: 'Jetzt läuft', nothingPlaying: 'Gerade läuft nichts',
    stopQuestion: 'Stream von {user} beenden?', stopMessage: 'Der Stream wurde beendet.',
    // Listen
    history: 'Verlauf', noHistory: 'Noch kein Verlauf', recent: 'Neu hinzugefügt', nothingNew: 'Nichts Neues',
    users: 'Nutzer', noUsers: 'Keine Nutzer', lastSeen: 'zuletzt', never: 'noch nie geschaut',
    libraries: 'Bibliotheken', noLibraries: 'Keine Bibliotheken', plays: 'Wiedergaben', watchTime: 'Wiedergabezeit',
    lastPlayed: 'Zuletzt',
    libUnits: { movie: ['Filme'], show: ['Serien', 'Folgen'], artist: ['Künstler', 'Alben'], photo: ['Fotos'] },
    // Statistik
    stats: 'Statistik', statToday: 'Heute', stat7: 'Letzte 7 Tage', stat30: 'Letzte 30 Tage',
    secWatchTime: 'Wiedergabezeit', secTopUsers: 'Top-Nutzer · 30 Tage', secTopMovies: 'Filme · 30 Tage',
    secTopShows: 'Serien · 30 Tage', noData: 'Keine Daten',
    // Diagramm
    chartHeader: '{days} Tage · {plays} Wiedergaben', chartLegend: 'Serien|Filme|Musik',
    // Fehler
    setupNeeded: 'Einrichtung nötig',
    noApiKey: 'API-Key fehlt: bitte in der Pebble-App eintragen',
    noUrl: 'Tautulli-Adresse fehlt: bitte in der Pebble-App eintragen',
    badResponse: 'Ungültige Antwort von Tautulli', httpError: 'HTTP-Fehler', badKey: 'API-Key ungültig',
    unreachable: 'Tautulli nicht erreichbar. Handy im Heim-WLAN?',
    timeout: 'Zeitüberschreitung – Tautulli antwortet nicht',
    stopped: 'Stream beendet', error: 'Fehler', needsPlexPass: 'Beenden braucht Plex Pass'
  },
  en: {
    weekdays: ['Su', 'Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa'],
    months: ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'],
    today: 'today', yesterday: 'yesterday',
    hours: 'h', minutes: 'min', lessMinute: '< 1 min',
    decimal: '.', thousands: ',',
    playing: 'Playing', paused: 'Paused', buffering: 'Buffering', pause: 'Paused',
    user: 'User', device: 'Device', state: 'State', time: 'Time', quality: 'Quality',
    bandwidth: 'Bandwidth', network: 'Network', start: 'Started', end: 'Stopped', stillPlaying: 'Still playing',
    duration: 'Duration', watchedAll: 'Watched completely', watchedPct: '% watched', playback: 'Playback',
    type: 'Type', library: 'Library', added: 'Added', year: 'Year', genre: 'Genre', length: 'Length',
    film: 'Movie', episode: 'Episode', season: 'Season',
    types: { movie: 'Movie', show: 'Show', season: 'Season', episode: 'Episode', artist: 'Artist',
             album: 'Album', track: 'Track', photo: 'Photo' },
    plexDown: 'Plex unreachable', plexOnline: 'Plex online', plexIsOnline: 'Plex is online',
    tautulliNoPlex: 'Tautulli can\'t reach Plex', tautulliDown: 'Tautulli unreachable',
    streamOne: 'stream', streamMany: 'streams', nowPlaying: 'Now playing', nothingPlaying: 'Nothing playing',
    stopQuestion: 'Stop {user}\'s stream?', stopMessage: 'The stream has been stopped.',
    history: 'History', noHistory: 'No history yet', recent: 'Recently added', nothingNew: 'Nothing new',
    users: 'Users', noUsers: 'No users', lastSeen: 'last', never: 'never watched',
    libraries: 'Libraries', noLibraries: 'No libraries', plays: 'Plays', watchTime: 'Watch time',
    lastPlayed: 'Last played',
    libUnits: { movie: ['movies'], show: ['shows', 'episodes'], artist: ['artists', 'albums'], photo: ['photos'] },
    stats: 'Statistics', statToday: 'Today', stat7: 'Last 7 days', stat30: 'Last 30 days',
    secWatchTime: 'Watch time', secTopUsers: 'Top users · 30 days', secTopMovies: 'Movies · 30 days',
    secTopShows: 'Shows · 30 days', noData: 'No data',
    chartHeader: '{days} days · {plays} plays', chartLegend: 'Shows|Movies|Music',
    setupNeeded: 'Setup needed',
    noApiKey: 'API key missing: add it in the Pebble app',
    noUrl: 'Tautulli address missing: add it in the Pebble app',
    badResponse: 'Invalid response from Tautulli', httpError: 'HTTP error', badKey: 'Invalid API key',
    unreachable: 'Can\'t reach Tautulli. Phone on home Wi-Fi?',
    timeout: 'Timeout – Tautulli is not responding',
    stopped: 'Stream stopped', error: 'Error', needsPlexPass: 'Stopping needs Plex Pass'
  }
};

/* ------------------------------------------------------------------ */
/* Einstellungen und Sprache                                           */
/* ------------------------------------------------------------------ */

function settings() {
  var s = {};
  try { s = JSON.parse(localStorage.getItem('clay-settings')) || {}; } catch (e) { s = {}; }
  var out = {};
  for (var k in DEFAULTS) {
    var v = s[k];
    out[k] = (v === undefined || v === null || v === '') ? DEFAULTS[k] : v;
  }
  out.CfgCount = Math.max(5, Math.min(25, parseInt(out.CfgCount, 10) || 15));
  out.CfgApiKey = String(out.CfgApiKey).trim();
  out.CfgUrl = String(out.CfgUrl).trim();
  return out;
}

// 'de' oder 'en'; "auto" folgt der Sprache der Uhr
function lang() {
  var s = settings().CfgLang;
  if (s === 'de' || s === 'en') return s;
  var loc = '';
  try { loc = (Pebble.getActiveWatchInfo() || {}).language || ''; } catch (e) { loc = ''; }
  if (!loc && typeof navigator !== 'undefined') loc = navigator.language || '';
  return /^de/i.test(loc) ? 'de' : 'en';
}

function L() { return TEXT[lang()]; }
function langCode() { return lang() === 'en' ? 1 : 0; }

function fill(tpl, values) {
  return tpl.replace(/\{(\w+)\}/g, function (m, k) { return values[k] !== undefined ? values[k] : m; });
}

function isDemo() { return settings().CfgUrl.toLowerCase() === 'demo'; }

/* ------------------------------------------------------------------ */
/* Formatierung                                                        */
/* ------------------------------------------------------------------ */

function utf8len(s) {
  try { return unescape(encodeURIComponent(s)).length; } catch (e) { return s.length * 3; }
}

// Die Schriften der Uhr kennen nur lateinische Zeichen und etwas Typografie.
// Emojis und Symbole würden als Kästchen erscheinen und werden entfernt.
function keepDrawable(s) {
  var out = '';
  for (var i = 0; i < s.length; i++) {
    var c = s.charCodeAt(i);
    if (c < 0x2000 || (c >= 0x2010 && c <= 0x2026) || c === 0x20AC) out += s.charAt(i);
  }
  return out.replace(/[ \t]+$/gm, '');
}

// Kürzt auf maximal (size - 1) Bytes UTF-8, damit es in den C-Puffer passt
function clip(s, size) {
  s = (s === undefined || s === null) ? '' : String(s);
  s = keepDrawable(s);
  var max = size - 1;
  if (utf8len(s) <= max) return s;
  while (s.length && utf8len(s) + 3 > max) s = s.slice(0, -1);
  return s.replace(/\s+$/, '') + '…';
}

function pad2(n) { return (n < 10 ? '0' : '') + n; }

function fmtNum(n) {
  n = Math.round(parseFloat(n) || 0);
  return String(n).replace(/\B(?=(\d{3})+(?!\d))/g, L().thousands);
}

function startOfDay(d) { return new Date(d.getFullYear(), d.getMonth(), d.getDate()).getTime(); }

function fmtShortDate(d) {
  var t = L();
  if (t.months) return t.months[d.getMonth()] + ' ' + d.getDate();
  return pad2(d.getDate()) + '.' + pad2(d.getMonth() + 1) + '.';
}

// Unix-Zeit (Sekunden) -> "heute 14:05", "gestern 21:30", "Do 14:05", "28.09." bzw. "Sep 28"
function fmtWhen(ts) {
  ts = parseInt(ts, 10);
  if (!ts) return '';
  var t = L();
  var d = new Date(ts * 1000);
  var days = Math.round((startOfDay(new Date()) - startOfDay(d)) / 86400000);
  var time = pad2(d.getHours()) + ':' + pad2(d.getMinutes());
  if (days <= 0) return t.today + ' ' + time;
  if (days === 1) return t.yesterday + ' ' + time;
  if (days < 7) return t.weekdays[d.getDay()] + ' ' + time;
  return fmtShortDate(d);
}

// Sekunden -> "1 Std 5 Min" / "1 h 5 min"
function fmtDur(sec) {
  var t = L();
  sec = parseInt(sec, 10) || 0;
  var h = Math.floor(sec / 3600);
  var m = Math.floor((sec % 3600) / 60);
  if (h > 0) return h + ' ' + t.hours + ' ' + m + ' ' + t.minutes;
  if (m > 0) return m + ' ' + t.minutes;
  return sec > 0 ? t.lessMinute : '0 ' + t.minutes;
}

// Sekunden -> "25 Std" oder "40 Min" (kurz, für Listen)
function fmtDurShort(sec) {
  var t = L();
  sec = parseInt(sec, 10) || 0;
  if (sec >= 3600) return fmtNum(Math.round(sec / 3600)) + ' ' + t.hours;
  return Math.round(sec / 60) + ' ' + t.minutes;
}

// Millisekunden -> "1:42:05" bzw. "23:10"
function fmtClock(ms) {
  var s = Math.floor((parseInt(ms, 10) || 0) / 1000);
  var h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60);
  return (h > 0 ? h + ':' + pad2(m) : m) + ':' + pad2(s % 60);
}

function epCode(season, episode) {
  var s = parseInt(season, 10), e = parseInt(episode, 10);
  if (isNaN(s) || isNaN(e)) return '';
  return 'S' + pad2(s) + 'E' + pad2(e);
}

function fmtMbit(kbps) {
  var v = (parseInt(kbps, 10) || 0) / 1000;
  return v.toFixed(1).replace('.', L().decimal) + ' Mbit/s';
}

function decision(d) {
  d = String(d || '').toLowerCase();
  if (d === 'direct play') return 'Direct Play';
  if (d === 'copy') return 'Direct Stream';
  if (d === 'transcode') return 'Transcode';
  return d;
}

function stateText(st) {
  var t = L();
  if (st === 'paused') return t.paused;
  if (st === 'buffering') return t.buffering;
  return t.playing;
}

// Haupttitel eines Mediums (Serie statt Folge)
function mainTitle(x) {
  if (x.media_type === 'episode') return x.grandparent_title || x.title;
  return x.title || x.full_title || '';
}

// Zweite Zeile der Detailansicht: "S06E01 · Folgentitel"
function episodeLine(x) {
  if (x.media_type === 'episode') {
    var code = epCode(x.parent_media_index, x.media_index);
    return (code ? code + ' · ' : '') + (x.title || '');
  }
  if (x.media_type === 'track') return [x.grandparent_title, x.parent_title].filter(Boolean).join(' · ');
  if (x.media_type === 'movie' && x.year) return L().film + ' · ' + x.year;
  return '';
}

function deviceName(x) {
  var device = x.player || x.platform || '';
  if (x.product && x.product !== device) device += ' (' + x.product + ')';
  return device;
}

function lines(arr) {
  return arr.filter(function (l) { return l !== null && l !== undefined && String(l).length > 0; }).join('\n');
}

/* ------------------------------------------------------------------ */
/* Senden an die Uhr (Warteschlange, eine Nachricht nach der anderen)  */
/* ------------------------------------------------------------------ */

var queue = [];
var sending = false;

function send(msg) {
  queue.push({ msg: msg, tries: 0 });
  pump();
}

function pump() {
  if (sending || !queue.length) return;
  sending = true;
  var entry = queue[0];
  Pebble.sendAppMessage(entry.msg, function () {
    queue.shift();
    sending = false;
    pump();
  }, function () {
    entry.tries++;
    if (entry.tries >= 3) queue.shift();
    sending = false;
    setTimeout(pump, 250);
  });
}

// Liste komplett an die Uhr schicken
// items: [{section, kind, title, subtitle, detail, key, progress, confirm}]
function sendList(listId, opts, items) {
  items = items.slice(0, MAX_ITEMS);
  send({
    Type: T_LIST_BEGIN,
    ListId: listId,
    Lang: langCode(),
    Total: items.length,
    Sections: (opts.sections || []).map(function (s) { return clip(s.replace(/\|/g, '/'), 28); }).join('|'),
    Header: clip(opts.header || '', 48),
    HeaderState: opts.headerState || 0
  });
  items.forEach(function (it, i) {
    var msg = {
      Type: T_ITEM,
      ListId: listId,
      Index: i,
      Section: it.section || 0,
      Kind: it.kind || (it.detail ? KIND_DETAIL : KIND_NONE),
      Title: clip(it.title, 40),
      Subtitle: clip(it.subtitle, 56),
      Detail: clip(it.detail, 700),
      Key: clip(it.key, 40),
      Progress: (typeof it.progress === 'number' && !isNaN(it.progress)) ? it.progress : -1
    };
    if (it.confirm) msg.Text = clip(it.confirm, 96);
    send(msg);
  });
  send({ Type: T_LIST_END, ListId: listId });
}

function sendError(listId, text, header, headerState) {
  var msg = { Type: T_ERROR, ListId: listId, Lang: langCode(), Text: clip(text, 96) };
  if (header) { msg.Header = clip(header, 48); msg.HeaderState = headerState || 0; }
  send(msg);
}

function emptyItem(title, subtitle) {
  return { section: 0, kind: KIND_NONE, title: title, subtitle: subtitle || '', progress: -1 };
}

/* ------------------------------------------------------------------ */
/* Tautulli-API                                                        */
/* ------------------------------------------------------------------ */

function api(cmd, params, cb, method) {
  var t = L();
  if (isDemo()) {
    // Beispieldaten leicht verzögert liefern, wie ein echter Server
    var res = demo.request(cmd, params || {}, lang());
    setTimeout(function () { cb(res.error || null, res.data); }, 150);
    return;
  }
  var s = settings();
  if (!s.CfgUrl) return cb(t.noUrl);
  if (!s.CfgApiKey) return cb(t.noApiKey);
  var base = s.CfgUrl.replace(/\/+$/, '');
  if (!/^https?:\/\//i.test(base)) base = 'http://' + base;
  var q = 'apikey=' + encodeURIComponent(s.CfgApiKey) + '&cmd=' + encodeURIComponent(cmd);
  for (var k in params) {
    if (params[k] !== undefined && params[k] !== null) q += '&' + k + '=' + encodeURIComponent(params[k]);
  }
  var done = false;
  function finish(err, data) { if (!done) { done = true; cb(err, data); } }

  var xhr = new XMLHttpRequest();
  xhr.open(method || 'GET', base + '/api/v2?' + q, true);
  xhr.timeout = 12000;
  xhr.onload = function () {
    var j;
    try { j = JSON.parse(xhr.responseText); } catch (e) {
      return finish(xhr.status === 200 ? t.badResponse : t.httpError + ' ' + xhr.status);
    }
    var r = j && j.response;
    if (r && r.result === 'success') return finish(null, r.data);
    var m = (r && r.message) || (t.httpError + ' ' + xhr.status);
    if (/apikey/i.test(m)) m = t.badKey;
    finish(m);
  };
  xhr.onerror = function () { finish(t.unreachable); };
  xhr.ontimeout = function () { finish(t.timeout); };
  xhr.send();
}

// Mehrere API-Aufrufe parallel; cb(results) mit {err, data} je Aufruf
function parallel(calls, cb) {
  var results = new Array(calls.length);
  var left = calls.length;
  calls.forEach(function (c, i) {
    api(c[0], c[1] || {}, function (err, data) {
      results[i] = { err: err, data: data };
      if (--left === 0) cb(results);
    });
  });
}

/* ------------------------------------------------------------------ */
/* Startbildschirm: Server-Status und aktuelle Streams                 */
/* ------------------------------------------------------------------ */

function streamItem(x) {
  var t = L();
  var playing = x.state !== 'paused';
  var progress = parseInt(x.progress_percent, 10);
  if (isNaN(progress)) progress = 0;
  var who = x.friendly_name || x.user || '';
  var what = x.media_type === 'episode' ? epCode(x.parent_media_index, x.media_index)
           : x.media_type === 'track' ? (x.grandparent_title || '')
           : (x.year || '');
  var sub = [who, what, playing ? progress + ' %' : t.pause].filter(Boolean).join(' · ');
  var quality = [x.stream_video_full_resolution || x.video_full_resolution, decision(x.transcode_decision)]
    .filter(Boolean).join(' · ');

  var detail = lines([
    episodeLine(x),
    t.user + ': ' + who,
    t.device + ': ' + deviceName(x),
    t.state + ': ' + stateText(x.state),
    t.time + ': ' + fmtClock(x.view_offset) + ' / ' + fmtClock(x.duration) + ' (' + progress + ' %)',
    quality ? t.quality + ': ' + quality : '',
    x.bandwidth ? t.bandwidth + ': ' + fmtMbit(x.bandwidth) : '',
    x.location ? t.network + ': ' + String(x.location).toUpperCase() : ''
  ]);

  return {
    section: 0,
    kind: x.session_id ? KIND_STREAM : KIND_DETAIL,
    title: mainTitle(x),
    subtitle: sub,
    detail: detail,
    key: x.session_id || '',
    progress: progress,
    confirm: fill(t.stopQuestion, { user: who }) + '\n\n' + mainTitle(x)
  };
}

function loadHome() {
  // Noch nicht eingerichtet: freundlicher Hinweis statt roter Fehlermeldung
  var s = settings();
  if (!isDemo() && (!s.CfgUrl || !s.CfgApiKey)) {
    var t0 = L();
    return sendError(LIST_HOME, s.CfgUrl ? t0.noApiKey : t0.noUrl, t0.setupNeeded, 0);
  }
  parallel([['server_status'], ['get_activity']], function (r) {
    var t = L();
    var status = r[0], act = r[1];
    if (act.err) return sendError(LIST_HOME, act.err, t.tautulliDown, 2);

    var connected = !status.err && status.data && status.data.connected === true;
    var sessions = ((act.data && act.data.sessions) || []).slice();
    sessions.sort(function (a, b) {
      return (a.state === 'paused' ? 1 : 0) - (b.state === 'paused' ? 1 : 0);
    });

    var header;
    if (!connected) header = t.plexDown;
    else if (!sessions.length) header = t.plexOnline;
    else header = sessions.length + ' ' + (sessions.length === 1 ? t.streamOne : t.streamMany) + ' · ' +
                  fmtMbit(act.data.total_bandwidth);

    var items = sessions.map(streamItem);
    if (!items.length) items.push(emptyItem(t.nothingPlaying, connected ? t.plexIsOnline : t.tautulliNoPlex));
    sendList(LIST_HOME, { sections: [t.nowPlaying], header: header, headerState: connected ? 1 : 2 }, items);
  });
}

/* ------------------------------------------------------------------ */
/* Verlauf (alle oder ein Nutzer)                                      */
/* ------------------------------------------------------------------ */

function historyItem(x, showUser) {
  var t = L();
  var who = x.friendly_name || x.user || '';
  var pct = parseInt(x.percent_complete, 10);
  var sub = [showUser ? who : '', fmtWhen(x.date || x.started), isNaN(pct) ? '' : pct + ' %']
    .filter(Boolean).join(' · ');
  var watched = parseFloat(x.watched_status) >= 1 ? t.watchedAll : (isNaN(pct) ? '' : pct + ' ' + t.watchedPct);
  var device = deviceName(x);
  var detail = lines([
    episodeLine(x),
    t.user + ': ' + who,
    t.start + ': ' + fmtWhen(x.started),
    x.stopped ? t.end + ': ' + fmtWhen(x.stopped) : t.stillPlaying,
    t.duration + ': ' + fmtDur(x.play_duration !== undefined ? x.play_duration : x.duration),
    watched,
    device ? t.device + ': ' + device : '',
    x.transcode_decision ? t.playback + ': ' + decision(x.transcode_decision) : ''
  ]);
  return { section: 0, kind: KIND_DETAIL, title: mainTitle(x), subtitle: sub, detail: detail, progress: -1 };
}

function loadHistory(listId, userId) {
  var params = { length: settings().CfgCount, order_column: 'date', order_dir: 'desc' };
  if (userId) params.user_id = userId;
  api('get_history', params, function (err, data) {
    var t = L();
    if (err) return sendError(listId, err);
    var rows = (data && data.data) || [];
    var items = rows.map(function (x) { return historyItem(x, !userId); });
    var header = t.history;
    if (userId && rows.length) header = (rows[0].friendly_name || rows[0].user || '') + ' · ' + t.history;
    if (!items.length) items.push(emptyItem(t.noHistory));
    sendList(listId, { header: header }, items);
  });
}

/* ------------------------------------------------------------------ */
/* Neu hinzugefügt                                                     */
/* ------------------------------------------------------------------ */

function loadRecent() {
  api('get_recently_added', { count: settings().CfgCount }, function (err, data) {
    var t = L();
    if (err) return sendError(LIST_RECENT, err);
    var rows = (data && data.recently_added) || [];
    var items = rows.map(function (x) {
      var title, what;
      switch (x.media_type) {
        case 'episode':
          title = x.grandparent_title || x.title;
          what = epCode(x.parent_media_index, x.media_index) || t.episode;
          break;
        case 'season':
          title = x.parent_title || x.title;
          what = x.media_index ? t.season + ' ' + x.media_index : (x.title || t.season);
          break;
        case 'album':
          title = x.title;
          what = x.parent_title || t.types.album;
          break;
        case 'movie':
          title = x.title;
          what = t.film + (x.year ? ' ' + x.year : '');
          break;
        default:
          title = x.title;
          what = t.types[x.media_type] || '';
      }
      var genres = (x.genres || []).slice(0, 3).join(', ');
      var dur = parseInt(x.duration, 10);
      var detail = lines([
        x.media_type === 'episode' ? episodeLine(x) : (x.media_type === 'season' ? what : ''),
        t.type + ': ' + (t.types[x.media_type] || x.media_type || ''),
        x.library_name ? t.library + ': ' + x.library_name : '',
        t.added + ': ' + fmtWhen(x.added_at),
        x.year ? t.year + ': ' + x.year : '',
        genres ? t.genre + ': ' + genres : '',
        dur > 0 ? t.length + ': ' + fmtDur(dur / 1000) : '',
        x.summary ? '\n' + x.summary : ''
      ]);
      return {
        section: 0, kind: KIND_DETAIL, title: title,
        subtitle: [what, fmtWhen(x.added_at)].filter(Boolean).join(' · '),
        detail: detail, progress: -1
      };
    });
    if (!items.length) items.push(emptyItem(t.nothingNew));
    sendList(LIST_RECENT, { header: t.recent }, items);
  });
}

/* ------------------------------------------------------------------ */
/* Nutzer                                                              */
/* ------------------------------------------------------------------ */

function loadUsers() {
  api('get_users_table', { order_column: 'last_seen', order_dir: 'desc', length: 50 }, function (err, data) {
    var t = L();
    if (err) return sendError(LIST_USERS, err);
    var rows = ((data && data.data) || []).filter(function (u) { return !u.deleted_user; });
    // Wer schon geschaut hat zuerst (neueste oben), danach der Rest
    rows.sort(function (a, b) { return (parseInt(b.last_seen, 10) || 0) - (parseInt(a.last_seen, 10) || 0); });
    var items = rows.map(function (u) {
      var plays = parseInt(u.plays, 10) || 0;
      var sub = plays > 0 && u.last_seen
        ? t.lastSeen + ' ' + fmtWhen(u.last_seen) + ' · ' + fmtDurShort(u.duration)
        : t.never;
      return {
        section: 0,
        kind: plays > 0 ? KIND_USER : KIND_NONE,
        title: u.friendly_name || u.username || '?',
        subtitle: sub,
        key: String(u.user_id || ''),
        progress: -1
      };
    });
    if (!items.length) items.push(emptyItem(t.noUsers));
    sendList(LIST_USERS, { header: t.users }, items);
  });
}

/* ------------------------------------------------------------------ */
/* Bibliotheken                                                        */
/* ------------------------------------------------------------------ */

var TYPE_ORDER = { movie: 0, show: 1, artist: 2, photo: 3 };

function libraryCounts(x) {
  var t = L();
  var type = String(x.section_type || '').toLowerCase();
  var units = t.libUnits[type];
  if (!units) return fmtNum(x.count);
  if (type === 'show') return fmtNum(x.count) + ' ' + units[0] + ' · ' + fmtNum(x.child_count) + ' ' + units[1];
  if (type === 'artist') return fmtNum(x.count) + ' ' + units[0] + ' · ' + fmtNum(x.parent_count) + ' ' + units[1];
  if (type === 'photo') return fmtNum(x.child_count || x.count) + ' ' + units[0];
  return fmtNum(x.count) + ' ' + units[0];
}

function loadLibraries() {
  api('get_libraries_table', { order_column: 'section_name', order_dir: 'asc', length: 50 }, function (err, data) {
    var t = L();
    if (err) return sendError(LIST_LIBRARIES, err);
    var rows = ((data && data.data) || []).filter(function (x) { return x.is_active !== 0; });
    rows.sort(function (a, b) {
      var ta = TYPE_ORDER[String(a.section_type).toLowerCase()], tb = TYPE_ORDER[String(b.section_type).toLowerCase()];
      ta = ta === undefined ? 9 : ta; tb = tb === undefined ? 9 : tb;
      var na = String(a.section_name).toLowerCase(), nb = String(b.section_name).toLowerCase();
      return ta - tb || (na < nb ? -1 : na > nb ? 1 : 0);
    });
    var items = rows.map(function (x) {
      var counts = libraryCounts(x);
      var detail = lines([
        counts,
        t.plays + ': ' + fmtNum(x.plays),
        t.watchTime + ': ' + fmtDur(x.duration),
        x.last_played ? t.lastPlayed + ': ' + x.last_played + (x.last_accessed ? ' (' + fmtWhen(x.last_accessed) + ')' : '') : ''
      ]);
      return { section: 0, kind: KIND_DETAIL, title: x.section_name, subtitle: counts, detail: detail, progress: -1 };
    });
    if (!items.length) items.push(emptyItem(t.noLibraries));
    sendList(LIST_LIBRARIES, { header: t.libraries }, items);
  });
}

/* ------------------------------------------------------------------ */
/* Statistik                                                           */
/* ------------------------------------------------------------------ */

function todayStr() {
  var d = new Date();
  return d.getFullYear() + '-' + pad2(d.getMonth() + 1) + '-' + pad2(d.getDate());
}

function statRows(res) {
  if (!res || res.err || !res.data) return [];
  var d = res.data;
  if (Object.prototype.toString.call(d) === '[object Array]') d = d[0] || {};
  return d.rows || [];
}

function sumRows(rows) {
  var dur = 0, plays = 0;
  rows.forEach(function (r) {
    dur += parseInt(r.total_duration, 10) || 0;
    plays += parseInt(r.total_plays, 10) || 0;
  });
  return { dur: dur, plays: plays };
}

function loadStats() {
  function users(extra) {
    var o = { stat_id: 'top_users', stats_type: 'duration', stats_count: 50 };
    for (var k in extra) o[k] = extra[k];
    return o;
  }
  parallel([
    ['get_home_stats', users({ time_range: 1, after: todayStr() })],
    ['get_home_stats', users({ time_range: 7 })],
    ['get_home_stats', users({ time_range: 30 })],
    ['get_home_stats', { stat_id: 'top_movies', stats_type: 'plays', stats_count: 5, time_range: 30 }],
    ['get_home_stats', { stat_id: 'top_tv', stats_type: 'plays', stats_count: 5, time_range: 30 }]
  ], function (r) {
    var t = L();
    // Ohne Verbindung haben alle Aufrufe denselben Fehler
    if (r[0].err && r[1].err && r[2].err) return sendError(LIST_STATS, r[2].err);

    var items = [];
    [[t.statToday, r[0]], [t.stat7, r[1]], [t.stat30, r[2]]].forEach(function (p) {
      var sum = sumRows(statRows(p[1]));
      items.push({
        section: 0, kind: KIND_NONE, title: p[0],
        subtitle: p[1].err ? t.error : fmtDur(sum.dur) + ' · ' + sum.plays + 'x', progress: -1
      });
    });

    var top = statRows(r[2]).slice(0, 5);
    top.forEach(function (u) {
      items.push({
        section: 1, kind: KIND_NONE, title: u.friendly_name || u.user || '?',
        subtitle: fmtDur(u.total_duration) + ' · ' + (parseInt(u.total_plays, 10) || 0) + 'x', progress: -1
      });
    });
    if (!top.length) items.push({ section: 1, kind: KIND_NONE, title: t.noData, progress: -1 });

    [[2, r[3]], [3, r[4]]].forEach(function (p) {
      var rows = statRows(p[1]).slice(0, 5);
      rows.forEach(function (m) {
        items.push({
          section: p[0], kind: KIND_NONE, title: m.title || m.grandparent_title || '?',
          subtitle: (parseInt(m.total_plays, 10) || 0) + 'x · ' + fmtDur(m.total_duration), progress: -1
        });
      });
      if (!rows.length) items.push({ section: p[0], kind: KIND_NONE, title: t.noData, progress: -1 });
    });

    sendList(LIST_STATS, {
      sections: [t.secWatchTime, t.secTopUsers, t.secTopMovies, t.secTopShows],
      header: t.stats
    }, items);
  });
}

/* ------------------------------------------------------------------ */
/* Diagramm: Wiedergaben pro Tag                                       */
/* ------------------------------------------------------------------ */

function chartLabel(dateStr, days) {
  var p = String(dateStr).split('-');
  var d = new Date(parseInt(p[0], 10), parseInt(p[1], 10) - 1, parseInt(p[2], 10));
  var t = L();
  if (days <= 10) return t.weekdays[d.getDay()];
  if (t.months) return (d.getMonth() + 1) + '/' + d.getDate();
  return d.getDate() + '.' + (d.getMonth() + 1) + '.';
}

function loadChart(daysParam) {
  var days = parseInt(daysParam, 10) === 30 ? 30 : 7;
  api('get_plays_by_date', { time_range: days, y_axis: 'plays' }, function (err, data) {
    var t = L();
    if (err) return sendError(LIST_CHART, err);
    var cats = (data && data.categories) || [];
    var series = (data && data.series) || [];
    function find(name) {
      for (var i = 0; i < series.length; i++) if (series[i].name === name) return series[i].data || [];
      return [];
    }
    var tv = find('TV'), movies = find('Movies'), music = find('Music'), live = find('Live TV');
    var start = Math.max(0, cats.length - days);
    var bytes = [], labels = [], total = 0;
    for (var i = start; i < cats.length; i++) {
      var v = [parseInt(tv[i], 10) || 0, parseInt(movies[i], 10) || 0,
               (parseInt(music[i], 10) || 0) + (parseInt(live[i], 10) || 0)];
      v.forEach(function (n) {
        n = Math.min(65535, Math.max(0, n));
        total += n;
        bytes.push(n & 0xff, (n >> 8) & 0xff);
      });
      labels.push(chartLabel(cats[i], days));
    }
    send({
      Type: T_CHART,
      ListId: LIST_CHART,
      Lang: langCode(),
      Total: labels.length,
      Data: bytes,
      Labels: labels.join('|'),
      Sections: t.chartLegend,
      Header: clip(fill(t.chartHeader, { days: days, plays: fmtNum(total) }), 48)
    });
  });
}

/* ------------------------------------------------------------------ */
/* Stream beenden                                                      */
/* ------------------------------------------------------------------ */

function terminate(sessionId) {
  api('terminate_session', { session_id: sessionId, message: L().stopMessage }, function (err) {
    var t = L();
    if (err) {
      var text = /plex ?pass/i.test(err) ? t.needsPlexPass : err;
      send({ Type: T_RESULT, Lang: langCode(), Ok: 0, Text: clip(t.error + ': ' + text, 96) });
    } else {
      send({ Type: T_RESULT, Lang: langCode(), Ok: 1, Text: t.stopped });
    }
  }, 'POST');
}

/* ------------------------------------------------------------------ */
/* Ereignisse                                                          */
/* ------------------------------------------------------------------ */

function load(listId, key) {
  switch (listId) {
    case LIST_HOME: return loadHome();
    case LIST_HISTORY: return loadHistory(LIST_HISTORY, null);
    case LIST_RECENT: return loadRecent();
    case LIST_STATS: return loadStats();
    case LIST_USERS: return loadUsers();
    case LIST_LIBRARIES: return loadLibraries();
    case LIST_USER_HISTORY: return loadHistory(LIST_USER_HISTORY, key);
    case LIST_CHART: return loadChart(key);
  }
}

Pebble.addEventListener('ready', function () {
  loadHome();
});

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload || {};
  if (p.Command === CMD_LOAD) load(p.ListId, p.Key);
  else if (p.Command === CMD_TERMINATE && p.Key) terminate(p.Key);
});

// Die Einstellungsseite wird in der aktuellen Sprache erzeugt
var clay = null;

Pebble.addEventListener('showConfiguration', function () {
  clay = new Clay(clayConfig(lang()), null, { autoHandleEvents: false });
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response || !clay) return;
  clay.getSettings(e.response, false); // speichert in localStorage
  loadHome();
});
