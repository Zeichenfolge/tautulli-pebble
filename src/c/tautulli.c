/*
 * Tautulli für Pebble
 * Zeigt aktuelle Plex-Streams, Verlauf, neu hinzugefügte Medien, Nutzer,
 * Bibliotheken, Statistiken und ein Diagramm eines Tautulli-Servers an und
 * kann laufende Streams beenden.
 *
 * Datenfluss: Die Uhr spricht nie direkt mit dem Server. Das PebbleKit-JS
 * auf dem Handy (src/pkjs/index.js) ruft die Tautulli-API auf, bereitet
 * alle Texte fertig (in der eingestellten Sprache) auf und schickt sie per
 * AppMessage hierher.
 *
 * Protokoll Handy -> Uhr (Schlüssel "Type"):
 *   1 ListBegin  ListId, Total, Sections ("A|B"), Header, HeaderState, Lang
 *   2 Item       ListId, Index, Section, Kind, Title, Subtitle, Detail, Key,
 *                Progress, Text (Rückfrage beim Beenden)
 *   3 ListEnd    ListId
 *   4 Error      ListId, Text, Lang, [Header, HeaderState]
 *   5 Result     Ok, Text   (Antwort auf "Stream beenden")
 *   6 Chart      Total, Data, Labels, Sections, Header, Lang
 *   7 Notify     Text  (z. B. „Emma hat … zu Ende geschaut“: Uhr vibriert)
 * Uhr -> Handy (Schlüssel "Command"):
 *   1 Laden      ListId, [Key]  (Key = "user_id|Name" bzw. "Tage[|user_id|Name]")
 *   2 Beenden    Key (session_id)
 */
#include <pebble.h>
#include "i18n.h"
#include "chart.h"

#define LIST_HOME         1
#define LIST_HISTORY      2
#define LIST_RECENT       3
#define LIST_STATS        4
#define LIST_USERS        5
#define LIST_LIBRARIES    6
#define LIST_USER_HISTORY 7
#define LIST_CHART        8

#define T_LIST_BEGIN 1
#define T_ITEM       2
#define T_LIST_END   3
#define T_ERROR      4
#define T_RESULT     5
#define T_CHART      6
#define T_NOTIFY     7

#define KIND_NONE   0   // nicht auswählbar
#define KIND_DETAIL 1   // öffnet Detailansicht
#define KIND_STREAM 2   // Detailansicht mit „Stream beenden“
#define KIND_USER   3   // öffnet die Ansicht dieser Person
#define KIND_CHART  4   // öffnet das Diagramm (Key = Person)

#define CMD_LOAD      1
#define CMD_TERMINATE 2

#define MAX_VIEWS    4
#define MAX_ITEMS    30
#define MAX_SECTIONS 5
#define HEADER_H     28
#define REFRESH_MS   30000
#define LOAD_TIMEOUT 20

#define COLOR_ACCENT GColorChromeYellow

typedef struct {
  uint8_t section;
  uint8_t kind;
  int8_t progress;          // 0..100, -1 = kein Fortschrittsbalken
  char title[40];
  char subtitle[56];
  char key[40];             // session_id bei Streams, "user_id|Name" bei Nutzern
  char *detail;             // Text der Detailansicht (malloc)
  char *confirm;            // Rückfrage beim Beenden (malloc, nur Streams)
} Item;

typedef struct {
  uint8_t list_id;
  char param[40];           // z. B. "user_id|Name" bei der Ansicht einer Person
  char title[40];           // Kopfzeile, bis Daten da sind
  Window *window;
  Layer *header_layer;
  MenuLayer *menu;

  char header[48];
  uint8_t header_state;     // 0 neutral, 1 gut (grün), 2 Problem (rot)
  char sections[MAX_SECTIONS][28];
  uint8_t num_sections;
  Item *items;
  uint8_t count;

  // Liste, die gerade vom Handy empfangen wird
  Item *pending;
  uint8_t pending_total;
  char pending_header[48];
  uint8_t pending_header_state;
  char pending_sections[MAX_SECTIONS][28];
  uint8_t pending_num_sections;

  char status[96];
  bool loading;
  time_t load_started;
  bool appeared_once;
} ListView;

typedef struct {
  Window *window;
  ScrollLayer *scroll;
  TextLayer *title_layer;
  Layer *bar_layer;
  TextLayer *body_layer;
  ActionBarLayer *action_bar;
  char *title;
  char *body;
  char *confirm;
  int8_t progress;
  char key[40];
} DetailView;

typedef struct {
  StrId title;
  StrId subtitle;
  uint8_t list_id;
} MoreEntry;

