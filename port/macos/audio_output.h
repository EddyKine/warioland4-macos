#ifndef WL4_DARWIN_AUDIO_OUTPUT_H
#define WL4_DARWIN_AUDIO_OUTPUT_H
#include <SDL3/SDL.h>
#include <stdatomic.h>
typedef struct DarwinAudioOutput {
 SDL_AudioStream *stream;
 unsigned rate,target,refills,drops,errors;
 float average,ratio;
 int primed,device,fast,fadeIn;
 int16_t tail[2];
 atomic_uint shortfallRequests,shortfallBytes;
} DarwinAudioOutput;
void DarwinAudioOutputInit(DarwinAudioOutput *,SDL_AudioStream *,unsigned,int);
void DarwinAudioOutputPush(DarwinAudioOutput *,const int16_t *,size_t);
void DarwinAudioOutputTick(DarwinAudioOutput *,int);
void DarwinAudioOutputFillDemand(DarwinAudioOutput *,int);
void DarwinAudioOutputDetach(DarwinAudioOutput *);
#endif
