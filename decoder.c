/* Receive-only BPSK31 decoder. All DSP runs locally in WebAssembly.
 * Carrier estimation squares complex audio to remove differential BPSK signs.
 * Neither carrier/timing recovery nor decoding uses the transmitted message.
 */
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
#define RATE 2000
#define FFT 2048
#define HOP 256
typedef struct { float r,i; } complex_t;
static char text[4096], error_text[160];
static int text_length;
static double metrics[12];
static const char *codes[128] = {
"1010101011","1011011011","1011101101","1101110111","1011101011","1101011111","1011101111","1011111101",
"1011111111","11101111","11101","1101101111","1011011101","11111","1101110101","1110101011",
"1011110111","1011110101","1110101101","1110101111","1101011011","1101101011","1101101101","1101010111",
"1101111011","1101111101","1110110111","1101010101","1101011101","1110111011","1011111011","1101111111",
"1","111111111","101011111","111110101","111011011","1011010101","1010111011","101111111",
"11111011","11110111","101101111","111011111","1110101","110101","1010111","110101111",
"10110111","10111101","11101101","11111111","101110111","101011011","101101011","110101101",
"110101011","110110111","11110101","110111101","111101101","1010101","111010111","1010101111",
"1010111101","1111101","11101011","10101101","10110101","1110111","11011011","11111101",
"101010101","1111111","111111101","101111101","11010111","10111011","11011101","10101011",
"11010101","111011101","10101111","1101111","1101101","101010111","110110101","101011101",
"101110101","101111011","1010101101","111110111","111101111","111111011","1010111111","101101101",
"1011011111","1011","1011111","101111","101101","11","111101","1011011",
"101011","1101","111101011","10111111","11011","111011","1111","111",
"111111","110111111","10101","10111","101","110111","1111011","1101011",
"11011111","1011101","111010101","1010110111","110111011","1010110101","1011010111","1110110101"
};