static const MoreEntry s_more[] = {
  { STR_MORE_HISTORY,   STR_MORE_HISTORY_SUB,   LIST_HISTORY },
  { STR_MORE_RECENT,    STR_MORE_RECENT_SUB,    LIST_RECENT },
  { STR_MORE_USERS,     STR_MORE_USERS_SUB,     LIST_USERS },
  { STR_MORE_STATS,     STR_MORE_STATS_SUB,     LIST_STATS },
  { STR_MORE_CHART,     STR_MORE_CHART_SUB,     LIST_CHART },
  { STR_MORE_LIBRARIES, STR_MORE_LIBRARIES_SUB, LIST_LIBRARIES },
};
#define MORE_COUNT ((int)ARRAY_LENGTH(s_more))

static ListView *s_views[MAX_VIEWS];
static ListView *s_home;
static DetailView *s_detail;
static AppTimer *s_refresh_timer;

static GBitmap *s_icon_stop;
static GBitmap *s_icon_check;

// Rückfrage „Stream beenden?“
static Window *s_confirm_window;
static TextLayer *s_confirm_text;
static ActionBarLayer *s_confirm_bar;
static char s_confirm_buf[96];
static char s_stop_key[40];

// Kurze Meldung
static Window *s_toast_window;
static TextLayer *s_toast_layer;
static char s_toast_text[96];
static AppTimer *s_toast_timer;

/* ------------------------------------------------------------------ */
/* Hilfsfunktionen                                                     */
/* ------------------------------------------------------------------ */

static void prv_copy(char *dst, size_t size, const char *src) {
  if (!src) { dst[0] = '\0'; return; }
  strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

static char *prv_strdup(const char *src) {
  if (!src) return NULL;
  size_t len = strlen(src);
  char *d = malloc(len + 1);
  if (d) memcpy(d, src, len + 1);
  return d;
}

static void prv_free_items(Item *items, uint8_t count) {
  if (!items) return;
  for (int i = 0; i < count; i++) {
    free(items[i].detail);
    free(items[i].confirm);
  }
  free(items);
}

// Zerlegt "A|B|C" in ein festes Array
static uint8_t prv_split(const char *src, char out[][28], uint8_t max_parts) {
  uint8_t n = 0;
  if (!src || !src[0]) return 0;
  const char *p = src;
  while (n < max_parts) {
    const char *end = strchr(p, '|');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= 28) len = 27;
    memcpy(out[n], p, len);
    out[n][len] = '\0';
    n++;
    if (!end) break;
    p = end + 1;
  }
  return n;
}

// String-Tupel nur übernehmen, wenn es wirklich Text enthält
static const char *prv_tuple_str(Tuple *t) {
  return (t && t->type == TUPLE_CSTRING && t->length > 1) ? t->value->cstring : NULL;
}

static ListView *prv_view_for(int list_id) {
  for (int i = 0; i < MAX_VIEWS; i++) {
    if (s_views[i] && s_views[i]->list_id == list_id) return s_views[i];
  }
  return NULL;
}

static bool prv_is_home(ListView *v) { return v == s_home; }

static void prv_reload(ListView *v) {
  if (v && v->menu) menu_layer_reload_data(v->menu);
  if (v && v->header_layer) layer_mark_dirty(v->header_layer);
}

// Sprache aus einer Nachricht übernehmen und bei Änderung alles neu zeichnen
static void prv_apply_lang(DictionaryIterator *it) {
  Tuple *t = dict_find(it, MESSAGE_KEY_Lang);
  if (!t) return;
  if (!i18n_set_language(t->value->int32 == LANG_EN ? LANG_EN : LANG_DE)) return;
  for (int i = 0; i < MAX_VIEWS; i++) prv_reload(s_views[i]);
  chart_refresh();
}

/* ------------------------------------------------------------------ */
/* Senden                                                              */
/* ------------------------------------------------------------------ */

static bool prv_send_command(int cmd, int list_id, const char *key) {
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return false;
  dict_write_int32(it, MESSAGE_KEY_Command, cmd);
  if (list_id) dict_write_int32(it, MESSAGE_KEY_ListId, list_id);
  if (key && key[0]) dict_write_cstring(it, MESSAGE_KEY_Key, key);
  return app_message_outbox_send() == APP_MSG_OK;
}

static void prv_send_load(ListView *v) {
  if (!v) return;
  if (!prv_send_command(CMD_LOAD, v->list_id, v->param)) {
    if (v->count == 0) prv_copy(v->status, sizeof(v->status), tr(STR_NO_PHONE));
    v->loading = false;
    prv_reload(v);
    return;
  }
  v->loading = true;
  v->load_started = time(NULL);
  if (v->count == 0) prv_copy(v->status, sizeof(v->status), tr(STR_LOADING));
  prv_reload(v);
}

static void prv_chart_request(int days, const char *key) {
  char buf[64];
  if (key && key[0]) snprintf(buf, sizeof(buf), "%d|%s", days, key);
  else snprintf(buf, sizeof(buf), "%d", days);
  if (!prv_send_command(CMD_LOAD, LIST_CHART, buf)) chart_handle_error(tr(STR_NO_PHONE));
}

/* ------------------------------------------------------------------ */
/* Toast (kurze Meldung)                                               */
/* ------------------------------------------------------------------ */

