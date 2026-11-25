# ST2000 Remote Capacitor App

Packages the ESP32 web UI (`../../data/index.html`) as a phone app. The firmware
files in `../../data/` remain the single source of truth.

```text
../../data/  -> npm run sync -> src/ and src/public/ -> vite build -> dist/ -> npx cap sync
```

`sync-data.mjs` copies `index.html` and its assets (skipping the firmware menu,
Wi-Fi manager, setup and display pages). It adds app-only files from `app-scripts/`:

- `device-config.js` rewrites `fetch()`, XHR and `EventSource` requests such as
  `/events`, `/SendCommand.html`, `/trollmode` and `/n2k.json` to the configured ESP32.
- `app-menu.js` / `app-menu.css` replace the firmware hamburger menu (Home, Settings,
  ESP32 host info).
- `settings.html` / `app-settings.js` set the ESP32 hostname or IP, with a Test button
  that calls `/host`. The value is stored in `localStorage`; default `ESPmcu.local`.

## ESP32 requirements

No new endpoints are needed. The firmware already sends `Access-Control-Allow-Origin: *`
through `DefaultHeaders` in `src/webserver.cpp`, which the app (origin `https://localhost`)
requires. The app uses `/events`, `/SendCommand.html`, `/trollmode`, `/n2k.json`,
`/clienttime` and `/host`.

## Build

```bash
cd capacitor/stw-remote-app
npm install
npm run build          # web bundle
npm run cap:sync       # build + copy into android/ and ios/
npm run build:android  # debug APK, copied to ~/Dropbox/stw-remote-debug.apk
```

Both native projects allow cleartext HTTP because the ESP32 serves plain HTTP.
iOS builds need Xcode on macOS. Generated `src/` and `dist/` are git-ignored.
The app uses the default Capacitor icon; add `assets/icon.png` and run
`npx @capacitor/assets generate` to customize.
