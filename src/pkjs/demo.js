/*
 * Tautulli für Pebble – Demo-Modus
 *
 * Steht in den Einstellungen als Adresse "demo", beantwortet diese Datei
 * alle API-Aufrufe mit erfundenen Beispieldaten im Format der Tautulli-API.
 * Alle Namen und Titel sind ausgedacht. Gedacht für Store-Screenshots und
 * zum Ausprobieren ohne eigenen Server.
 */

var USERS = [
  { user_id: 101, friendly_name: 'Emma' },
  { user_id: 102, friendly_name: 'Noah' },
  { user_id: 103, friendly_name: 'Lena' },
  { user_id: 104, friendly_name: 'Max' }
];

// Titel: [Typ, Serie/Film, Folgentitel, Staffel, Folge, Jahr, Länge in Minuten]
var MEDIA = [
  ['episode', 'Harbor Patrol', 'The Lost Buoy', 2, 5, 2024, 24],
  ['movie', 'The Clockmaker\'s Secret', '', 0, 0, 2024, 104],
  ['episode', 'Cosmo & Pixel', 'Moon Picnic', 1, 12, 2023, 11],
  ['movie', 'Northern Lights', '', 0, 0, 2023, 118],
  ['episode', 'Kitchen Heroes', 'Pasta Night', 3, 2, 2025, 45],
  ['movie', 'Robot Garden', '', 0, 0, 2025, 92],
  ['episode', 'Harbor Patrol', 'Storm Warning', 2, 4, 2024, 24]
];

var PLAYERS = [
  ['Living Room TV', 'Plex for Android (TV)', 'Android'],
  ['iPad', 'Plex for iOS', 'iOS'],
  ['Kitchen Speaker', 'Plexamp', 'Android'],
  ['Laptop', 'Plex Web', 'Chrome']
];

// Laufende Streams; beendete werden bis zum nächsten App-Start entfernt
var sessions = null;

function now() { return Math.floor(Date.now() / 1000); }

function mediaFields(m) {
  var o = { media_type: m[0], year: String(m[5]), duration: String(m[6] * 60000) };
  if (m[0] === 'episode') {
    o.grandparent_title = m[1];
    o.title = m[2];
    o.parent_media_index = String(m[3]);
    o.media_index = String(m[4]);
    o.full_title = m[1] + ' - ' + m[2];
  } else {
    o.title = m[1];
    o.full_title = m[1];
  }
  return o;
}

function extend(a, b) {
  for (var k in b) a[k] = b[k];
  return a;
}

function getSessions() {
  if (!sessions) {
    sessions = [
      extend(mediaFields(MEDIA[0]), {
        session_id: 'demo-1', session_key: '1', state: 'playing', friendly_name: 'Emma', user: 'emma',
        player: PLAYERS[0][0], product: PLAYERS[0][1], platform: PLAYERS[0][2],
        progress_percent: '42', view_offset: String(Math.round(24 * 60000 * 0.42)),
        stream_video_full_resolution: '1080p', transcode_decision: 'direct play', bandwidth: '8200', location: 'lan'
      }),
      extend(mediaFields(MEDIA[1]), {
        session_id: 'demo-2', session_key: '2', state: 'paused', friendly_name: 'Noah', user: 'noah',
        player: PLAYERS[1][0], product: PLAYERS[1][1], platform: PLAYERS[1][2],
        progress_percent: '76', view_offset: String(Math.round(104 * 60000 * 0.76)),
        stream_video_full_resolution: '720p', transcode_decision: 'transcode', bandwidth: '4300', location: 'wan'
      })
    ];
  }
  return sessions;
}

function activity() {
  var s = getSessions();
  var bw = 0;
  s.forEach(function (x) { bw += parseInt(x.bandwidth, 10) || 0; });
  return { stream_count: String(s.length), total_bandwidth: bw, sessions: s };
}

