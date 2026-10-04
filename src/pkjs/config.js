// Einstellungsseite (Clay) – erscheint in der Pebble-App unter den Einstellungen der App.
// Die Seite wird in der eingestellten Sprache erzeugt (nach dem Umstellen
// erscheint sie beim nächsten Öffnen in der neuen Sprache).

// Links im Bereich "Unterstützen". Leer lassen, um den jeweiligen Link auszublenden.
var SUPPORT_URL = 'https://buymeacoffee.com/michaelrunge';
var PROJECT_URL = 'https://github.com/Zeichenfolge/tautulli-pebble';

var TEXT = {
  de: {
    intro: 'Zeigt deine Plex-Streams, den Verlauf, neue Medien, Nutzer, Bibliotheken und Statistiken aus Tautulli auf der Uhr.',
    server: 'Server',
    url: 'Tautulli-Adresse',
    urlHint: 'Mit einer lokalen Adresse funktioniert die App nur im Heim-WLAN. ' +
      'Mit <b>demo</b> zeigt die App Beispieldaten, ganz ohne Server.',
    apiKey: 'API-Key',
    apiKeyHint: 'In Tautulli unter Settings → Web Interface → API.',
    apiKeyPlaceholder: 'API-Key einfügen',
    display: 'Anzeige',
    language: 'Sprache',
    langAuto: 'Wie die Uhr',
    count: 'Einträge in „Verlauf“ und „Neu hinzugefügt“',
    save: 'Speichern',
    support: 'Unterstützen',
    supportText: 'Tautulli für Pebble ist kostenlos und Open Source. Wenn dir die App gefällt, freue ich mich über einen Kaffee.',
    supportHint: 'Bitte vorher speichern – der Link öffnet sich in diesem Fenster.',
    project: 'Fehler gefunden oder eine Idee? Auf GitHub kannst du sie melden.'
  },
  en: {
    intro: 'Shows your Plex streams, history, new media, users, libraries and statistics from Tautulli on your watch.',
    server: 'Server',
    url: 'Tautulli address',
    urlHint: 'With a local address the app only works on your home Wi-Fi. ' +
      'Enter <b>demo</b> to see sample data without a server.',
    apiKey: 'API key',
    apiKeyHint: 'In Tautulli under Settings → Web Interface → API.',
    apiKeyPlaceholder: 'Paste API key',
    display: 'Display',
    language: 'Language',
    langAuto: 'Same as watch',
    count: 'Entries in “History” and “Recently added”',
    save: 'Save',
    support: 'Support',
    supportText: 'Tautulli for Pebble is free and open source. If you enjoy it, I’d be happy about a coffee.',
    supportHint: 'Please save first – the link opens in this window.',
    project: 'Found a bug or have an idea? Report it on GitHub.'
  }
};

function button(url, label) {
  return '<a href="' + url + '" style="display:block;margin:12px 0 4px;padding:12px;border-radius:8px;' +
    'background:#ffdd00;color:#000;font-weight:bold;text-align:center;text-decoration:none;">' + label + '</a>';
}

// Bereich "Unterstützen" – steht unter dem Speichern-Knopf, damit niemand
// ungespeicherte Einstellungen verliert, wenn er den Link öffnet.
function supportSection(t) {
  var items = [{ type: 'heading', defaultValue: t.support }];
  if (SUPPORT_URL) {
    items.push({ type: 'text', defaultValue: t.supportText + button(SUPPORT_URL, '☕ Buy me a coffee') });
    items.push({ type: 'text', defaultValue: '<small>' + t.supportHint + '</small>' });
  }
  if (PROJECT_URL) {
    items.push({ type: 'text', defaultValue: t.project + ' <a href="' + PROJECT_URL + '">GitHub</a>' });
  }
  return items.length > 1 ? [{ type: 'section', items: items }] : [];
}

module.exports = function (lang) {
  var t = TEXT[lang] || TEXT.de;
  return [
    { type: 'heading', defaultValue: 'Tautulli' },
    { type: 'text', defaultValue: t.intro },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.server },
        {
          type: 'input',
          messageKey: 'CfgUrl',
          label: t.url,
          description: t.urlHint,
          defaultValue: '',
          // Bewusst kein type "url": sonst lehnt die Seite "demo" und Adressen ohne http:// ab.
          attributes: { placeholder: 'http://192.168.1.10:8181', type: 'text', inputmode: 'url', autocapitalize: 'off', autocorrect: 'off', spellcheck: 'false' }
        },
        {
          type: 'input',
          messageKey: 'CfgApiKey',
          label: t.apiKey,
          description: t.apiKeyHint,
          defaultValue: '',
          attributes: { placeholder: t.apiKeyPlaceholder, autocapitalize: 'off', autocorrect: 'off' }
        }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.display },
        {
          type: 'select',
          messageKey: 'CfgLang',
          label: t.language,
          defaultValue: 'auto',
          options: [
            { label: t.langAuto, value: 'auto' },
            { label: 'Deutsch', value: 'de' },
            { label: 'English', value: 'en' }
          ]
        },
        {
          type: 'slider',
          messageKey: 'CfgCount',
          label: t.count,
          defaultValue: 15,
          min: 5,
          max: 25,
          step: 1
        }
      ]
    },
    { type: 'submit', defaultValue: t.save }
  ].concat(supportSection(t));
};
