/* Reproduce a stale SDL handle independently of the game or any player save. */
#include "port/controls.h"
#include <stdio.h>
static int failures;
#define CHECK(x,what) do {if(!(x)){fprintf(stderr,"FAIL: %s\n",what);failures++;}}while(0)
static void DropEvents(void) {SDL_UpdateGamepads();SDL_Event e;while(SDL_PollEvent(&e)){} }
int main(void) {
 SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
 SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0x1209/0x574c");
 if(!SDL_Init(SDL_INIT_GAMEPAD))return 2;
 SDL_VirtualJoystickDesc d;SDL_INIT_INTERFACE(&d);
 d.vendor_id=0x1209;d.product_id=0x574c;
 d.type=SDL_JOYSTICK_TYPE_GAMEPAD;d.naxes=SDL_GAMEPAD_AXIS_COUNT;d.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
 d.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;d.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
 d.name="WL4 stale-handle recovery test (virtual)";
 SDL_JoystickID id=SDL_AttachVirtualJoystick(&d);SDL_Joystick *j=SDL_OpenJoystick(id);
 PortSettings settings;PortSettingsDefaults(&settings);PortControls c;PortControlsOpen(&c,&settings);
 PortControlsClose(&c);c.gamepad=SDL_OpenGamepad(id);
 SDL_SetJoystickVirtualButton(j,SDL_GAMEPAD_BUTTON_SOUTH,true);DropEvents();
 CHECK(PortControlsPressed(&c,NULL)&1,"initial virtual button reaches A");
 SDL_CloseJoystick(j);SDL_DetachVirtualJoystick(id);DropEvents();
 CHECK(c.gamepad&&!SDL_GamepadConnected(c.gamepad),"fixture retains disconnected handle when event is dropped");
 id=SDL_AttachVirtualJoystick(&d);j=SDL_OpenJoystick(id);DropEvents();
 SDL_SetJoystickVirtualButton(j,SDL_GAMEPAD_BUTTON_EAST,true);DropEvents();
 CHECK(PortControlsPressed(&c,NULL)&2,"new controller recovers without removal/addition event");
 CHECK(c.gamepad&&SDL_GamepadConnected(c.gamepad),"polling releases stale handle and opens a connected controller");
 PortControlsClose(&c);SDL_CloseJoystick(j);SDL_DetachVirtualJoystick(id);SDL_Quit();
 printf("Missed-event gamepad recovery: %s\n",failures?"FAIL":"PASS");return failures?1:0;
}
