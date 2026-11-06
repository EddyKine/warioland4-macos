#include "audio_output.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
static int failures;
#define CHECK(x,label) do {if(!(x)){fprintf(stderr,"FAIL: %s (%s)\n",label,SDL_GetError());failures++;}}while(0)
int main(void) {
 const SDL_AudioSpec format={SDL_AUDIO_S16,2,65536};
 SDL_AudioStream *stream=SDL_CreateAudioStream(&format,&format);
 CHECK(stream!=NULL,"create isolated stream without device");if(!stream)return 1;
 DarwinAudioOutput o;DarwinAudioOutputInit(&o,stream,65536,0);
 int16_t tone[8192*2],out[8192*2];
 for(unsigned i=0;i<8192;i++)tone[i*2]=tone[i*2+1]=(int16_t)(12000*sin(i*0.071));
 DarwinAudioOutputPush(&o,tone,7000);DarwinAudioOutputTick(&o,0);
 CHECK(o.primed&&o.refills==1,"startup primes from real samples only");
 CHECK(SDL_GetAudioStreamQueued(stream)==28000,"startup adds no silence");
 CHECK(SDL_GetAudioStreamData(stream,out,6000*4)==6000*4,"consume source to below former refill threshold");
 int before=SDL_GetAudioStreamQueued(stream);
 CHECK(before>0&&before<4*(65536/15)/2,"fixture is low but not empty");
 DarwinAudioOutputTick(&o,0);
 CHECK(SDL_GetAudioStreamQueued(stream)==before,"healthy low-water does not insert zero samples");
 CHECK(o.refills==1,"low-water is not counted as a re-prime");
 float prior=o.ratio;
 for(unsigned i=0;i<100;i++){
  DarwinAudioOutputTick(&o,0);
  CHECK(fabsf(o.ratio-prior)<0.00011f,"frequency ratio changes smoothly");
  CHECK(o.ratio>=0.995f&&o.ratio<=1.005f,"frequency correction stays bounded");prior=o.ratio;
 }
 DarwinAudioOutputTick(&o,1);
 CHECK(!o.primed&&SDL_GetAudioStreamQueued(stream)==0,"fast forward pauses and clears once");
 DarwinAudioOutputPush(&o,tone,1000);
 CHECK(SDL_GetAudioStreamQueued(stream)==0,"fast forward does not retain skipped sound");
 DarwinAudioOutputTick(&o,0);DarwinAudioOutputPush(&o,tone,7000);DarwinAudioOutputTick(&o,0);
 CHECK(o.primed&&o.refills==2,"normal playback resumes after real-sample pre-roll");
 CHECK(!o.errors,"no SDL API errors");
 SDL_ClearAudioStream(stream);SDL_SetAudioStreamFrequencyRatio(stream,1);
 for(unsigned i=0;i<8192*2;i++)tone[i]=12000;
 o.fadeIn=0;DarwinAudioOutputPush(&o,tone,128);
 CHECK(SDL_GetAudioStreamData(stream,out,128*4)==128*4,"consume fixture tail");
 DarwinAudioOutputFillDemand(&o,256*4);
 CHECK(SDL_GetAudioStreamData(stream,out,256*4)==256*4,"fill exact device shortfall only");
 CHECK(out[0]>11000&&out[63*2]==0&&out[255*2]==0,"starvation tail fades to zero");
 int largest=0;
 for(unsigned i=1;i<256;i++){int d=abs(out[i*2]-out[(i-1)*2]);if(d>largest)largest=d;}
 CHECK(largest<=188,"shortfall ramp avoids a 12000-unit hard cut");
 DarwinAudioOutputPush(&o,tone,128);
 CHECK(SDL_GetAudioStreamData(stream,out,128*4)==128*4,"resume real source frames");
 CHECK(out[0]<100&&out[127*2]==12000,"real source fades back without a zero-to-full jump");
 CHECK(atomic_load(&o.shortfallRequests)==1,"demand callback accounting is explicit");
 printf("Simulated starvation cut: old jump=12000, new max adjacent step=%d PCM units.\n",largest);
 SDL_DestroyAudioStream(stream);
 /* Contrast the old policy's discontinuity at an arbitrary low-water musical phase. */
 unsigned oldZeros=(65536/15)-1000;
 printf("Old low-water policy would append %u zero frames (~%.1f ms); new policy appends 0.\n",oldZeros,oldZeros*1000.0/65536);
 printf("Audio queue continuity checks: %s\n",failures?"FAIL":"PASS");return failures?1:0;
}
