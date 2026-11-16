# QO-100 browser PSK31 monitor

A static, receive-only application extracted from the local QO-100 WebSDR decoder. No Python decoder, SDR hardware, transmitter, cloud service, API keys or package installation is needed. WebAssembly DSP and automatic multi-signal search run in a browser worker. Audio and decoded text are not uploaded.

## Run

```sh
cd /root/psk31-websdr
python3 -m http.server 8092 --bind 127.0.0.1
```

Open http://localhost:8092 in a current desktop Chromium browser. Use localhost or HTTPS: audio capture is unavailable on ordinary remote HTTP origins. The Python command only serves static files; any static HTTPS host works. Opening index.html as a file does not work because module/worker/WASM loading needs HTTP.

1. Try **two-signal demo** for a reproducible local check.
2. For live reception, open https://eshail.batc.org.uk/nb/ in another tab, start its audio, select USB and tune/widen the filter to include the desired digital signals.
3. Choose **Share WebSDR tab audio**, select that tab and enable **Share tab audio**. The display stream is used solely to obtain audio; video is never processed or saved. No audio is played back by this app, preventing feedback.
4. Alternatively choose **Audio input** for a sound-card/loopback input, or load a mono/stereo WAV recording. Audio input uses the default device selected in your browser/OS. Browser tab audio capture support varies; mobile browsers, Firefox and Safari may need loopback audio instead.
5. Wait about 24 seconds for acquisition. Text appears separately for each detected audio carrier. Save text exports the current results. Stop releases capture tracks, closes audio and terminates DSP.

## What “automatic search” covers

Every analysis window is searched automatically; no carrier clicks, frequency guesses or known message are needed. The detector examines 150–3500 Hz of **received audio**, ranks spectral peaks and attempts independent BPSK31 decodes on up to 32 separated candidates. All candidates within that limit are processed. The waterfall shows audio frequency, not satellite RF. Tune the WebSDR itself to change the RF coverage.

**The BATC public WebSDR supplies a tuned audio channel, not whole-transponder IQ. This app cannot autonomously tune a different-origin WebSDR tab or receive every transmission across the satellite simultaneously.** It finds multiple PSK31 transmissions only when they fit inside the source's audio passband. There is no hidden server, proxy, extension, private WebSDR protocol or swarm of connections. Whole-transponder automatic reception would require a browser-accessible wideband IQ feed and a different channelizer.

Supported: differential **BPSK31**, 31.25 baud, ASCII varicode, approximately 150–3500 Hz audio carriers. WAV inputs are decoded by the browser and resampled to 8 kHz; live audio is resampled by Web Audio to 8 kHz. QPSK31/FEC, PSK63/125, RTTY, FT8, DVB and RF/IQ input are not supported. No RF testing or transmission is performed.

Windows are 24 seconds with four seconds of overlap; the last file window may be shorter (minimum eight seconds). Outputs are labeled snapshots; overlapping text can repeat. Each carrier retains its latest 30 snapshots until Clear. This avoids silently deleting legitimate repeated messages. A busy worker skips intermediate live analysis times rather than building an unbounded queue. Long recordings are limited to six minutes / 100 MB, and files are processed in bounded windows. Short bursts, carriers less than about 65 Hz apart, strong nearby interference, large/irregular drift and weak signals can fail. PSK31 has no checksum: readable-looking output is not proof of a valid transmission, and the signal/fit thresholds are heuristics, not modulation classification guarantees.

## Processing

- Main thread: permissions, audio routing, UI, text export and waterfall.
- AudioWorklet: forwards mono 8 kHz PCM without microphone speech processing requested.
- Worker: FFT candidate detection and WebAssembly decode for each candidate.
- WASM: mixing/filtering, squared-carrier estimation, polynomial drift fit, differential symbols and varicode. No message-assisted correction.
- Server: static HTML, JavaScript and WASM files only.

## Verify and rebuild

The compiled decoder.wasm is included. Rebuild with Emscripten installed:

```sh
sh build_decoder.sh
node tests/decoder.mjs
```

Tests cover simultaneous messages, noise, carrier drift, low/high audio carriers, silence and noise-only rejection. `tests/browser.py` additionally checks the real worker/WASM path, WAV resampling, UI, export and absence of external requests; it requires Python Playwright and a Chromium installation. Set PLAYWRIGHT_CHROMIUM_EXECUTABLE if needed. Start the static server first, then run it against http://127.0.0.1:8092.

## Origin and licensing

Decoder source, wrapper and build recipe are derived from `/root/pyespargos/demos/iq/transmit-beamforming/web/{decoder.c,decoder.mjs,build_decoder.sh}` (parent checkout f69d2f9; local source may contain uncommitted changes). New code adds independent automatic detection, local capture and UI. This repository is distributed under LGPL-3.0-or-later, matching that checkout. See LICENSE and COPYING. Full editable decoder source and rebuild instructions are included. Varicode is the ASCII PSK31 table described by Peter Martinez G3PLX, QEX July/August 1999. No BATC/WebSDR implementation or assets are bundled.
