# Build-only derivatives preserve all shared GBA sources and generated originals.
from pathlib import Path
import re,sys
def write(path, text):
 if not path.exists() or path.read_text()!=text: path.write_text(text)

root=Path(sys.argv[1]); out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
header=out/'include/gba/gba_ptr.h';header.parent.mkdir(parents=True,exist_ok=True)
s=(root/'include/gba/gba_ptr.h').read_text()
s=s.replace('#ifdef PORTABLE','#ifdef PORTABLE\n#include "address.h"',1)
s=s.replace('if (address >> 32)','address = PortDarwinGba(address);\n    if (address >> 32)')
s=s.replace('(__UINTPTR_TYPE__)(x).a','PortDarwinHost((__UINTPTR_TYPE__)(x).a)')
write(header,s)
s=(root/'port/generated/gba_symbols.s').read_text()
def symbol(m):
 name,value=m.group(1),int(m.group(2),16)
 if value >= 0x01000000: value+=0x8000000000
 return f'    .globl _{name}\n    .set _{name}, 0x{value:X}'
s=re.sub(r'    \.globl (\w+)\n    \.set \w+, (0x[0-9a-fA-F]+)',symbol,s)
write(out/'gba_symbols.s',s)
for name in ['color_fading','rom','game_thread','main','gba_memory','gba_io','ppu','frame','bios','audio','m4a_native','save','gba_functions']:
 source=root/('src/color_fading.c' if name=='color_fading' else f'port/src/{name}.c')
 s=source.read_text()
 s='#include "address.h"\n'+s
 if name=='color_fading': s=s.replace('section(".text")','section("__TEXT,__const")')
 if name=='rom': s=s.replace('(unsigned char *)(uintptr_t)GBA_ROM_BASE','(unsigned char *)PortDarwinHost(GBA_ROM_BASE)')
 if name=='game_thread': s=s.replace('(void *)(uintptr_t)PORT_GAME_STACK_LIMIT','(void *)PortDarwinHost(PORT_GAME_STACK_LIMIT)')
 if name=='main':
  s=s.replace('uintptr_t a = (uintptr_t)p;','uintptr_t a = PortDarwinGba((uintptr_t)p);')
  s=s.replace('int main(int argc, char **argv)\n{','int main(int argc, char **argv)\n{\n    setvbuf(stdout, NULL, _IOLBF, 0);')
 if name=='gba_memory': s=s.replace('memset((void *)(uintptr_t)GBA_SRAM_BASE','memset((void *)PortDarwinHost(GBA_SRAM_BASE)')
 if name=='gba_io': s=s.replace('    return address;','    return PortDarwinHost(address);')
 if name=='ppu': s=re.sub(r'\(uintptr_t\)(GBA_\w+_BASE)',r'PortDarwinHost(\1)',s)
 if name=='frame': s=s.replace('(const void *)(uintptr_t)kRegions[i].base','(const void *)PortDarwinHost(kRegions[i].base)')
 # GP_SET must accept the high stack and host aliases; runtime literals can fault as a fallback.
 write(out/f'{name}.c',s)
s=(root/'src/minigames/roulette/update_roulette_intro.c').read_text().replace('section(".text")','section("__TEXT,__const")')
write(out/'update_roulette_intro.c',s)
s=(root/'port/src/bios_test.c').read_text()
s=s.replace('__asm__ volatile("movq (%0), %0" : "+r"(p));','__asm__ volatile("ldr %0, [%0]" : "+r"(p));')
s=s.replace('__asm__ volatile("movzbl 6(%1,%2,8), %k0" : "=&r"(out) : "r"(p), "r"(index));','out = *(volatile uint8_t *)(p + index * 8 + 6);')
s=s.replace('__asm__ volatile("addl 8(%1), %0" : "+r"(sum) : "r"(p));','sum += *(volatile uint32_t *)(p + 8);')
write(out/'bios_test.c',s)
for name in ['hoggus','aerodent','cuckoo_condor']:
 s=(root/f'src/sprite_ai/{name}.c').read_text()
 s=re.sub(r'asm\("(\w+)"\)',lambda m:'asm("_'+m.group(1)+'")',s)
 write(out/f'{name}.c',s)
s=(root/'port/generated/layout_checks/color_fading.c').read_text()
s=s.replace('../../../src/color_fading.c',str(out/'color_fading.c'))
write(out/'layout_color_fading.c',s)

# macOS diagnostics and virtual-device regression run the same frame/controls path.
s=(out/'main.c').read_text()
s='''#include <SDL3/SDL.h>
extern int PortDarwinPadRouteEnabled(void);
extern unsigned short PortDarwinPadRoute(unsigned, unsigned short);
extern int PortDarwinPadRouteFinish(void);
extern void PortDarwinTraceFrame(unsigned, unsigned, int, SDL_Gamepad *);
'''+s
s=s.replace('PortIoWrite16(REG_KEYINPUT, (uint16_t)(~PortInputScriptKeys(PortFrameSteps() + 1) & KEYS_ALL));','{ unsigned wanted=PortInputScriptKeys(PortFrameSteps()+1);\n                unsigned sent=PortDarwinPadRoute(PortFrameSteps()+1,wanted);\n                PortIoWrite16(REG_KEYINPUT,(uint16_t)(~sent & KEYS_ALL)); }')
s=s.replace('        PortSaveFrame();','        PortSaveFrame();\n        if(getenv("WL4_STATE_TRACE")) PortDarwinTraceFrame(PortInputScriptKeys(PortFrameSteps()+1), (~PortIoRead16(REG_KEYINPUT))&KEYS_ALL, 1, NULL);',1)
# Always report device-route mismatch without changing ordinary RunGame results.
s=s.replace('    return RunGame(', '    int padResult = RunGame(')
if '    int padResult = RunGame(' in s:
 pos=s.index('    int padResult = RunGame(');end=s.index(';',pos)+1
 s=s[:end]+'\n    return padResult || PortDarwinPadRouteFinish();'+s[end:]