// Feste, aber natürlich wirkende Abfolge von Wiedergaben der letzten Tage
function historyRows() {
  var rows = [];
  var t = now() - 1800;
  for (var i = 0; i < 40; i++) {
    var m = MEDIA[(i * 3) % MEDIA.length];
    var u = USERS[(i * 5 + 1) % USERS.length];
    var p = PLAYERS[i % PLAYERS.length];
    var len = m[6] * 60;
    var pct = (i % 4 === 1) ? 35 + (i * 7) % 50 : 92 + (i % 8);
    var play = Math.round(len * pct / 100);
    rows.push(extend(mediaFields(m), {
      date: t - play, started: t - play, stopped: t,
      friendly_name: u.friendly_name, user: u.friendly_name.toLowerCase(), user_id: u.user_id,
      player: p[0], product: p[1], platform: p[2],
      percent_complete: Math.min(pct, 100), play_duration: play, watched_status: pct >= 90 ? 1 : 0,
      transcode_decision: i % 3 === 0 ? 'transcode' : 'direct play'
    }));
    t -= 3600 * (3 + (i * 7) % 11);
  }
  return rows;
}

function history(params) {
  var rows = historyRows();
  if (params.user_id) rows = rows.filter(function (r) { return String(r.user_id) === String(params.user_id); });
  var n = parseInt(params.length, 10) || 25;
  return { recordsFiltered: rows.length, data: rows.slice(0, n) };
}

function recentlyAdded(params, lang) {
  var de = lang === 'de';
  var t = now();
  var items = [
    extend(mediaFields(MEDIA[5]), { added_at: String(t - 5400), library_name: de ? 'Filme' : 'Movies',
      genres: ['Animation', 'Sci-Fi'], summary: de
        ? 'Ein kleiner Roboter pflegt einen Garten auf einer verlassenen Raumstation.'
        : 'A small robot tends a garden on an abandoned space station.' }),
    extend(mediaFields(MEDIA[2]), { added_at: String(t - 86400 - 3000), library_name: de ? 'Kinder' : 'Kids' }),
    { media_type: 'season', parent_title: 'Kitchen Heroes', title: (de ? 'Staffel' : 'Season') + ' 3', media_index: '3',
      added_at: String(t - 3 * 86400), library_name: de ? 'Serien' : 'TV Shows' },
    extend(mediaFields(MEDIA[3]), { added_at: String(t - 4 * 86400), library_name: de ? 'Filme' : 'Movies',
      genres: ['Drama'] }),
    { media_type: 'album', title: 'Morning Songs', parent_title: 'The Sunny Band',
      added_at: String(t - 9 * 86400), library_name: de ? 'Musik' : 'Music' }
  ];
  return { recently_added: items.slice(0, parseInt(params.count, 10) || 10) };
}

function homeStats(params) {
  var range = parseInt(params.time_range, 10) || 30;
  var f = range === 1 ? 0.04 : range === 7 ? 0.27 : 1;
  if (params.stat_id === 'top_users') {
    var base = [[101, 'Emma', 81000, 64], [102, 'Noah', 52000, 38], [103, 'Lena', 30500, 17], [104, 'Max', 12600, 6]];
    return { stat_id: 'top_users', stat_type: 'total_duration', rows: base.map(function (b) {
      return { user_id: b[0], friendly_name: b[1], total_duration: Math.round(b[2] * f), total_plays: Math.round(b[3] * f) };
    }).filter(function (r) { return r.total_plays > 0; }) };
  }
  if (params.stat_id === 'top_movies') {
    return { stat_id: 'top_movies', rows: [
      { title: 'Robot Garden', total_plays: 9, total_duration: 41000 },
      { title: 'The Clockmaker\'s Secret', total_plays: 4, total_duration: 22000 },
      { title: 'Northern Lights', total_plays: 2, total_duration: 13400 }
    ] };
  }
  if (params.stat_id === 'top_tv') {
    return { stat_id: 'top_tv', rows: [
      { title: 'Harbor Patrol', grandparent_title: 'Harbor Patrol', total_plays: 31, total_duration: 44000 },
      { title: 'Cosmo & Pixel', grandparent_title: 'Cosmo & Pixel', total_plays: 22, total_duration: 14500 },
      { title: 'Kitchen Heroes', grandparent_title: 'Kitchen Heroes', total_plays: 8, total_duration: 21000 }
    ] };
  }
  return { rows: [] };
}

