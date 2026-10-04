/*
 * Tautulli für Pebble – Texte auf der Uhr in Deutsch und Englisch.
 * Die meisten Texte erzeugt das Handy bereits in der richtigen Sprache;
 * hier stehen nur die Texte, die die Uhr selbst anzeigt.
 */
#pragma once
#include <pebble.h>

typedef enum {
  LANG_DE = 0,
  LANG_EN = 1,
} Language;

typedef enum {
  STR_APP_NAME,
  STR_LOADING,
  STR_NO_PHONE,
  STR_RELOAD_HINT,
  STR_NO_ENTRIES,
  STR_ERROR,
  STR_MORE,
  STR_MORE_HISTORY,
  STR_MORE_HISTORY_SUB,
  STR_MORE_RECENT,
  STR_MORE_RECENT_SUB,
  STR_MORE_USERS,
  STR_MORE_USERS_SUB,
  STR_MORE_STATS,
  STR_MORE_STATS_SUB,
  STR_MORE_CHART,
  STR_MORE_CHART_SUB,
  STR_MORE_LIBRARIES,
  STR_MORE_LIBRARIES_SUB,
  STR_STOP_CONFIRM,
  STR_STOPPING,
  STR_STOPPED,
  STR_CHART_TO_30,
  STR_CHART_TO_7,
  STR_COUNT
} StrId;

void i18n_init(void);
// Setzt die Sprache und speichert sie. Liefert true, wenn sie sich geändert hat.
bool i18n_set_language(Language lang);
Language i18n_language(void);
const char *tr(StrId id);
