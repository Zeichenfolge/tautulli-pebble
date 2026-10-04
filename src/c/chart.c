/*
 * Tautulli für Pebble – Balkendiagramm „Wiedergaben pro Tag“.
 *
 * Gestapelte Balken je Tag: Serien, Filme, Sonstiges (Musik, Live-TV).
 * SELECT wechselt zwischen 7 und 30 Tagen.
 *
 * Daten vom Handy:
 *   Total    Anzahl Tage (max. 31)
 *   Data     Bytes: je Tag drei uint16 (little endian): Serien, Filme, Sonstiges
 *   Labels   Achsenbeschriftung je Tag, getrennt durch "|"
 *   Sections Namen der drei Reihen für die Legende, getrennt durch "|"
 *   Header   Kopfzeile, z. B. "7 Tage · 42 Wiedergaben"
 */
#include "chart.h"
#include "i18n.h"

#define MAX_DAYS   31
#define SERIES     3
#define HEADER_H   28
#define LABEL_LEN  6
#define NAME_LEN   16

typedef struct {
  Window *window;
  Layer *layer;
  ChartRequestFn request;
  int days_requested;
  int days;
  uint16_t values[MAX_DAYS][SERIES];
  char labels[MAX_DAYS][LABEL_LEN];
  char names[SERIES][NAME_LEN];
  char header[48];
  char status[96];
  bool loading;
  bool has_data;
} Chart;

static Chart *s_chart;

static GColor prv_series_color(int i) {
  switch (i) {
    case 0: return GColorOrange;
    case 1: return GColorVividCerulean;
    default: return GColorJaegerGreen;
  }
}

static void prv_copy(char *dst, size_t size, const char *src) {
  if (!src) { dst[0] = '\0'; return; }
  strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

static int prv_split(const char *src, char *out, int max_parts, size_t part_size) {
  int n = 0;
  if (!src) return 0;
  const char *p = src;
  while (n < max_parts) {
    const char *end = strchr(p, '|');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= part_size) len = part_size - 1;
    memcpy(out + n * part_size, p, len);
    out[n * part_size + len] = '\0';
    n++;
    if (!end) break;
    p = end + 1;
  }
  return n;
}

static void prv_request(void) {
  if (!s_chart) return;
  s_chart->loading = true;
  if (!s_chart->has_data) prv_copy(s_chart->status, sizeof(s_chart->status), tr(STR_LOADING));
  layer_mark_dirty(s_chart->layer);
  s_chart->request(s_chart->days_requested);
}

static void prv_draw_text(GContext *ctx, const char *text, const char *font, GRect r, GTextAlignment align) {
  graphics_draw_text(ctx, text, fonts_get_system_font(font), r, GTextOverflowModeTrailingEllipsis, align, NULL);
}

static void prv_update(Layer *layer, GContext *ctx) {
  Chart *c = s_chart;
  if (!c) return;
  GRect b = layer_get_bounds(layer);

  // Kopfzeile
  graphics_context_set_fill_color(ctx, GColorChromeYellow);
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, HEADER_H), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  char head[56];
  snprintf(head, sizeof(head), "%s%s", c->header[0] ? c->header : tr(STR_MORE_CHART), c->loading ? " …" : "");
  prv_draw_text(ctx, head, FONT_KEY_GOTHIC_18_BOLD, GRect(4, 2, b.size.w - 8, HEADER_H - 2), GTextAlignmentCenter);

  // Fußzeile mit Hinweis
  const int16_t hint_h = 18;
  graphics_context_set_text_color(ctx, GColorDarkGray);
  prv_draw_text(ctx, tr(c->days_requested == 7 ? STR_CHART_TO_30 : STR_CHART_TO_7), FONT_KEY_GOTHIC_14,
                GRect(0, b.size.h - hint_h, b.size.w, hint_h), GTextAlignmentCenter);

  if (!c->has_data) {
    graphics_context_set_text_color(ctx, GColorBlack);
    prv_draw_text(ctx, c->status, FONT_KEY_GOTHIC_24_BOLD,
                  GRect(8, HEADER_H + 10, b.size.w - 16, b.size.h - HEADER_H - 40), GTextAlignmentCenter);
    return;
  }

  // Legende
  const int16_t legend_h = 18;
  int16_t legend_y = b.size.h - hint_h - legend_h;
  int16_t x = 6;
  int16_t col_w = (b.size.w - 12) / SERIES;
  for (int s = 0; s < SERIES; s++) {
    graphics_context_set_fill_color(ctx, prv_series_color(s));
    graphics_fill_rect(ctx, GRect(x, legend_y + 6, 8, 8), 1, GCornersAll);
    graphics_context_set_text_color(ctx, GColorBlack);
    prv_draw_text(ctx, c->names[s], FONT_KEY_GOTHIC_14, GRect(x + 11, legend_y, col_w - 12, legend_h),
                  GTextAlignmentLeft);
    x += col_w;
  }

  // Diagrammfläche
  const int16_t label_h = 16;
  const int16_t value_h = 16;
  const int16_t pad = 6;
  int16_t top = HEADER_H + 4 + value_h;
  int16_t bottom = legend_y - label_h - 2;  // Grundlinie
  int16_t height = bottom - top;
  if (height < 10 || c->days <= 0) return;

  int max = 1;
  int max_day = -1;   // letzter Tag mit dem Höchstwert
  for (int d = 0; d < c->days; d++) {
    int sum = c->values[d][0] + c->values[d][1] + c->values[d][2];
    if (sum >= max) { max = sum; max_day = d; }
  }

  int16_t area_w = b.size.w - 2 * pad;
  int16_t slot = area_w / c->days;
  int16_t gap = c->days > 10 ? 1 : 4;
  int16_t bar_w = slot - gap;
  if (bar_w < 2) bar_w = 2;
  int16_t x0 = pad + (area_w - slot * c->days) / 2;
  bool few = c->days <= 10;
  int label_every = few ? 1 : 7;

  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(pad, bottom), GPoint(b.size.w - pad, bottom));

  for (int d = 0; d < c->days; d++) {
    int16_t bx = x0 + d * slot + gap / 2;
    int16_t y = bottom;
    int sum = 0;
    for (int s = 0; s < SERIES; s++) {
      int v = c->values[d][s];
      if (v <= 0) continue;
      sum += v;
      int16_t h = (int32_t)v * height / max;
      if (h < 1) h = 1;
      graphics_context_set_fill_color(ctx, prv_series_color(s));
      graphics_fill_rect(ctx, GRect(bx, y - h, bar_w, h), 0, GCornerNone);
      y -= h;
    }
    char buf[12];
    // Werte über den Balken: bei 7 Tagen alle, bei 30 Tagen nur das Maximum
    if (sum > 0 && (few || d == max_day)) {
      snprintf(buf, sizeof(buf), "%d", sum);
      graphics_context_set_text_color(ctx, GColorBlack);
      int16_t tw = few ? slot : 30;
      prv_draw_text(ctx, buf, FONT_KEY_GOTHIC_14, GRect(bx + bar_w / 2 - tw / 2, y - value_h - 1, tw, value_h),
                    GTextAlignmentCenter);
    }
    // Achsenbeschriftung: bei 30 Tagen jeden 7. Tag, von hinten (heute) gezählt
    if ((c->days - 1 - d) % label_every == 0) {
      graphics_context_set_text_color(ctx, GColorBlack);
      int16_t lw = few ? slot : 30;
      prv_draw_text(ctx, c->labels[d], FONT_KEY_GOTHIC_14,
                    GRect(bx + bar_w / 2 - lw / 2, bottom + 1, lw, label_h), GTextAlignmentCenter);
    }
  }
}

