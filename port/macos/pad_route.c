/* Explicit diagnostic mode: script -> SDL virtual device -> real controls -> GBA.
 * Never changes a physical controller or the normal player save path. */
#include <SDL3/SDL.h>
#include "port/controls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int sEnabled=-1,sFailures,sDropped,sReconnected;
static SDL_JoystickID sId;
static SDL_Joystick *sJoy;
static PortControls sControls;
static SDL_VirtualJoystickDesc sDesc;
static int sInject;
static void Attach(void) {
 sId=SDL_AttachVirtualJoystick(&sDesc);sJoy=SDL_OpenJoystick(sId);
 if(!sId||!sJoy){fprintf(stderr,"Virtual pad attachment failed: %s\n",SDL_GetError());sFailures++;}
}
int PortDarwinPadRouteEnabled(void) {
 if(sEnabled<0){
  sEnabled=getenv("WL4_PAD_ROUTE_TEST")!=NULL;
  if(sEnabled){
   SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
   SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0x1209/0x574c");
   if(!SDL_Init(SDL_INIT_GAMEPAD)){sFailures++;return 0;}
   SDL_INIT_INTERFACE(&sDesc);sDesc.vendor_id=0x1209;sDesc.product_id=0x574c;sDesc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
   sDesc.naxes=SDL_GAMEPAD_AXIS_COUNT;sDesc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
   sDesc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
   sDesc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
   sDesc.name="WL4 room-transition regression (virtual device)";
   Attach();PortSettings settings;PortSettingsDefaults(&settings);
   PortControlsOpen(&sControls,&settings);PortControlsClose(&sControls);
   sControls.gamepad=SDL_OpenGamepad(sId);
   sInject=getenv("WL4_PAD_DROP_EVENT_TEST")!=NULL;
  }
 }
 return sEnabled;
}
uint16_t PortDarwinPadRoute(unsigned step,uint16_t desired) {
 if(!PortDarwinPadRouteEnabled())return desired;
 if(sInject&&!sDropped&&step>=8070){
  SDL_CloseJoystick(sJoy);sJoy=NULL;SDL_DetachVirtualJoystick(sId);sDropped=1;
  printf("Input regression: drop removal event at step %u\n",step);
 }
 if(sDropped&&!sReconnected&&step>=8071){Attach();sReconnected=1;printf("Input regression: reattach at step %u\n",step);}
 if(sJoy){
  for(int action=0;action<PORT_ACTIONS;action++)SDL_SetJoystickVirtualButton(sJoy,(SDL_GamepadButton)sControls.settings.buttons[action],!!(desired&PortActionBits[action]));
 }
 SDL_UpdateGamepads();SDL_Event event;
 while(SDL_PollEvent(&event)){
  if(sDropped && (event.type==SDL_EVENT_GAMEPAD_ADDED||event.type==SDL_EVENT_GAMEPAD_REMOVED))continue;
  PortControlsEvent(&sControls,&event);
 }
 uint16_t actual=PortControlsPressed(&sControls,NULL);
 if(actual!=desired && (!sDropped||sReconnected)){
  if(sFailures++<5)fprintf(stderr,"Input mismatch step=%u expected=%03x actual=%03x connected=%d\n",step,desired,actual,sControls.gamepad?SDL_GamepadConnected(sControls.gamepad):0);
 }
 return actual;
}
int PortDarwinPadRouteFinish(void) {
 if(sEnabled!=1)return 0;
 PortControlsClose(&sControls);
 if(sJoy)SDL_CloseJoystick(sJoy);
 SDL_DetachVirtualJoystick(sId);SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
 printf("Virtual pad room route: %s (%d mismatches)\n",sFailures?"FAIL":"PASS",sFailures);
 return sFailures!=0;
}