function usersTable() {
  var t = now();
  var seen = [t - 1800, t - 7200, t - 2 * 86400, t - 6 * 86400];
  var dur = [310000, 205000, 98000, 41000];
  var plays = [212, 140, 61, 23];
  var rows = USERS.map(function (u, i) {
    return { user_id: u.user_id, friendly_name: u.friendly_name, username: u.friendly_name.toLowerCase(),
             last_seen: seen[i], duration: dur[i], plays: plays[i], deleted_user: 0 };
  });
  rows.push({ user_id: 105, friendly_name: 'Guest', username: 'guest', last_seen: null, duration: 0, plays: 0, deleted_user: 0 });
  return { recordsFiltered: rows.length, data: rows };
}

function librariesTable(lang) {
  var de = lang === 'de';
  var t = now();
  return { data: [
    { section_name: de ? 'Filme' : 'Movies', section_type: 'movie', count: 245, parent_count: 0, child_count: 0,
      plays: 512, duration: 2950000, last_played: 'Robot Garden', last_accessed: t - 7200, is_active: 1 },
    { section_name: de ? 'Serien' : 'TV Shows', section_type: 'show', count: 38, parent_count: 156, child_count: 2104,
      plays: 1480, duration: 3720000, last_played: 'Harbor Patrol - The Lost Buoy', last_accessed: t - 1800, is_active: 1 },
    { section_name: de ? 'Kinder' : 'Kids', section_type: 'show', count: 21, parent_count: 60, child_count: 890,
      plays: 2210, duration: 1530000, last_played: 'Cosmo & Pixel - Moon Picnic', last_accessed: t - 86400, is_active: 1 },
    { section_name: de ? 'Musik' : 'Music', section_type: 'artist', count: 120, parent_count: 310, child_count: 4200,
      plays: 860, duration: 205000, last_played: 'The Sunny Band - Morning Songs', last_accessed: t - 3 * 86400, is_active: 1 }
  ] };
}

function playsByDate(params) {
  var days = parseInt(params.time_range, 10) || 30;
  var cats = [], tv = [], movies = [], music = [], live = [];
  var today = new Date();
  for (var i = days - 1; i >= 0; i--) {
    var d = new Date(today.getFullYear(), today.getMonth(), today.getDate() - i);
    var m = d.getMonth() + 1, day = d.getDate();
    cats.push(d.getFullYear() + '-' + (m < 10 ? '0' : '') + m + '-' + (day < 10 ? '0' : '') + day);
    var wd = d.getDay();
    var weekend = wd === 0 || wd === 6;
    var seed = (d.getDate() * 7 + d.getMonth() * 3) % 5;
    tv.push((weekend ? 6 : 3) + seed);
    movies.push(weekend ? 2 + (seed % 2) : (seed % 3 === 0 ? 1 : 0));
    music.push(seed % 2);
    live.push(0);
  }
  return { categories: cats, series: [
    { name: 'TV', data: tv }, { name: 'Movies', data: movies }, { name: 'Music', data: music }, { name: 'Live TV', data: live }
  ] };
}

function terminate(params) {
  var s = getSessions();
  var before = s.length;
  sessions = s.filter(function (x) { return x.session_id !== params.session_id; });
  return before === sessions.length ? { error: 'Invalid session_id' } : { data: null };
}

exports.request = function (cmd, params, lang) {
  switch (cmd) {
    case 'server_status': return { data: { result: 'success', connected: true } };
    case 'get_activity': return { data: activity() };
    case 'get_history': return { data: history(params) };
    case 'get_recently_added': return { data: recentlyAdded(params, lang) };
    case 'get_home_stats': return { data: homeStats(params) };
    case 'get_users_table': return { data: usersTable() };
    case 'get_libraries_table': return { data: librariesTable(lang) };
    case 'get_plays_by_date': return { data: playsByDate(params) };
    case 'terminate_session': return terminate(params);
  }
  return { error: 'Unknown command' };
};
