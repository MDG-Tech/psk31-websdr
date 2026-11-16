# Validation

Validated locally on 2026-11-16 with Node.js and headless Chromium/Playwright.

- `sh build_decoder.sh`: compiled the included C source with Emscripten.
- `node tests/decoder.mjs`: two simultaneous messages at 720 and 1720 Hz with +0.12 / −0.08 Hz/s drift decoded exactly; 200 and 3100 Hz carriers decoded; silence and deterministic noise produced no decoded signals.
- `tests/browser.py`: the actual static page loaded its worker and WASM; demo decoded both messages; a 16 kHz WAV was resampled and decoded; exported text included both messages; desktop and 390 px layouts passed; missing shared audio produced a clear error and released the video track; a synthetic live MediaStream passed through the real AudioContext/AudioWorklet and produced decoded text; Stop ended its audio tracks; no browser page errors or external network requests occurred.

Live reception from a human-selected BATC tab and hardware loopback input were not tested in the automated environment. The synthetic live test validates the capture-processing path, not public WebSDR tab permission behavior or reception performance. There was no RF transmission and no modification of the existing receiver/transmitter projects.

Browser screenshots from validation were written to `/tmp/psk31-websdr-desktop.png` and `/tmp/psk31-websdr-mobile.png`; these temporary files are not required by the application.
