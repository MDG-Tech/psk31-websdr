import {codes} from './varicode.mjs';
export const messages=['CQ CQ DE TEST1 PSK31 browser decoder test\n','HELLO FROM TEST2 automatic signal search\n'];
export function synthesize(message,hz,seconds=24,drift=0){
 const bits='0'.repeat(64)+[...message].map(c=>codes[c.charCodeAt(0)]+'00').join('')+'0'.repeat(1000);
 const result=new Float32Array(seconds*8000);let previous=1,level=1;
 for(let k=0;k<result.length;k++){
  const symbol=Math.floor(k/256),part=(k%256)/256;
  if(k%256===0){previous=level;if(bits[symbol]==='0')level=-level;}
  const envelope=previous+(level-previous)*(.5-.5*Math.cos(Math.PI*part)),t=k/8000;
  result[k]=.28*envelope*Math.cos(2*Math.PI*(hz*t+.5*drift*t*t));
 }return result;
}
export function demo(){const a=synthesize(messages[0],720,24,.12),b=synthesize(messages[1],1720,24,-.08);let seed=42;for(let i=0;i<a.length;i++){seed=(1664525*seed+1013904223)>>>0;a[i]+=b[i]+.025*(seed/4294967296-.5);}return a;}
