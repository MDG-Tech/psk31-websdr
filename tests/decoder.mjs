import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createDecoder} from '../decoder.mjs';
import {scan} from '../scanner.mjs';
import {demo,messages,synthesize} from '../demo.mjs';
const decoder=await createDecoder(readFileSync(new URL('../decoder.wasm',import.meta.url)));
const result=scan(demo(),decoder);
console.log(result.signals.map(s=>({hz:s.hz,text:s.text,fit:s.metrics.fitRmsHz})));
assert.equal(result.signals.length,2);
for(const message of messages)assert(result.signals.some(s=>s.text.includes(message.trim())),message);
for(const hz of [200,3100]){const text='CQ TEST browser PSK31\n';assert(scan(synthesize(text,hz),decoder).signals.some(s=>s.text.includes(text.trim())),`carrier ${hz}`);}
assert.equal(scan(new Float32Array(24*8000),decoder).signals.length,0);
let seed=37;const noise=Float32Array.from({length:24*8000},()=>{seed=(1664525*seed+1013904223)>>>0;return seed/4294967296-.5;});assert.equal(scan(noise,decoder).signals.length,0);
console.log('PASS: simultaneous carriers, drift, search edges, silence and noise');
