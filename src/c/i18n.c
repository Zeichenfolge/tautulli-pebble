/*
 * Tautulli für Pebble – Texte in Deutsch und Englisch.
 * Die Sprache kommt aus den Einstellungen (Handy) und wird auf der Uhr
 * gespeichert, damit schon der erste Bildschirm in der richtigen Sprache
 * erscheint. Vor der ersten Verbindung gilt die Sprache der Uhr.
 */
#include "i18n.h"

#define PERSIST_KEY_LANG 1

static Language s_lang = LANG_DE;

static const char *const s_de[STR_COUNT] = {
  [STR_APP_NAME]           = "Tautulli",
  [STR_LOADING]            = "Lade …",
  [STR_NO_PHONE]           = "Keine Verbindung zum Handy",
  [STR_RELOAD_HINT]        = "SELECT: neu laden",
  [STR_NO_ENTRIES]         = "Keine Einträge",
  [STR_ERROR]              = "Fehler",
  [STR_MORE]               = "Mehr",
  [STR_MORE_HISTORY]       = "Verlauf",
  [STR_MORE_HISTORY_SUB]   = "Zuletzt geschaut",
  [STR_MORE_RECENT]        = "Neu hinzugefügt",
  [STR_MORE_RECENT_SUB]    = "Filme und Folgen",
  [STR_MORE_USERS]         = "Nutzer",
  [STR_MORE_USERS_SUB]     = "Wer zuletzt geschaut hat",
  [STR_MORE_STATS]         = "Statistik",
  [STR_MORE_STATS_SUB]     = "Wiedergabezeit, Top-Listen",
  [STR_MORE_CHART]         = "Diagramm",
  [STR_MORE_CHART_SUB]     = "Wiedergaben pro Tag",
  [STR_MORE_LIBRARIES]     = "Bibliotheken",
  [STR_MORE_LIBRARIES_SUB] = "Filme, Serien, Musik",
  [STR_STOP_CONFIRM]       = "Stream beenden?",
  [STR_STOPPING]           = "Stream wird beendet …",
  [STR_STOPPED]            = "Stream beendet",
  [STR_CHART_TO_30]        = "SELECT: 30 Tage",
  [STR_CHART_TO_7]         = "SELECT: 7 Tage",
};

static const char *const s_en[STR_COUNT] = {
  [STR_APP_NAME]           = "Tautulli",
  [STR_LOADING]            = "Loading …",
  [STR_NO_PHONE]           = "No connection to phone",
  [STR_RELOAD_HINT]        = "SELECT: reload",
  [STR_NO_ENTRIES]         = "Nothing here",
  [STR_ERROR]              = "Error",
  [STR_MORE]               = "More",
  [STR_MORE_HISTORY]       = "History",
  [STR_MORE_HISTORY_SUB]   = "Recently watched",
  [STR_MORE_RECENT]        = "Recently added",
  [STR_MORE_RECENT_SUB]    = "Movies and episodes",
  [STR_MORE_USERS]         = "Users",
  [STR_MORE_USERS_SUB]     = "Who watched last",
  [STR_MORE_STATS]         = "Statistics",
  [STR_MORE_STATS_SUB]     = "Watch time, top lists",
  [STR_MORE_CHART]         = "Chart",
  [STR_MORE_CHART_SUB]     = "Plays per day",
  [STR_MORE_LIBRARIES]     = "Libraries",
  [STR_MORE_LIBRARIES_SUB] = "Movies, shows, music",
  [STR_STOP_CONFIRM]       = "Stop stream?",
  [STR_STOPPING]           = "Stopping stream …",
  [STR_STOPPED]            = "Stream stopped",
  [STR_CHART_TO_30]        = "SELECT: 30 days",
  [STR_CHART_TO_7]         = "SELECT: 7 days",
};

void i18n_init(void) {
  if (persist_exists(PERSIST_KEY_LANG)) {
    s_lang = persist_read_int(PERSIST_KEY_LANG) == LANG_EN ? LANG_EN : LANG_DE;
  } else {
    const char *loc = i18n_get_system_locale();
    s_lang = (loc && strncmp(loc, "de", 2) == 0) ? LANG_DE : LANG_EN;
  }
}

bool i18n_set_language(Language lang) {
  if (lang != LANG_EN) lang = LANG_DE;
  bool changed = lang != s_lang || !persist_exists(PERSIST_KEY_LANG);
  s_lang = lang;
  if (changed) persist_write_int(PERSIST_KEY_LANG, lang);
  return changed;
}

Language i18n_language(void) {
  return s_lang;
}

const char *tr(StrId id) {
  if (id >= STR_COUNT) return "";
  const char *s = (s_lang == LANG_EN ? s_en : s_de)[id];
  return s ? s : "";
}
