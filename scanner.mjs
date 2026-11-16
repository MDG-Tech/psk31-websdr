export function spectrum(samples) {
 const n=4096,r=new Float64Array(n),im=new Float64Array(n),out=new Float64Array(n/2);
 let windows=0;
 for(let offset=0;offset+n<=samples.length;offset+=Math.max(n,Math.floor(samples.length/12/n)*n)){
  for(let i=0;i<n;i++){r[i]=samples[offset+i]*(.5-.5*Math.cos(2*Math.PI*i/(n-1)));im[i]=0;}
  for(let i=1,j=0;i<n;i++){let b=n>>1;for(;j&b;b>>=1)j^=b;j^=b;if(i<j){const v=r[i];r[i]=r[j];r[j]=v;}}
  for(let len=2;len<=n;len*=2)for(let k=0;k<n;k+=len)for(let j=0;j<len/2;j++){
   const angle=-2*Math.PI*j/len,c=Math.cos(angle),s=Math.sin(angle),a=k+j,b=a+len/2,tr=r[b]*c-im[b]*s,ti=r[b]*s+im[b]*c;
   r[b]=r[a]-tr;im[b]=im[a]-ti;r[a]+=tr;im[a]+=ti;
  }
  for(let i=0;i<out.length;i++)out[i]+=r[i]*r[i]+im[i]*im[i];windows++;
 }
 return Float32Array.from(out,x=>10*Math.log10(x/Math.max(windows,1)+1e-20));
}
export function candidates(db) {
 const smooth=Array.from(db,(_,i)=>{let v=0;for(let j=-10;j<=10;j++)v+=10**((db[Math.max(0,Math.min(db.length-1,i+j))]||-200)/10);return 10*Math.log10(v/21+1e-20);});
 const floor=[...smooth.slice(77,1792)].sort((a,b)=>a-b)[857];
 const peaks=[];
 for(let i=77;i<1792;i++)if(smooth[i]>floor+7&&smooth[i]>=smooth[i-1]&&smooth[i]>smooth[i+1])peaks.push({hz:i*8000/4096,power:smooth[i]});
 const selected=[];for(const p of peaks.sort((a,b)=>b.power-a.power))if(!selected.some(q=>Math.abs(q.hz-p.hz)<65))selected.push(p);
 return {peaks:selected.slice(0,32).sort((a,b)=>a.hz-b.hz),truncated:selected.length>32};
}
export function scan(samples,decoder) {
 const db=spectrum(samples),found=candidates(db),signals=[];
 for(const p of found.peaks)try{
  const result=decoder.decode(samples,{centerHz:p.hz,searchHz:40});
  const m=result.metrics;
  if(result.text.trim().length>=3&&m.fitRmsHz<3&&m.snr31Db>0)signals.push({...result,hz:(m.startCarrierHz+m.endCarrierHz)/2});
 }catch{/* Noise, speech and other modulation are expected candidates. */}
 return {db,signals,candidates:found.peaks.length,truncated:found.truncated};
}
