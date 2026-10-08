/*
 * Tautulli für Pebble – Balkendiagramm „Wiedergaben pro Tag“.
 */
#pragma once
#include <pebble.h>

// Fordert beim Handy die Daten für die angegebene Anzahl Tage an.
// key ist leer (alle) oder der Nutzer-Schlüssel "user_id|Name".
typedef void (*ChartRequestFn)(int days, const char *key);

void chart_open(ChartRequestFn request, const char *key);
bool chart_is_open(void);
// Daten vom Handy (Type = Chart). Header, Total, Data, Labels, Sections
void chart_handle_data(DictionaryIterator *it);
// Fehler vom Handy oder keine Verbindung
void chart_handle_error(const char *text);
// Sprache hat sich geändert -> neu zeichnen
void chart_refresh(void);