write(out/'main.c',s)
s=(root/'port/platform/sdl/sdl_frontend.c').read_text()
s=s.replace('static SDL_AudioStream *sAudio;','extern void PortDarwinTraceFrame(unsigned,unsigned,int,SDL_Gamepad *);\nextern void PortDarwinTraceEvent(const SDL_Event *);\nstatic SDL_AudioStream *sAudio;')
s=s.replace('    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {','    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");\n    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {')
s=s.replace('            PortControlsEvent(&controls, &event);','            PortDarwinTraceEvent(&event);\n            PortControlsEvent(&controls, &event);')
s=s.replace('            keys = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)\n                       ? PortControlsPressed(&controls, SDL_GetKeyboardState(NULL)) : 0;','            unsigned raw = PortControlsPressed(&controls, SDL_GetKeyboardState(NULL));\n            int focused = !!(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);\n            keys = focused ? raw : 0;')
s=s.replace('            PortSaveFrame();','            PortSaveFrame();\n            PortDarwinTraceFrame(raw,keys,focused,controls.gamepad);\n            if(PortFrameCount()%120==0) printf("Audio state: frame=%u queued_bytes=%d refills=%u drops=%u clock_resets=%u\\n",PortFrameCount(),sAudio?SDL_GetAudioStreamQueued(sAudio):0,sAudioRefills,sAudioDrops,pacer.resets);')
s=s.replace('    SDL_ResumeAudioStreamDevice(sAudio);','    SDL_ResumeAudioStreamDevice(sAudio);\n    printf("Audio: device opened, source rate=%u Hz, stereo\\n", PORT_AUDIO_RATE);')
s=s.replace('int timingStats = SDL_getenv("WL4_TIMING_STATS") != NULL;','int timingStats = 1; // Log pacing/underrun evidence on every normal exit.')
write(out/'sdl_frontend.c',s)
# Audio experiments are opt-in; normal builds retain the responsive player-confirmed path.
if '--experimental-audio' in sys.argv:
 # Replace only the Darwin presentation queue; the GBA sound generator stays unchanged.
 s=(out/'sdl_frontend.c').read_text()
 s=s.replace('static SDL_AudioStream *sAudio;','#include "audio_output.h"\nstatic DarwinAudioOutput sDarwinAudio;\nstatic SDL_AudioStream *sAudio;')
 s=s.replace('    SDL_ResumeAudioStreamDevice(sAudio);','    DarwinAudioOutputInit(&sDarwinAudio,sAudio,PORT_AUDIO_RATE,1);')
 a=s.index('    int16_t frames[2048 * 2];',s.index('static void QueueAudio('));b=s.index('\n}',a)
 s=s[:a]+'''    int16_t frames[2048 * 2];
     size_t n;
     DarwinAudioOutputTick(&sDarwinAudio,fastForward);
     while((n=PortAudioTake(frames,2048))!=0)
         if(sAudio&&!fastForward)DarwinAudioOutputPush(&sDarwinAudio,frames,n);
     DarwinAudioOutputTick(&sDarwinAudio,fastForward);
     sAudioRefills=sDarwinAudio.refills;sAudioDrops=sDarwinAudio.drops;'''+s[b:]
 s=s.replace('    SDL_DestroyAudioStream(sAudio);','    printf("Audio continuity: pre_roll_ms=67 ratio=%.6f demand_shortfalls=%u demand_bytes=%u api_errors=%u\\n",sDarwinAudio.ratio,atomic_load(&sDarwinAudio.shortfallRequests),atomic_load(&sDarwinAudio.shortfallBytes),sDarwinAudio.errors);\n    DarwinAudioOutputDetach(&sDarwinAudio);\n    SDL_DestroyAudioStream(sAudio);')
 s=s.replace('    printf("Audio: device opened, source rate=%u Hz, stereo\\n", PORT_AUDIO_RATE);','    printf("Audio: continuous queue, real pre-roll=67 ms, source rate=%u Hz, stereo\\n", PORT_AUDIO_RATE);')
 write(out/'sdl_frontend.c',s)
# Bound diagnostic playback without synthesizing UI events or touching a player save.
s=(out/'sdl_frontend.c').read_text()
s=s.replace('    while (running) {','    const char *limitText=SDL_getenv("WL4_TEST_FRAMES");\n    unsigned testLimit=limitText?(unsigned)SDL_strtoul(limitText,NULL,10):0;\n    while (running) {\n        if(testLimit && PortFrameCount()>=testLimit)running=0;')
write(out/'sdl_frontend.c',s)
