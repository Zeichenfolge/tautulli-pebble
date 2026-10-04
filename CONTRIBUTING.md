# Contributing

Thanks for helping! Bug reports and ideas are just as welcome as code.
German or English – both are fine.

## Reporting bugs and ideas

Use the [issue templates](../../issues/new/choose). For bugs, please include your
watch model, phone (iOS/Android), app version and Tautulli version.
Logs help a lot: connect the watch (Dev Connect) and run `pebble logs --cloudpebble`.

**Never post your API key or your server address** in an issue.

## Development setup

```bash
uv tool install pebble-tool --python 3.13
pebble sdk install latest
pebble build
pebble install --emulator emery
pebble emu-app-config        # opens the settings page for the emulator
```

Tip: enter `demo` as Tautulli address to work without a server.

## Things to know

- **Protocol:** message types, list ids, item kinds and commands are defined twice –
  at the top of `src/c/tautulli.c` (watch) and at the top of `src/pkjs/index.js` (phone).
  New AppMessage keys also go into `messageKeys` in `package.json`.
- **Phone code is ES5.** The Pebble SDK bundles PebbleKit JS with webpack 1, and the
  iOS app runs it in plain JavaScriptCore. Please avoid `let`/`const`, arrow functions,
  classes, `async` and locale functions like `localeCompare` (the emulator's JS engine lacks them).
- **Texts:** the phone sends ready-made texts. Watch-only texts live in `src/c/i18n.c`.
- **Adding a language:** `src/c/i18n.c` (watch), `TEXT` in `src/pkjs/index.js` (messages),
  `TEXT` in `src/pkjs/config.js` (settings page) and the language list in `config.js`.
- **Demo data:** if you add an API call, please add a matching answer in `src/pkjs/demo.js`.

## Pull requests

- One topic per pull request
- `pebble build` must pass without warnings (CI checks this)
- Test on the emulator (`emery`) and describe what you tested
- Screenshots for UI changes are great – please use demo mode, not your own server

AI-assisted contributions are welcome too – please test them yourself and mention it in the pull request.