static void prv_toast_timer(void *ctx) {
  s_toast_timer = NULL;
  if (s_toast_window) window_stack_remove(s_toast_window, true);
}

// Text setzen und senkrecht mittig ausrichten
static void prv_toast_layout(void) {
  if (!s_toast_window || !s_toast_layer) return;
  GRect b = layer_get_bounds(window_get_root_layer(s_toast_window));
  layer_set_frame(text_layer_get_layer(s_toast_layer), GRect(8, 0, b.size.w - 16, b.size.h));
  text_layer_set_text(s_toast_layer, s_toast_text);
  GSize sz = text_layer_get_content_size(s_toast_layer);
  int16_t h = sz.h + 8 < b.size.h ? sz.h + 8 : b.size.h;
  layer_set_frame(text_layer_get_layer(s_toast_layer), GRect(8, (b.size.h - h) / 2, b.size.w - 16, h));
}

static void prv_toast_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  window_set_background_color(w, COLOR_ACCENT);
  // Text über die ganze Höhe umbrechen und senkrecht mittig setzen
  s_toast_layer = text_layer_create(GRect(8, 0, b.size.w - 16, b.size.h));
  text_layer_set_background_color(s_toast_layer, GColorClear);
  text_layer_set_text_color(s_toast_layer, GColorBlack);
  text_layer_set_font(s_toast_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_toast_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_toast_layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_toast_layer));
  prv_toast_layout();
}

static void prv_toast_unload(Window *w) {
  if (s_toast_timer) { app_timer_cancel(s_toast_timer); s_toast_timer = NULL; }
  text_layer_destroy(s_toast_layer);
  s_toast_layer = NULL;
  window_destroy(w);
  s_toast_window = NULL;
}

static void prv_toast_show_ms(const char *text, uint32_t ms) {
  prv_copy(s_toast_text, sizeof(s_toast_text), text);
  if (!s_toast_window) {
    s_toast_window = window_create();
    window_set_window_handlers(s_toast_window, (WindowHandlers) {
      .load = prv_toast_load, .unload = prv_toast_unload,
    });
    window_stack_push(s_toast_window, true);
  } else {
    prv_toast_layout();
  }
  if (s_toast_timer) { app_timer_cancel(s_toast_timer); s_toast_timer = NULL; }
  s_toast_timer = app_timer_register(ms, prv_toast_timer, NULL);
}

static void prv_toast_show(const char *text, bool auto_close) {
  // Ohne Antwort schließt sich die Meldung spätestens nach 15 Sekunden
  prv_toast_show_ms(text, auto_close ? 2000 : 15000);
}

/* ------------------------------------------------------------------ */
/* Rückfrage „Stream beenden?“                                         */
/* ------------------------------------------------------------------ */

static void prv_confirm_select(ClickRecognizerRef rec, void *ctx) {
  if (s_confirm_window) window_stack_remove(s_confirm_window, false);
  if (prv_send_command(CMD_TERMINATE, 0, s_stop_key)) {
    prv_toast_show(tr(STR_STOPPING), false);
  } else {
    vibes_double_pulse();
    prv_toast_show(tr(STR_NO_PHONE), true);
  }
}

static void prv_confirm_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_confirm_select);
}

static void prv_confirm_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  int16_t text_w = b.size.w - ACTION_BAR_WIDTH - 12;

  s_confirm_text = text_layer_create(GRect(6, 0, text_w, 2000));
  text_layer_set_font(s_confirm_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_overflow_mode(s_confirm_text, GTextOverflowModeWordWrap);
  text_layer_set_text(s_confirm_text, s_confirm_buf);
  GSize sz = text_layer_get_content_size(s_confirm_text);
  int16_t h = sz.h + 8;
  if (h > b.size.h) h = b.size.h;
  layer_set_frame(text_layer_get_layer(s_confirm_text), GRect(6, (b.size.h - h) / 2, text_w, h));
  layer_add_child(root, text_layer_get_layer(s_confirm_text));

  s_confirm_bar = action_bar_layer_create();
  action_bar_layer_set_background_color(s_confirm_bar, GColorRed);
  action_bar_layer_set_icon(s_confirm_bar, BUTTON_ID_SELECT, s_icon_check);
  action_bar_layer_set_click_config_provider(s_confirm_bar, prv_confirm_click_config);
  action_bar_layer_add_to_window(s_confirm_bar, w);
}

static void prv_confirm_unload(Window *w) {
  action_bar_layer_destroy(s_confirm_bar);
  text_layer_destroy(s_confirm_text);
  window_destroy(w);
  s_confirm_window = NULL;
}

static void prv_confirm_open(const char *key, const char *question) {
  if (s_confirm_window) return;
  prv_copy(s_stop_key, sizeof(s_stop_key), key);
  prv_copy(s_confirm_buf, sizeof(s_confirm_buf), question && question[0] ? question : tr(STR_STOP_CONFIRM));
  s_confirm_window = window_create();
  window_set_window_handlers(s_confirm_window, (WindowHandlers) {
    .load = prv_confirm_load, .unload = prv_confirm_unload,
  });
  window_stack_push(s_confirm_window, true);
}