const char *psk_text(void) { return text; }
const char *psk_error(void) { return error_text; }
int psk_text_length(void) { return text_length; }
double psk_metric(int index) { return index>=0 && index<12 ? metrics[index] : NAN; }
static float power(complex_t a) { return a.r*a.r+a.i*a.i; }
static void fft(complex_t *a) {
    for (unsigned i=1,j=0;i<FFT;i++) {
        unsigned bit=FFT>>1;
        for (;j&bit;bit>>=1) j^=bit;
        j^=bit;
        if(i<j) { complex_t t=a[i];a[i]=a[j];a[j]=t; }
    }
    for (unsigned len=2;len<=FFT;len<<=1) {
        double angle=-2*PI/len;
        complex_t step={(float)cos(angle),(float)sin(angle)};
        for (unsigned i=0;i<FFT;i+=len) {
            complex_t w={1,0};
            for (unsigned j=0;j<len/2;j++) {
                complex_t u=a[i+j],b=a[i+j+len/2];
                complex_t v={b.r*w.r-b.i*w.i,b.r*w.i+b.i*w.r};
                a[i+j]=(complex_t){u.r+v.r,u.i+v.i};
                a[i+j+len/2]=(complex_t){u.r-v.r,u.i-v.i};
                w=(complex_t){w.r*step.r-w.i*step.i,w.r*step.i+w.i*step.r};
            }
        }
    }
}
static int solve(double a[3][4],double *x) {
    for(int k=0;k<3;k++) {
        int p=k;for(int j=k+1;j<3;j++)if(fabs(a[j][k])>fabs(a[p][k]))p=j;
        if(fabs(a[p][k])<1e-12)return 0;
        for(int j=k;j<4;j++){double t=a[k][j];a[k][j]=a[p][j];a[p][j]=t;}
        double v=a[k][k];for(int j=k;j<4;j++)a[k][j]/=v;
        for(int i=0;i<3;i++)if(i!=k){v=a[i][k];for(int j=k;j<4;j++)a[i][j]-=v*a[k][j];}
    }
    for(int k=0;k<3;k++)x[k]=a[k][3];return 1;
}
static double curve(const double *c,double t) { return c[0]+t*(c[1]+t*c[2]); }
static void emit_code(const char *code) {
    for(int i=0;i<128;i++) if(!strcmp(codes[i],code)) {
        if((i>=32 && i<=126)||i==10||i==13||i==9)
            if(text_length<(int)sizeof(text)-1)text[text_length++]=(char)i;
        return;
    }
}
int psk_decode(const float *pcm,int samples,int sample_rate,double center,double width,double begin,double end) {
    text_length=0;text[0]=0;error_text[0]=0;
    for(int i=0;i<12;i++)metrics[i]=NAN;
    if(!pcm || samples<sample_rate*2 || sample_rate!=8000 || samples>sample_rate*360 ||
       !isfinite(center)||!isfinite(width)||center<150||center>3500||width<40||width>400) {
        strcpy(error_text,"Expected 2–360 seconds of 8 kHz audio and a valid carrier search band");return -1;
    }
    int n=samples/4;
    complex_t *z=calloc(n,sizeof(*z)),*mixed=malloc(samples*sizeof(*mixed)),*filtered=calloc(n,sizeof(*filtered));
    complex_t *a=malloc(FFT*sizeof(*a));
    int max_windows=n/HOP+2;
    double *tx=calloc(max_windows,sizeof(double)),*fy=calloc(max_windows,sizeof(double)),*weight=calloc(max_windows,sizeof(double));
    if(!z||!mixed||!filtered||!a||!tx||!fy||!weight){strcpy(error_text,"Not enough decoder memory");goto fail;}
    for(int i=0;i<samples;i++) {
        double phase=2*PI*center*i/sample_rate;
        mixed[i]=(complex_t){pcm[i]*(float)cos(phase),-pcm[i]*(float)sin(phase)};
    }
    float taps[129];double sum=0;
    for(int j=-64;j<=64;j++) {
        double v=j?sin(2*PI*850*j/sample_rate)/(PI*j):2.*850/sample_rate;
        v*=.54+.46*cos(PI*j/64);taps[j+64]=v;sum+=v;
    }
    for(int j=0;j<129;j++)taps[j]/=sum;
    for(int i=0;i<n;i++)for(int j=-64;j<=64;j++) {
        int k=i*4+j;if(k<0||k>=samples)continue;
        z[i].r+=mixed[k].r*taps[j+64];z[i].i+=mixed[k].i*taps[j+64];
    }
    free(mixed);mixed=NULL;
    if(!isfinite(begin)||!isfinite(end)||begin<0||end<=begin){begin=0;end=(double)samples/sample_rate;}
    int lo=fmax(0,begin*RATE),hi=fmin(n,end*RATE);
    if(hi-lo<2*RATE){strcpy(error_text,"Selected frame is shorter than two seconds");goto fail;}
    double mid=(lo+hi)/(2.*RATE);int count=0;
    for(int offset=lo;offset+FFT<hi;offset+=HOP) {
        for(int j=0;j<FFT;j++) {
            complex_t v=z[offset+j];double w=.5-.5*cos(2*PI*j/(FFT-1));
            a[j]=(complex_t){(v.r*v.r-v.i*v.i)*w,2*v.r*v.i*w};
        }
        fft(a);int peak=0;double best=0,total=0;int bins=0;
        for(int j=0;j<FFT;j++) {
            double f=(j<=FFT/2?j:j-FFT)*(double)RATE/FFT;
            if(fabs(f)>2*width)continue;
            double p=power(a[j]);total+=p;bins++;if(p>best){best=p;peak=j;}
        }
        double strength=best/(total/fmax(bins,1)+1e-30);
        double p1=log(power(a[(peak+FFT-1)%FFT])+1e-30),p2=log(best+1e-30),p3=log(power(a[(peak+1)%FFT])+1e-30);
        double denominator=p1-2*p2+p3;
        double shift=denominator? .5*(p1-p3)/denominator:0;
        if(fabs(shift)>.5)shift=0;
        tx[count]=(offset+FFT/2.)/RATE-mid;
        fy[count]=((peak<=FFT/2?peak:peak-FFT)+shift)*(double)RATE/FFT/2;
        /* Random noise maxima in this search have a ratio around 7. */
        weight[count]=strength>12?fmin(strength,300):0;count++;
    }
    double c[3]={0},rms=0;int used=0;
    for(int iteration=0;iteration<4;iteration++) {
        double matrix[3][4]={{0}};used=0;
        for(int k=0;k<count;k++) {
            double w=weight[k];if(!w)continue;
            if(iteration && fabs(fy[k]-curve(c,tx[k]))>fmax(6,3*rms))continue;
            double q[3]={1,tx[k],tx[k]*tx[k]};
            for(int i=0;i<3;i++){for(int j=0;j<3;j++)matrix[i][j]+=w*q[i]*q[j];matrix[i][3]+=w*q[i]*fy[k];}used++;
        }
        if(used<4||!solve(matrix,c)){strcpy(error_text,"No consistent BPSK carrier found in the selected band");goto fail;}
        double squared=0,total_weight=0;
        for(int k=0;k<count;k++)if(weight[k]){double e=fy[k]-curve(c,tx[k]);if(iteration && fabs(e)>fmax(6,3*rms))continue;squared+=weight[k]*e*e;total_weight+=weight[k];}
        rms=sqrt(squared/fmax(total_weight,1));
    }
    for(int k=0;k<n;k++) {
        double t=(double)k/RATE-mid,phase=2*PI*(c[0]*t+c[1]*t*t/2+c[2]*t*t*t/3);
        float r=cos(phase),i=-sin(phase);complex_t v=z[k];z[k]=(complex_t){v.r*r-v.i*i,v.r*i+v.i*r};
    }
    /* Narrow receive filtering is essential: a few unfiltered peak samples
     * have much more noise bandwidth than a PSK31 symbol. Keep z unfiltered
     * for the independent adjacent-band noise/SNR measurement below. */
    sum=0;
    for(int j=-64;j<=64;j++) {
        double v=j?sin(2*PI*45*j/RATE)/(PI*j):2.*45/RATE;
        v*=.54+.46*cos(PI*j/64);taps[j+64]=v;sum+=v;
    }
    for(int j=0;j<129;j++)taps[j]/=sum;
    for(int k=0;k<n;k++)for(int j=-64;j<=64;j++) {
        int index=k+j;if(index<0||index>=n)continue;
        filtered[k].r+=z[index].r*taps[j+64];filtered[k].i+=z[index].i*taps[j+64];
    }
    /* Symbol peaks maximize envelope energy, independently of data polarity. */
    int timing=0;double best_score=-1;
    for(int p=0;p<64;p++) {
        double score=0;int number=0;
        for(int k=((lo+63)/64)*64+p;k<hi;k+=64){score+=power(filtered[k]);number++;}
        score/=fmax(number,1);if(score>best_score){best_score=score;timing=p;}
    }
    char code[16]={0};int length=0,zeros=0;bool have=false;complex_t previous={0};
    for(int k=((lo+63)/64)*64+timing;k<hi;k+=64) {
        complex_t v={0};int amount=0;
        for(int j=-2;j<=2;j++)if(k+j>=0&&k+j<n){v.r+=filtered[k+j].r;v.i+=filtered[k+j].i;amount++;}
        v.r/=amount;v.i/=amount;
        if(!have){previous=v;have=true;continue;}
        int bit=v.r*previous.r+v.i*previous.i>=0;previous=v;
        if(bit) {
            if(zeros==1 && length && length<14)code[length++]='0';
            if(length<14)code[length++]='1';else length=0;
            zeros=0;
        } else if(++zeros>=2) {
            if(length){code[length]=0;emit_code(code);}length=0;
        }
    }
    text[text_length]=0;
    /* Complex two-sided PSD: integrate signal, subtract local noise, then
     * report S/N in an explicit 31.25 Hz reference bandwidth. */
    double psd[FFT]={0},window_power=0;int windows=0;
    for(int j=0;j<FFT;j++){double w=.5-.5*cos(2*PI*j/(FFT-1));window_power+=w*w;}
    for(int offset=lo;offset+FFT<hi;offset+=FFT/2) {
        for(int j=0;j<FFT;j++){double w=.5-.5*cos(2*PI*j/(FFT-1));a[j]=(complex_t){z[offset+j].r*w,z[offset+j].i*w};}
        fft(a);for(int j=0;j<FFT;j++)psd[j]+=power(a[j])/(RATE*window_power);windows++;
    }
    double noise=0,total_signal=0;int noise_bins=0,signal_bins=0;
    for(int j=0;j<FFT;j++) {
        double f=fabs((j<=FFT/2?j:j-FFT)*(double)RATE/FFT),p=psd[j]/fmax(windows,1);
        if(f>=100&&f<=250){noise+=p;noise_bins++;}
        if(f<=40){total_signal+=p*RATE/FFT;signal_bins++;}
    }
    noise/=fmax(noise_bins,1);double signal=total_signal-noise*signal_bins*RATE/FFT;
    metrics[0]=noise>0&&signal>0?10*log10(signal/noise):NAN;
    metrics[1]=metrics[0]-10*log10(31.25);metrics[2]=metrics[0]-10*log10(50);
    metrics[3]=center+curve(c,lo/(double)RATE-mid);metrics[4]=center+curve(c,hi/(double)RATE-mid);
    metrics[5]=rms;metrics[6]=used;metrics[7]=timing;metrics[8]=begin;metrics[9]=end;
    metrics[10]=c[1];metrics[11]=c[2];
    free(z);free(filtered);free(a);free(tx);free(fy);free(weight);return text_length;
fail:
    free(z);free(mixed);free(filtered);free(a);free(tx);free(fy);free(weight);return -1;
}
