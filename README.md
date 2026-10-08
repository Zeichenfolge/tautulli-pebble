# Tautulli for Pebble

Your [Tautulli](https://tautulli.com/) / Plex activity on your wrist.
See who is watching what right now, stop a stream with one button, and browse
history, new media, users, libraries and statistics – on a Pebble Time 2.

![Screenshots](docs/screenshots.png)

*Screenshots from a Pebble Time 2 in demo mode with made-up data.*

[Deutsche Version](README.de.md) · [Changelog](CHANGELOG.md) · [Contributing](CONTRIBUTING.md)

If you enjoy the app, you can [buy me a coffee](https://buymeacoffee.com/michaelrunge) ☕ –
it stays free and open source either way.

## Features

- **Now playing** – current streams with user, episode/year and a progress bar.
  The header shows whether Plex is reachable, the number of streams and the total bandwidth.
  Refreshes every 30 seconds while open. Progress as percent, time left (“14 min left”)
  or end time (“ends 21:45”) – your choice in the settings.
- **Stream details** – device, time, quality (Direct Play / Transcode), bandwidth, LAN/WAN.
- **Stop a stream** – red stop button next to SELECT, with a confirmation.
  The viewer sees “The stream has been stopped.” (requires Plex Pass).
- **History** – recently watched, with user, time and progress.
- **Recently added** – new movies, episodes, seasons and albums.
- **Users** – who watched last and how much. Open a person to see their watch time
  (today / 7 / 30 days), their own plays-per-day chart and their history.
- **Watched to the end** – the watch vibrates and shows “Emma finished Harbor Patrol S02E05”
  when a stream ends after at least 90 % (also when autoplay starts the next episode).
  Works while the app is open; for notifications with the app closed see
  [Notifications with the app closed](#notifications-with-the-app-closed-ntfy).
- **Statistics** – watch time today / 7 / 30 days, top users, top movies and shows.
- **Chart** – plays per day as stacked bars (shows, movies, music), 7 or 30 days.
- **Libraries** – number of movies, shows/episodes, artists/albums per library.
- **German and English** – watch, messages and settings page.
- **Demo mode** – enter `demo` as address to try the app without a server.

## Installation

1. Install the app on your watch (Pebble appstore, the `.pbw` from the
   [latest release](../../releases/latest), or build it yourself – see below).
2. In Tautulli, copy your API key: *Settings → Web Interface → API*.
3. In the Pebble app on your phone, open **Tautulli → Settings** and enter:
   - **Tautulli address**, e.g. `http://192.168.1.10:8181`
   - **API key**
   - optionally the **language**, how **progress** is shown (percent, time left, end time),
     whether the watch should **vibrate** when something was watched to the end,
     and how many entries the lists show

The watch never talks to your server directly – your phone does. With a local
address the app therefore only works while your phone is on your home Wi-Fi.
For use on the go you need an address that is reachable from outside (e.g. a reverse proxy with HTTPS).

## Notifications with the app closed (ntfy)

A Pebble app only runs while it is open on the watch. If you want a buzz when
something was watched to the end even with the app closed, let Tautulli send it –
your Pebble shows every phone notification anyway:

1. Install the **ntfy** app on your phone and subscribe to a topic with a hard-to-guess name,
   e.g. `plex-michael-7f3k`. In the Pebble app, allow notifications from ntfy.
2. In Tautulli: *Settings → Notification Agents → Add a new notification agent → ntfy*.
   - **Configuration:** ntfy Host Address `https://ntfy.sh` (or your own ntfy server), ntfy Topic as above.
   - **Triggers:** tick **Watched** (optionally also *Playback Start* or *Playback Stop*).
   - **Text:** e.g. subject `Plex`, body `{user} watched {title}`.
3. Test it with *Test Notifications* at the bottom of the agent.

When a stream counts as “watched” is set in *Settings → General → Watched Percent*.

## Buttons

| Button | Action |
|---|---|
| UP / DOWN | scroll |
| SELECT | open |
| long SELECT | reload |
| SELECT in stream details | stop stream (with confirmation) |
| SELECT in the chart | switch 7 ↔ 30 days |
| BACK | back |

## How it works

PebbleKit JS on the phone calls the Tautulli API v2, prepares all texts in the selected
language and sends them to the watch via AppMessage. The watch only draws lists, details and the chart.

Tautulli commands used: `server_status`, `get_activity`, `get_history`, `get_recently_added`,
`get_home_stats`, `get_users_table`, `get_user_watch_time_stats`, `get_libraries_table`, `get_plays_by_date`,
`terminate_session` (POST).

## Building

```bash
# Linux / macOS / Windows (WSL) – see https://developer.repebble.com/sdk/
uv tool install pebble-tool --python 3.13
pebble sdk install latest

pebble build
pebble install --emulator emery     # emulator (settings: pebble emu-app-config)
pebble install --cloudpebble        # your watch (enable Dev Connect in the Pebble app)
```

Every push and release is also built automatically by GitHub Actions; the `.pbw`
is attached to each release.

## Project layout

| Path | Runs on | Purpose |
|---|---|---|
| `src/c/tautulli.c` | watch | lists, details, stop button with confirmation, message handling |
| `src/c/chart.c` | watch | bar chart |
| `src/c/i18n.c` | watch | German/English texts |
| `src/pkjs/index.js` | phone | Tautulli API, texts, formatting |
| `src/pkjs/demo.js` | phone | sample data for demo mode |
| `src/pkjs/config.js` | phone | settings page (Clay) |
| `resources/images/` | watch | app icon, stop and check icons |

## How this was made

This app was developed with the help of an AI assistant (Claude by Anthropic).
I review, test and maintain it myself.

## Contributing

Bug reports, ideas and pull requests are welcome – see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE) © 2026 Michael Runge.

Not affiliated with Tautulli, Plex or Core Devices.