/* ------------------------------------------------------------------ */
/* Detailansicht                                                       */
/* ------------------------------------------------------------------ */

static void prv_bar_update(Layer *layer, GContext *ctx) {
  if (!s_detail) return;
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(0, 2, b.size.w, 6), 3, GCornersAll);
  int w = b.size.w * s_detail->progress / 100;
  if (w > 0) {
    graphics_context_set_fill_color(ctx, GColorOrange);
    graphics_fill_rect(ctx, GRect(0, 2, w, 6), 3, GCornersAll);
  }
}

static void prv_detail_up(ClickRecognizerRef rec, void *ctx) {
  if (s_detail) scroll_layer_scroll_up_click_handler(rec, s_detail->scroll);
}

static void prv_detail_down(ClickRecognizerRef rec, void *ctx) {
  if (s_detail) scroll_layer_scroll_down_click_handler(rec, s_detail->scroll);
}

static void prv_detail_stop(ClickRecognizerRef rec, void *ctx) {
  if (s_detail && s_detail->key[0]) prv_confirm_open(s_detail->key, s_detail->confirm);
}

// Bei Streams gehören die Tasten der Aktionsleiste: UP/DOWN scrollen, SELECT beendet
static void prv_detail_bar_click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, prv_detail_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, prv_detail_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_detail_stop);
}

