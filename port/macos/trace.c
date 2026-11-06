/* Compact state evidence separates host input loss from a live but blocked player. */
#include "main.h"
#include "wario.h"
#include "input.h"
#include "global_data.h"
#include "port/frame.h"
#include "port/gba_io.h"
#include <SDL3/SDL.h>
#include <stdio.h>
void PortDarwinTraceFrame(unsigned raw,unsigned delivered,int focus,SDL_Gamepad *pad) {
 static int lastMode=-1,lastSub=-1,lastRoom=-1,lastFocus=-1;
 unsigned frame=PortFrameCount();
 int changed=lastMode!=gMainGameMode || lastSub!=gSubGameMode || lastRoom!=gCurrentRoom || lastFocus!=focus;
 if(changed || frame%120==0){
  printf("State: frame=%u step=%u mode=%d sub=%d passage=%u stage=%d room=%u pose=%u reaction=%u xy=%u,%u velocity=%d,%d raw=%03x sent=%03x gba=%03x focus=%d pad=%u connected=%d pause=%u disable=%u demo=%u\n",frame,PortFrameSteps(),gMainGameMode,gSubGameMode,gCurrentPassage,gCurrentStageID,gCurrentRoom,gWarioData.pose,gWarioData.reaction,gWarioData.xPosition,gWarioData.yPosition,gWarioData.xVelocity,gWarioData.yVelocity,raw,delivered,gButtonsHeld,focus,pad?SDL_GetGamepadID(pad):0,pad?SDL_GamepadConnected(pad):0,gWarioPauseTimer,gDisableWario,gDemoState);
 }
 lastMode=gMainGameMode;lastSub=gSubGameMode;lastRoom=gCurrentRoom;lastFocus=focus;
}
void PortDarwinTraceEvent(const SDL_Event *event) {
 if(event->type==SDL_EVENT_GAMEPAD_ADDED||event->type==SDL_EVENT_GAMEPAD_REMOVED||event->type==SDL_EVENT_GAMEPAD_REMAPPED)
  printf("Input event: type=%u gamepad=%u time=%llu\n",event->type,event->gdevice.which,(unsigned long long)SDL_GetTicks());
 if(event->type==SDL_EVENT_WINDOW_FOCUS_LOST||event->type==SDL_EVENT_WINDOW_FOCUS_GAINED)
  printf("Input focus: %s time=%llu\n",event->type==SDL_EVENT_WINDOW_FOCUS_GAINED?"gained":"lost",(unsigned long long)SDL_GetTicks());
}
