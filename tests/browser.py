"""Run with the static server on localhost:8092 and Playwright installed."""
import os
from pathlib import Path
import subprocess
import tempfile
import wave
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
raw = subprocess.check_output(['node', '--input-type=module', '-e',
    "import {demo} from './demo.mjs'; const a=demo(),b=Buffer.alloc(a.length*4); for(let i=0;i<a.length;i++){const v=Math.round(a[i]*30000);b.writeInt16LE(v,i*4);b.writeInt16LE(v,i*4+2);}process.stdout.write(b);"], cwd=ROOT)
with tempfile.TemporaryDirectory() as tmp:
    wav = Path(tmp) / 'two-carriers-16k.wav'
    with wave.open(str(wav), 'wb') as output:
        output.setparams((1, 2, 16000, 0, 'NONE', 'not compressed'))
        output.writeframes(raw)
    with sync_playwright() as p:
        cached = sorted(Path('/root/.cache/ms-playwright').glob('chromium_headless_shell-*/chrome-headless-shell-linux64/chrome-headless-shell'))
        executable = os.environ.get('PLAYWRIGHT_CHROMIUM_EXECUTABLE') or (str(cached[-1]) if cached else None)
        browser = p.chromium.launch(headless=True, executable_path=executable, args=['--no-sandbox'])
        page = browser.new_page(viewport={'width':1280,'height':1100})
        errors, external = [], []
        page.on('pageerror', lambda error: errors.append(str(error)))
        page.on('request', lambda request: external.append(request.url) if not request.url.startswith('http://127.0.0.1:8092/') and not request.url.startswith('blob:') else None)
        page.goto('http://127.0.0.1:8092/')
        page.get_by_role('button', name='Try two-signal demo').click()
        page.wait_for_function("document.querySelector('#status').textContent==='Recording search complete'")
        assert page.locator('.signal').count() == 2
        assert 'CQ CQ DE TEST1' in page.locator('#signals').inner_text()
        assert 'HELLO FROM TEST2' in page.locator('#signals').inner_text()
        with page.expect_download() as info:
            page.get_by_role('button', name='Save text').click()
        download = info.value
        download.save_as(Path(tmp)/'decoded.txt')
        assert 'HELLO FROM TEST2' in (Path(tmp)/'decoded.txt').read_text()
        page.get_by_role('button', name='Clear', exact=True).click()
        assert page.locator('.signal').count() == 0
        page.locator('#file').set_input_files(str(wav))
        page.wait_for_function("document.querySelectorAll('.signal').length===2 && document.querySelector('#status').textContent==='Recording search complete'")
        assert 'CQ CQ DE TEST1' in page.locator('#signals').inner_text()
        page.screenshot(path='/tmp/psk31-websdr-desktop.png', full_page=True)
        page.set_viewport_size({'width':390,'height':844})
        assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
        page.screenshot(path='/tmp/psk31-websdr-mobile.png', full_page=True)
        # Capture without supplied audio must fail clearly and release display tracks.
        page.evaluate("""() => { window.testTrack=null; navigator.mediaDevices.getDisplayMedia=async()=>{const s=document.createElement('canvas').captureStream();window.testTrack=s.getVideoTracks()[0];return s;}; }""")
        page.get_by_role('button', name='Share WebSDR tab audio').click()
        page.wait_for_function("document.querySelector('#status').textContent.includes('No audio was shared')")
        assert page.evaluate("testTrack.readyState==='ended'")
        # Exercise the real AudioWorklet and browser audio path with a synthetic input.
        page.evaluate("""async () => {const {demo}=await import('./demo.mjs');window.testContext=new AudioContext({sampleRate:8000});const a=demo(),b=testContext.createBuffer(1,a.length,8000);b.copyToChannel(a,0);const s=testContext.createBufferSource();s.buffer=b;s.loop=true;const d=testContext.createMediaStreamDestination();s.connect(d);s.start();window.testAudio=d.stream; navigator.mediaDevices.getUserMedia=async()=>testAudio; }""")
        page.get_by_role('button', name='Clear', exact=True).click()
        page.get_by_role('button', name='Audio input', exact=True).click()
        page.wait_for_function("document.querySelector('#progress').textContent.includes('Live audio')", timeout=45000)
        assert 'CQ CQ DE TEST1' in page.locator('#signals').inner_text()
        page.get_by_role('button', name='Stop', exact=True).click()
        page.wait_for_function("document.querySelector('#status').textContent==='Stopped'")
        assert page.evaluate("testAudio.getTracks().every(t=>t.readyState==='ended')")
        page.evaluate('testContext.close()')
        assert not errors, errors
        assert not external, external
        browser.close()
print('PASS: browser worker/WASM, demo, 16 kHz WAV, export, layouts, capture errors, live AudioWorklet, stop and local-only requests')