static void prv_detail_load(Window *w) {
  DetailView *d = s_detail;
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  bool stream = d->key[0] != '\0';
  const int16_t pad = 8;
  int16_t view_w = stream ? b.size.w - ACTION_BAR_WIDTH : b.size.w;
  const int16_t tw = view_w - 2 * pad;

  d->scroll = scroll_layer_create(GRect(0, 0, view_w, b.size.h));
  if (!stream) scroll_layer_set_click_config_onto_window(d->scroll, w);
  scroll_layer_set_shadow_hidden(d->scroll, true);

  int16_t y = 4;
  d->title_layer = text_layer_create(GRect(pad, y, tw, 2000));
  text_layer_set_font(d->title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_overflow_mode(d->title_layer, GTextOverflowModeWordWrap);
  text_layer_set_text(d->title_layer, d->title);
  GSize ts = text_layer_get_content_size(d->title_layer);
  layer_set_frame(text_layer_get_layer(d->title_layer), GRect(pad, y, tw, ts.h + 6));
  scroll_layer_add_child(d->scroll, text_layer_get_layer(d->title_layer));
  y += ts.h + 6;

  if (d->progress >= 0) {
    d->bar_layer = layer_create(GRect(pad, y, tw, 10));
    layer_set_update_proc(d->bar_layer, prv_bar_update);
    scroll_layer_add_child(d->scroll, d->bar_layer);
    y += 12;
  }

  d->body_layer = text_layer_create(GRect(pad, y, tw, 4000));
  text_layer_set_font(d->body_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_overflow_mode(d->body_layer, GTextOverflowModeWordWrap);
  text_layer_set_text(d->body_layer, d->body);
  GSize bs = text_layer_get_content_size(d->body_layer);
  layer_set_frame(text_layer_get_layer(d->body_layer), GRect(pad, y, tw, bs.h + 8));
  scroll_layer_add_child(d->scroll, text_layer_get_layer(d->body_layer));
  y += bs.h + 16;

  scroll_layer_set_content_size(d->scroll, GSize(view_w, y));
  layer_add_child(root, scroll_layer_get_layer(d->scroll));

  if (stream) {
    d->action_bar = action_bar_layer_create();
    action_bar_layer_set_background_color(d->action_bar, GColorRed);
    action_bar_layer_set_icon(d->action_bar, BUTTON_ID_SELECT, s_icon_stop);
    action_bar_layer_set_click_config_provider(d->action_bar, prv_detail_bar_click_config);
    action_bar_layer_add_to_window(d->action_bar, w);
  }
}

static void prv_detail_unload(Window *w) {
  DetailView *d = s_detail;
  if (d->action_bar) action_bar_layer_destroy(d->action_bar);
  text_layer_destroy(d->title_layer);
  text_layer_destroy(d->body_layer);
  if (d->bar_layer) layer_destroy(d->bar_layer);
  scroll_layer_destroy(d->scroll);
  free(d->title);
  free(d->body);
  free(d->confirm);
  free(d);
  s_detail = NULL;
  window_destroy(w);
}

static void prv_detail_open(const Item *item) {
  if (s_detail || !item->detail) return;
  DetailView *d = calloc(1, sizeof(DetailView));
  if (!d) return;
  d->progress = item->progress;
  if (item->kind == KIND_STREAM) {
    prv_copy(d->key, sizeof(d->key), item->key);
    d->confirm = prv_strdup(item->confirm);
  }
  d->title = prv_strdup(item->title);
  d->body = prv_strdup(item->detail);
  if (!d->title || !d->body) {
    free(d->title); free(d->body); free(d->confirm); free(d);
    return;
  }
  s_detail = d;
  d->window = window_create();
  window_set_window_handlers(d->window, (WindowHandlers) {
    .load = prv_detail_load, .unload = prv_detail_unload,
  });
  window_stack_push(d->window, true);
}

/* ------------------------------------------------------------------ */
/* Listen                                                              */
/* ------------------------------------------------------------------ */

static ListView *prv_list_open(uint8_t list_id, const char *param, const char *title);

// Anzahl der Abschnitte, die vom Handy kommen (mind. 1 für Status/Platzhalter)
static uint8_t prv_data_sections(ListView *v) {
  return v->num_sections > 0 ? v->num_sections : 1;
}

static bool prv_is_more_section(ListView *v, uint16_t section) {
  return prv_is_home(v) && section == prv_data_sections(v);
}

static bool prv_shows_status(ListView *v) { return v->count == 0; }

// Liefert das Item zu Abschnitt/Zeile oder NULL
static Item *prv_item_at(ListView *v, uint16_t section, uint16_t row) {
  uint16_t n = 0;
  for (int i = 0; i < v->count; i++) {
    if (v->items[i].section != section) continue;
    if (n == row) return &v->items[i];
    n++;
  }
  return NULL;
}

static uint16_t prv_num_sections(MenuLayer *m, void *ctx) {
  ListView *v = ctx;
  return prv_data_sections(v) + (prv_is_home(v) ? 1 : 0);
}

static uint16_t prv_num_rows(MenuLayer *m, uint16_t section, void *ctx) {
  ListView *v = ctx;
  if (prv_is_more_section(v, section)) return MORE_COUNT;
  if (prv_shows_status(v)) return section == 0 ? 1 : 0;
  uint16_t n = 0;
  for (int i = 0; i < v->count; i++) if (v->items[i].section == section) n++;
  return n;
}

static int16_t prv_header_h(MenuLayer *m, uint16_t section, void *ctx) {
  ListView *v = ctx;
  if (prv_is_more_section(v, section)) return MENU_CELL_BASIC_HEADER_HEIGHT;
  if (section < v->num_sections && v->sections[section][0]) return MENU_CELL_BASIC_HEADER_HEIGHT;
  return 0;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  ListView *v = data;
  if (prv_is_more_section(v, section)) {
    menu_cell_basic_header_draw(ctx, cell, tr(STR_MORE));
  } else if (section < v->num_sections) {
    menu_cell_basic_header_draw(ctx, cell, v->sections[section]);
  }
}

static int16_t prv_cell_h(MenuLayer *m, MenuIndex *idx, void *ctx) {
  ListView *v = ctx;
  // Status- und Fehlermeldungen bekommen Platz für mehrere Zeilen
  if (!prv_is_more_section(v, idx->section) && prv_shows_status(v)) return 112;
  if (!prv_is_more_section(v, idx->section)) {
    Item *it = prv_item_at(v, idx->section, idx->row);
    if (it && it->progress >= 0) return 54;
  }
  return 46;
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  ListView *v = data;
  if (prv_is_more_section(v, idx->section)) {
    menu_cell_basic_draw(ctx, cell, tr(s_more[idx->row].title), tr(s_more[idx->row].subtitle), NULL);
    return;
  }
  if (prv_shows_status(v)) {
    GRect sb = layer_get_bounds(cell);
    graphics_context_set_text_color(ctx, menu_cell_layer_is_highlighted(cell) ? GColorWhite : GColorBlack);
    graphics_draw_text(ctx, v->status[0] ? v->status : tr(STR_LOADING),
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(6, 2, sb.size.w - 12, sb.size.h - 24),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    if (!v->loading) {
      graphics_draw_text(ctx, tr(STR_RELOAD_HINT), fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(6, sb.size.h - 20, sb.size.w - 12, 18), GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentLeft, NULL);
    }
    return;
  }
  Item *it = prv_item_at(v, idx->section, idx->row);
  if (!it) return;
  bool hl = menu_cell_layer_is_highlighted(cell);
  GRect b = layer_get_bounds(cell);

  if (it->progress >= 0) {
    // Eigene Zeile mit Fortschrittsbalken
    graphics_context_set_text_color(ctx, hl ? GColorWhite : GColorBlack);
    graphics_draw_text(ctx, it->title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(6, -4, b.size.w - 12, 28), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, it->subtitle, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(6, 22, b.size.w - 12, 22), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
    GRect bar = GRect(6, b.size.h - 8, b.size.w - 12, 4);
    graphics_context_set_fill_color(ctx, hl ? GColorDarkGray : GColorLightGray);
    graphics_fill_rect(ctx, bar, 2, GCornersAll);
    int w = bar.size.w * it->progress / 100;
    if (w > 0) {
      graphics_context_set_fill_color(ctx, hl ? GColorWhite : GColorOrange);
      graphics_fill_rect(ctx, GRect(bar.origin.x, bar.origin.y, w, bar.size.h), 2, GCornersAll);
    }
  } else {
    menu_cell_basic_draw(ctx, cell, it->title, it->subtitle[0] ? it->subtitle : NULL, NULL);
  }
}

static void prv_select(MenuLayer *m, MenuIndex *idx, void *ctx) {
  ListView *v = ctx;
  if (prv_is_more_section(v, idx->section)) {
    uint8_t id = s_more[idx->row].list_id;
    if (id == LIST_CHART) chart_open(prv_chart_request, NULL);
    else prv_list_open(id, NULL, tr(s_more[idx->row].title));
    return;
  }
  if (prv_shows_status(v)) {
    if (!v->loading) prv_send_load(v);
    return;
  }
  Item *it = prv_item_at(v, idx->section, idx->row);
  if (!it) return;
  switch (it->kind) {
    case KIND_DETAIL:
    case KIND_STREAM:
      prv_detail_open(it);
      break;
    case KIND_USER:
      prv_list_open(LIST_USER_HISTORY, it->key, it->title);
      break;
    case KIND_CHART:
      chart_open(prv_chart_request, it->key);
      break;
  }
}

static void prv_select_long(MenuLayer *m, MenuIndex *idx, void *ctx) {
  ListView *v = ctx;
  vibes_short_pulse();
  prv_send_load(v);
}

static void prv_header_update(Layer *layer, GContext *ctx) {
  ListView *v = *(ListView **)layer_get_data(layer);
  GRect b = layer_get_bounds(layer);
  GColor bg = COLOR_ACCENT;
  GColor fg = GColorBlack;
  if (v->header_state == 1) { bg = GColorIslamicGreen; fg = GColorWhite; }
  if (v->header_state == 2) { bg = GColorRed; fg = GColorWhite; }
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, fg);
  char buf[56];
  const char *head = v->header[0] ? v->header : (v->title[0] ? v->title : tr(STR_APP_NAME));
  snprintf(buf, sizeof(buf), "%s%s", head, v->loading ? " …" : "");
  graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(4, 2, b.size.w - 8, b.size.h - 2), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void prv_list_window_load(Window *w) {
  ListView *v = window_get_user_data(w);
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);

  v->header_layer = layer_create_with_data(GRect(0, 0, b.size.w, HEADER_H), sizeof(ListView *));
  *(ListView **)layer_get_data(v->header_layer) = v;
  layer_set_update_proc(v->header_layer, prv_header_update);
  layer_add_child(root, v->header_layer);

  v->menu = menu_layer_create(GRect(0, HEADER_H, b.size.w, b.size.h - HEADER_H));
  menu_layer_set_highlight_colors(v->menu, GColorOrange, GColorWhite);
  menu_layer_set_callbacks(v->menu, v, (MenuLayerCallbacks) {
    .get_num_sections = prv_num_sections,
    .get_num_rows = prv_num_rows,
    .get_header_height = prv_header_h,
    .draw_header = prv_draw_header,
    .get_cell_height = prv_cell_h,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
    .select_long_click = prv_select_long,
  });
  menu_layer_set_click_config_onto_window(v->menu, w);
  layer_add_child(root, menu_layer_get_layer(v->menu));
}

static void prv_list_window_appear(Window *w) {
  ListView *v = window_get_user_data(w);
  // Beim ersten Öffnen lädt das Handy selbst (Startbildschirm) bzw. die
  // Unterliste fordert ihre Daten in prv_list_open an. Bei jeder Rückkehr
  // zum Startbildschirm wird aktualisiert.
  if (!v->appeared_once) { v->appeared_once = true; return; }
  if (prv_is_home(v) && !v->loading) prv_send_load(v);
}

static void prv_list_window_unload(Window *w) {
  ListView *v = window_get_user_data(w);
  menu_layer_destroy(v->menu);
  layer_destroy(v->header_layer);
  prv_free_items(v->items, v->count);
  prv_free_items(v->pending, v->pending_total);
  for (int i = 0; i < MAX_VIEWS; i++) if (s_views[i] == v) s_views[i] = NULL;
  if (v == s_home) s_home = NULL;
  free(v);
  window_destroy(w);
}

static ListView *prv_list_create(uint8_t list_id, const char *param, const char *title) {
  int slot = -1;
  for (int i = 0; i < MAX_VIEWS; i++) if (!s_views[i]) { slot = i; break; }
  if (slot < 0) return NULL;
  ListView *v = calloc(1, sizeof(ListView));
  if (!v) return NULL;
  v->list_id = list_id;
  prv_copy(v->param, sizeof(v->param), param);
  prv_copy(v->title, sizeof(v->title), title);
  prv_copy(v->status, sizeof(v->status), tr(STR_LOADING));
  v->window = window_create();
  window_set_user_data(v->window, v);
  window_set_window_handlers(v->window, (WindowHandlers) {
    .load = prv_list_window_load,
    .appear = prv_list_window_appear,
    .unload = prv_list_window_unload,
  });
  s_views[slot] = v;
  return v;
}

static ListView *prv_list_open(uint8_t list_id, const char *param, const char *title) {
  if (prv_view_for(list_id)) return NULL;   // ist schon offen
  ListView *v = prv_list_create(list_id, param, title);
  if (!v) return NULL;
  window_stack_push(v->window, true);
  prv_send_load(v);
  return v;
}

/* ------------------------------------------------------------------ */
/* Empfang                                                             */
/* ------------------------------------------------------------------ */

static void prv_handle_begin(ListView *v, DictionaryIterator *it) {
  prv_free_items(v->pending, v->pending_total);
  v->pending = NULL;
  v->pending_total = 0;

  Tuple *t = dict_find(it, MESSAGE_KEY_Total);
  int total = t ? t->value->int32 : 0;
  if (total > MAX_ITEMS) total = MAX_ITEMS;
  if (total < 0) total = 0;
  if (total > 0) {
    v->pending = calloc(total, sizeof(Item));
    if (!v->pending) total = 0;
  }
  v->pending_total = total;
  for (int i = 0; i < total; i++) v->pending[i].progress = -1;

  t = dict_find(it, MESSAGE_KEY_Header);
  prv_copy(v->pending_header, sizeof(v->pending_header), t ? t->value->cstring : v->header);
  t = dict_find(it, MESSAGE_KEY_HeaderState);
  v->pending_header_state = t ? t->value->int32 : 0;
  t = dict_find(it, MESSAGE_KEY_Sections);
  v->pending_num_sections = prv_split(t ? t->value->cstring : NULL, v->pending_sections, MAX_SECTIONS);
}

static void prv_handle_item(ListView *v, DictionaryIterator *it) {
  Tuple *t = dict_find(it, MESSAGE_KEY_Index);
  if (!t || !v->pending) return;
  int idx = t->value->int32;
  if (idx < 0 || idx >= v->pending_total) return;
  Item *item = &v->pending[idx];

  t = dict_find(it, MESSAGE_KEY_Section);
  item->section = t ? t->value->int32 : 0;
  t = dict_find(it, MESSAGE_KEY_Kind);
  item->kind = t ? t->value->int32 : KIND_NONE;
  t = dict_find(it, MESSAGE_KEY_Progress);
  item->progress = t ? t->value->int32 : -1;
  if (item->progress > 100) item->progress = 100;
  t = dict_find(it, MESSAGE_KEY_Title);
  prv_copy(item->title, sizeof(item->title), t ? t->value->cstring : "");
  t = dict_find(it, MESSAGE_KEY_Subtitle);
  prv_copy(item->subtitle, sizeof(item->subtitle), t ? t->value->cstring : "");
  t = dict_find(it, MESSAGE_KEY_Key);
  prv_copy(item->key, sizeof(item->key), t ? t->value->cstring : "");
  free(item->detail);
  item->detail = prv_strdup(prv_tuple_str(dict_find(it, MESSAGE_KEY_Detail)));
  free(item->confirm);
  item->confirm = prv_strdup(prv_tuple_str(dict_find(it, MESSAGE_KEY_Text)));
  // Ohne Detailtext gibt es nichts zu öffnen
  if ((item->kind == KIND_DETAIL || item->kind == KIND_STREAM) && !item->detail) item->kind = KIND_NONE;
  if (item->kind == KIND_STREAM && !item->key[0]) item->kind = KIND_DETAIL;
  if ((item->kind == KIND_USER || item->kind == KIND_CHART) && !item->key[0]) item->kind = KIND_NONE;
}

static void prv_handle_end(ListView *v) {
  MenuIndex sel = menu_layer_get_selected_index(v->menu);

  prv_free_items(v->items, v->count);
  v->items = v->pending;
  v->count = v->pending_total;
  v->pending = NULL;
  v->pending_total = 0;

  prv_copy(v->header, sizeof(v->header), v->pending_header);
  v->header_state = v->pending_header_state;
  memcpy(v->sections, v->pending_sections, sizeof(v->sections));
  v->num_sections = v->pending_num_sections;
  v->loading = false;
  prv_copy(v->status, sizeof(v->status), v->count ? "" : tr(STR_NO_ENTRIES));

  menu_layer_reload_data(v->menu);
  // Auswahl möglichst beibehalten
  uint16_t sections = prv_num_sections(v->menu, v);
  if (sel.section >= sections) sel.section = sections - 1;
  uint16_t rows = prv_num_rows(v->menu, sel.section, v);
  if (rows == 0) { sel.section = 0; sel.row = 0; }
  else if (sel.row >= rows) sel.row = rows - 1;
  menu_layer_set_selected_index(v->menu, sel, MenuRowAlignNone, false);
  layer_mark_dirty(v->header_layer);
}

static void prv_handle_error(ListView *v, DictionaryIterator *it) {
  Tuple *t = dict_find(it, MESSAGE_KEY_Text);
  prv_copy(v->status, sizeof(v->status), t ? t->value->cstring : tr(STR_ERROR));
  t = dict_find(it, MESSAGE_KEY_Header);
  if (t) prv_copy(v->header, sizeof(v->header), t->value->cstring);
  t = dict_find(it, MESSAGE_KEY_HeaderState);
  if (t) v->header_state = t->value->int32;
  v->loading = false;
  prv_free_items(v->pending, v->pending_total);
  v->pending = NULL;
  v->pending_total = 0;
  // Alte Daten wären veraltet -> Fehlermeldung anzeigen
  prv_free_items(v->items, v->count);
  v->items = NULL;
  v->count = 0;
  v->num_sections = 0;
  prv_reload(v);
}

static void prv_handle_result(DictionaryIterator *it) {
  Tuple *ok = dict_find(it, MESSAGE_KEY_Ok);
  Tuple *text = dict_find(it, MESSAGE_KEY_Text);
  bool success = ok && ok->value->int32;
  if (success) {
    vibes_short_pulse();
    if (s_detail) window_stack_remove(s_detail->window, false);
  } else {
    vibes_double_pulse();
  }
  prv_toast_show(text ? text->value->cstring : (success ? tr(STR_STOPPED) : tr(STR_ERROR)), true);
  if (success && s_home) prv_send_load(s_home);
}

// Eine Wiedergabe ist zu Ende geschaut: zweimal lang vibrieren und kurz anzeigen
static void prv_handle_notify(DictionaryIterator *it) {
  const char *text = prv_tuple_str(dict_find(it, MESSAGE_KEY_Text));
  if (!text) return;
  static const uint32_t segments[] = { 300, 150, 300 };
  vibes_enqueue_custom_pattern((VibePattern) { .durations = segments, .num_segments = ARRAY_LENGTH(segments) });
  prv_toast_show_ms(text, 5000);
}

static void prv_inbox(DictionaryIterator *it, void *ctx) {
  Tuple *t = dict_find(it, MESSAGE_KEY_Type);
  if (!t) return;
  int type = t->value->int32;
  prv_apply_lang(it);
  if (type == T_RESULT) { prv_handle_result(it); return; }
  if (type == T_CHART) { chart_handle_data(it); return; }
  if (type == T_NOTIFY) { prv_handle_notify(it); return; }

  Tuple *lt = dict_find(it, MESSAGE_KEY_ListId);
  int list_id = lt ? lt->value->int32 : 0;
  if (list_id == LIST_CHART) {
    if (type == T_ERROR) chart_handle_error(prv_tuple_str(dict_find(it, MESSAGE_KEY_Text)));
    return;
  }
  ListView *v = prv_view_for(list_id);
  if (!v) return;   // Fenster wurde inzwischen geschlossen
  switch (type) {
    case T_LIST_BEGIN: prv_handle_begin(v, it); break;
    case T_ITEM:       prv_handle_item(v, it); break;
    case T_LIST_END:   prv_handle_end(v); break;
    case T_ERROR:      prv_handle_error(v, it); break;
  }
}

static void prv_outbox_failed(DictionaryIterator *it, AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Senden fehlgeschlagen: %d", (int)reason);
  for (int i = 0; i < MAX_VIEWS; i++) {
    ListView *v = s_views[i];
    if (v && v->loading) {
      v->loading = false;
      if (v->count == 0) prv_copy(v->status, sizeof(v->status), tr(STR_NO_PHONE));
      prv_reload(v);
    }
  }
  if (chart_is_open()) chart_handle_error(tr(STR_NO_PHONE));
}

/* ------------------------------------------------------------------ */
/* Automatische Aktualisierung des Startbildschirms                    */
/* ------------------------------------------------------------------ */

static void prv_refresh_tick(void *ctx) {
  s_refresh_timer = app_timer_register(REFRESH_MS, prv_refresh_tick, NULL);
  if (!s_home) return;
  if (window_stack_get_top_window() != s_home->window) return;
  if (s_home->loading && time(NULL) - s_home->load_started < LOAD_TIMEOUT) return;
  prv_send_load(s_home);
}

/* ------------------------------------------------------------------ */

static void prv_init(void) {
  i18n_init();
  s_icon_stop = gbitmap_create_with_resource(RESOURCE_ID_ICON_STOP);
  s_icon_check = gbitmap_create_with_resource(RESOURCE_ID_ICON_CHECK);

  app_message_register_inbox_received(prv_inbox);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(2048, 256);

  s_home = prv_list_create(LIST_HOME, NULL, tr(STR_APP_NAME));
  s_home->loading = true;
  s_home->load_started = time(NULL);
  window_stack_push(s_home->window, true);
  s_refresh_timer = app_timer_register(REFRESH_MS, prv_refresh_tick, NULL);
}

static void prv_deinit(void) {
  if (s_refresh_timer) app_timer_cancel(s_refresh_timer);
  window_stack_pop_all(false);
  gbitmap_destroy(s_icon_stop);
  gbitmap_destroy(s_icon_check);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
