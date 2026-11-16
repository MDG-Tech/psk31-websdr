class Capture extends AudioWorkletProcessor {
 constructor(){super();this.buffer=new Float32Array(4096);this.used=0;this.phase=0;this.sum=0;this.count=0;}
 process(inputs){const channels=inputs[0];if(!channels?.length)return true;
  for(let i=0;i<channels[0].length;i++){let x=0;for(const c of channels)x+=c[i]/channels.length;this.sum+=x;this.count++;this.phase+=8000;
   if(this.phase>=sampleRate){this.phase-=sampleRate;this.buffer[this.used++]=this.sum/this.count;this.sum=0;this.count=0;
    if(this.used===4096){this.port.postMessage(this.buffer,[this.buffer.buffer]);this.buffer=new Float32Array(4096);this.used=0;}}
  }return true;
 }
}
registerProcessor('capture',Capture);
