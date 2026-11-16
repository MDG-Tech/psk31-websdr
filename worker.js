import {createDecoder} from './decoder.mjs';
import {scan,spectrum} from './scanner.mjs';
const ready=fetch('./decoder.wasm').then(r=>{if(!r.ok)throw Error('Cannot load decoder.wasm');return r.arrayBuffer();}).then(createDecoder);
self.onmessage=async({data})=>{if(data.type==='spectrum'){self.postMessage({type:'spectrum',db:spectrum(data.samples)});return;}try{const decoder=await ready;self.postMessage({id:data.id,...scan(data.samples,decoder)});}catch(e){self.postMessage({id:data.id,error:e.message});}};