static void prv_select(ClickRecognizerRef rec, void *ctx) {
  if (!s_chart) return;
  s_chart->days_requested = s_chart->days_requested == 7 ? 30 : 7;
  prv_request();
}

static void prv_select_long(ClickRecognizerRef rec, void *ctx) {
  vibes_short_pulse();
  prv_request();
}

static void prv_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_select_long, NULL);
}

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_chart->layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_chart->layer, prv_update);
  layer_add_child(root, s_chart->layer);
}

static void prv_unload(Window *w) {
  layer_destroy(s_chart->layer);
  free(s_chart);
  s_chart = NULL;
  window_destroy(w);
}

void chart_open(ChartRequestFn request) {
  if (s_chart) return;
  s_chart = calloc(1, sizeof(Chart));
  if (!s_chart) return;
  s_chart->request = request;
  s_chart->days_requested = 7;
  s_chart->window = window_create();
  window_set_click_config_provider(s_chart->window, prv_click_config);
  window_set_window_handlers(s_chart->window, (WindowHandlers) {
    .load = prv_load, .unload = prv_unload,
  });
  window_stack_push(s_chart->window, true);
  prv_request();
}

bool chart_is_open(void) {
  return s_chart != NULL;
}

void chart_handle_data(DictionaryIterator *it) {
  Chart *c = s_chart;
  if (!c) return;
  Tuple *t = dict_find(it, MESSAGE_KEY_Total);
  int days = t ? t->value->int32 : 0;
  if (days > MAX_DAYS) days = MAX_DAYS;
  if (days < 0) days = 0;
  memset(c->values, 0, sizeof(c->values));
  t = dict_find(it, MESSAGE_KEY_Data);
  if (t && t->type == TUPLE_BYTE_ARRAY) {
    int n = t->length / 2;
    const uint8_t *p = t->value->data;
    for (int i = 0; i < n && i < days * SERIES; i++) {
      c->values[i / SERIES][i % SERIES] = p[2 * i] | (p[2 * i + 1] << 8);
    }
  }
  memset(c->labels, 0, sizeof(c->labels));
  t = dict_find(it, MESSAGE_KEY_Labels);
  prv_split(t ? t->value->cstring : NULL, &c->labels[0][0], MAX_DAYS, LABEL_LEN);
  memset(c->names, 0, sizeof(c->names));
  t = dict_find(it, MESSAGE_KEY_Sections);
  prv_split(t ? t->value->cstring : NULL, &c->names[0][0], SERIES, NAME_LEN);
  t = dict_find(it, MESSAGE_KEY_Header);
  prv_copy(c->header, sizeof(c->header), t ? t->value->cstring : "");
  c->days = days;
  c->has_data = true;
  c->loading = false;
  layer_mark_dirty(c->layer);
}

void chart_handle_error(const char *text) {
  Chart *c = s_chart;
  if (!c) return;
  c->loading = false;
  c->has_data = false;
  prv_copy(c->status, sizeof(c->status), text ? text : tr(STR_ERROR));
  layer_mark_dirty(c->layer);
}

void chart_refresh(void) {
  if (s_chart) layer_mark_dirty(s_chart->layer);
}
