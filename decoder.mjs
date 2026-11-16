export function readWav(buffer) {
  const v=new DataView(buffer),str=(o,n)=>String.fromCharCode(...new Uint8Array(buffer,o,n));
  if(v.byteLength<44||str(0,4)!=='RIFF'||str(8,4)!=='WAVE')throw Error('Choose a PCM WAV file.');
  let format=null,data=null;
  for(let p=12;p+8<=v.byteLength;){
    const name=str(p,4),size=v.getUint32(p+4,true),start=p+8;
    if(name==='fmt '&&size>=16)format={code:v.getUint16(start,true),channels:v.getUint16(start+2,true),rate:v.getUint32(start+4,true),bits:v.getUint16(start+14,true)};
    if(name==='data')data={start,length:Math.min(size,v.byteLength-start)};
    p=start+size+(size&1);
  }
  if(!format||!data||![1,3].includes(format.code)||!([16,32].includes(format.bits))||format.channels<1||format.channels>2)throw Error('Supported WAV: mono/stereo PCM16 or float32.');
  if(format.code===3&&format.bits!==32)throw Error('Invalid floating point WAV.');
  const bytes=format.bits/8,frames=Math.floor(data.length/(bytes*format.channels)),samples=new Float32Array(frames);
  for(let i=0;i<frames;i++){
    let sum=0;for(let c=0;c<format.channels;c++){
      const o=data.start+(i*format.channels+c)*bytes;
      sum+=format.code===3?v.getFloat32(o,true):format.bits===16?v.getInt16(o,true)/32768:v.getInt32(o,true)/2147483648;
    }samples[i]=sum/format.channels;
  }
  if(format.rate===8000)return {samples,rate:8000};
  throw Error(`Audio is ${format.rate} Hz. Use the WebSDR 8 kHz recording.`);
}

export async function createDecoder(wasmBytes) {
  const {instance}=await WebAssembly.instantiate(wasmBytes,{wasi_snapshot_preview1:{proc_exit:code=>{throw Error(`Decoder exit ${code}`);}}});
  const e=instance.exports;if(e._initialize)e._initialize();
  const stringAt=p=>{const b=new Uint8Array(e.memory.buffer);let end=p;while(b[end])end++;return new TextDecoder().decode(b.subarray(p,end));};
  return {
    decode(buffer,options={}) {
      const {samples,rate}=buffer instanceof Float32Array ? {samples:buffer,rate:8000} : readWav(buffer);
      if(samples.length>rate*360)throw Error('Recording exceeds six minutes.');
      const p=e.malloc(samples.byteLength);if(!p)throw Error('Not enough decoder memory.');
      try {
        new Float32Array(e.memory.buffer,p,samples.length).set(samples);
        const n=e.psk_decode(p,samples.length,rate,options.centerHz??1000,options.searchHz??300,options.start??0,options.end??0);
        if(n<0)throw Error(stringAt(e.psk_error()));
        const names=['cn0DbHz','snr31Db','snr50Db','startCarrierHz','endCarrierHz','fitRmsHz','carrierWindows','timingSample','start','end','driftHzPerSecond','curvature'];
        const metrics=Object.fromEntries(names.map((name,i)=>[name,e.psk_metric(i)]));
        const text=new TextDecoder().decode(new Uint8Array(e.memory.buffer,e.psk_text(),e.psk_text_length()));
        return {text,metrics,decoder:'WebAssembly BPSK31',sampleRate:rate};
      } finally {e.free(p);}
    }
  };
}
