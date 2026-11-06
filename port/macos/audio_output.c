/* Keep source audio continuous: low-water is feedback, never a reason to append zeros. */
#include "audio_output.h"
#include <string.h>
/* Called with the stream lock held (including SDL's get callback). A genuine
 * short request gets only the missing frames, ramping the prior tail to zero. */
void DarwinAudioOutputFillDemand(DarwinAudioOutput *o,int additional) {
 if(additional<=0)return;
 atomic_fetch_add_explicit(&o->shortfallRequests,1,memory_order_relaxed);
 atomic_fetch_add_explicit(&o->shortfallBytes,(unsigned)additional,memory_order_relaxed);
 unsigned left=((unsigned)additional+3)/4,at=0;
 int16_t recovery[512*2];
 while(left){
  unsigned n=left>512?512:left;
  for(unsigned i=0;i<n;i++,at++)for(unsigned ch=0;ch<2;ch++)
   recovery[i*2+ch]=at<64?(int16_t)((int)o->tail[ch]*(63-(int)at)/64):0;
  if(!SDL_PutAudioStreamData(o->stream,recovery,(int)(n*4)))o->errors++;
  left-=n;
 }
 o->tail[0]=o->tail[1]=0;o->fadeIn=1;
}
static void SDLCALL NeedAudio(void *userdata,SDL_AudioStream *stream,int additional,int total) {
 (void)stream;(void)total;
 DarwinAudioOutputFillDemand(userdata,additional);
}
void DarwinAudioOutputInit(DarwinAudioOutput *o,SDL_AudioStream *stream,unsigned rate,int device) {
 memset(o,0,sizeof(*o));o->stream=stream;o->rate=rate;o->target=rate/15;
 o->average=(float)o->target;o->ratio=1;o->device=device;
 atomic_init(&o->shortfallRequests,0);atomic_init(&o->shortfallBytes,0);
 if(device && stream)SDL_SetAudioStreamGetCallback(stream,NeedAudio,o);
}
void DarwinAudioOutputPush(DarwinAudioOutput *o,const int16_t *samples,size_t n) {
 if(!o->stream||o->fast||!n)return;
 SDL_LockAudioStream(o->stream);
 size_t offset=0;
 if(o->fadeIn){
  int16_t ramp[128*2];size_t count=n<128?n:128;
  for(size_t i=0;i<count;i++)for(unsigned ch=0;ch<2;ch++)
   ramp[i*2+ch]=(int16_t)((int)samples[i*2+ch]*(int)(i+1)/(int)count);
  if(!SDL_PutAudioStreamData(o->stream,ramp,(int)(count*4)))o->errors++;
  offset=count;o->fadeIn=0;
 }
 if(offset<n&&!SDL_PutAudioStreamData(o->stream,samples+offset*2,(int)((n-offset)*4)))o->errors++;
 o->tail[0]=samples[(n-1)*2];o->tail[1]=samples[(n-1)*2+1];
 SDL_UnlockAudioStream(o->stream);
}
void DarwinAudioOutputTick(DarwinAudioOutput *o,int fast) {
 if(!o->stream)return;
 if(fast){
  if(!o->fast){
   if(o->device)SDL_PauseAudioStreamDevice(o->stream);
   if(!SDL_ClearAudioStream(o->stream))o->errors++;
   o->primed=0;o->ratio=1;o->average=(float)o->target;
   o->tail[0]=o->tail[1]=0;o->fadeIn=1;
  }
  o->fast=1;return;
 }
 o->fast=0;
 int bytes=SDL_GetAudioStreamQueued(o->stream);
 if(bytes<0){o->errors++;return;}
 unsigned queued=(unsigned)bytes/4;
 if(queued>4*o->target){
  if(o->device)SDL_PauseAudioStreamDevice(o->stream);
  SDL_ClearAudioStream(o->stream);o->primed=0;o->drops++;
  o->average=(float)o->target;o->ratio=1;return;
 }
 if(!o->primed){
  /* A real 67 ms pre-roll, while the device is paused, replaces silence insertion. */
  if(queued>=o->target){
   o->primed=1;o->average=(float)queued;o->refills++;
   if(o->device&&!SDL_ResumeAudioStreamDevice(o->stream))o->errors++;
  }
  return;
 }
 /* SDL consumes chunks, so raw queue length oscillates even with healthy audio.
  * Filter that sawtooth and slew the old +/-0.5% clock correction gradually. */
 o->average+=0.05f*((float)queued-o->average);
 float error=(o->average-o->target)/o->target;
 if(error>1)error=1;if(error< -1)error=-1;
 float wanted=1+0.005f*error;
 o->ratio+=0.02f*(wanted-o->ratio);
 if(!SDL_SetAudioStreamFrequencyRatio(o->stream,o->ratio))o->errors++;
}
void DarwinAudioOutputDetach(DarwinAudioOutput *o) {
 if(o->stream&&o->device)SDL_SetAudioStreamGetCallback(o->stream,NULL,NULL);
}
