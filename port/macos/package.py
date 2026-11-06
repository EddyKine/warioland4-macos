"""Package a private local app on the external SSD; the ROM stays outside the bundle."""
from pathlib import Path
import plistlib,shutil,subprocess,sys
root=Path(__file__).resolve().parents[2]; trial='--audio-trial' in sys.argv
dest=root.parent/('Wario Land 4 Audio.app' if trial else 'Wario Land 4.app'); build=root/'build/macos-arm64'
cache=(build/'CMakeCache.txt').read_text()
experimental='WL4_DARWIN_AUDIO_EXPERIMENT:BOOL=ON' in cache
if experimental != trial:
 raise SystemExit('Audio experiment and package choice differ; normal app requires experiment OFF, trial requires ON.')
mac=dest/'Contents/MacOS';frameworks=dest/'Contents/Frameworks';resources=dest/'Contents/Resources'
for p in [mac,frameworks,resources]:p.mkdir(parents=True,exist_ok=True)
subprocess.run(['clang',str(root/'port/macos/launcher.c'),*(['-DWL4_AUDIO_TRIAL'] if trial else []),'-o',str(mac/'Launch')],check=True)
shutil.copyfile(build/'wl4',mac/'wl4')
# Use the installed SDL already verified by CMake; retain its license and origin.
sdl=Path('/opt/homebrew/opt/sdl3/lib/libSDL3.0.dylib').resolve()
shutil.copyfile(sdl,frameworks/'libSDL3.0.dylib')
subprocess.run(['install_name_tool','-change','/opt/homebrew/opt/sdl3/lib/libSDL3.0.dylib','@executable_path/../Frameworks/libSDL3.0.dylib',str(mac/'wl4')],check=True)
subprocess.run(['install_name_tool','-id','@executable_path/../Frameworks/libSDL3.0.dylib',str(frameworks/'libSDL3.0.dylib')],check=True)
with (dest/'Contents/Info.plist').open('wb') as f:
 plistlib.dump(dict(CFBundleExecutable='Launch',CFBundleIdentifier='local.warioland4.audio-trial' if trial else 'local.warioland4.experimental',CFBundleName='Wario Land 4 Audio' if trial else 'Wario Land 4',CFBundleDisplayName='Wario Land 4 Audio' if trial else 'Wario Land 4',CFBundlePackageType='APPL',CFBundleVersion='1',CFBundleShortVersionString='0.1-local',NSHighResolutionCapable=True,LSMinimumSystemVersion='15.0',NSSupportsAutomaticGraphicsSwitching=True),f)
shutil.copyfile(root/'LICENSE',resources/'LICENSE-project.txt')
sdl_license=sdl.parent.parent/'LICENSE.txt'
if not sdl_license.exists():
 for p in sdl.parent.parent.glob('**/LICENSE*'):
  if p.is_file():sdl_license=p;break
if sdl_license.exists():shutil.copyfile(sdl_license,resources/'LICENSE-SDL.txt')
for p in [frameworks/'libSDL3.0.dylib',mac/'wl4']:
 subprocess.run(['codesign','--force','--sign','-',str(p)],check=True)
# exFAT AppleDouble metadata is not executable code. Preserve it outside the app
# before the signer enumerates nested code; never remove user files.
metadata=build/'package-metadata'
for item in sorted(dest.rglob('._*'),key=lambda p:len(p.parts),reverse=True):
 target=metadata/item.relative_to(dest)
 target.parent.mkdir(parents=True,exist_ok=True)
 if target.exists():target=target.with_name(target.name+'.'+str(__import__('time').time_ns()))
 item.rename(target)
subprocess.run(['codesign','--force','--sign','-',str(dest)],check=True)
print(dest)
