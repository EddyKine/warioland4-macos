"""Create a public app from an existing build, excluding all private resources."""
from pathlib import Path
import argparse, hashlib, plistlib, shutil, subprocess, zipfile, time
YOSHI = Path(__file__).resolve().parent.name == 'scripts'
root = Path(__file__).resolve().parents[1 if YOSHI else 2]
p = argparse.ArgumentParser(); p.add_argument('--output', type=Path, required=True); a = p.parse_args()
out = a.output.resolve(); out.mkdir(parents=True, exist_ok=False)
name = 'Yoshi Island' if YOSHI else 'Wario Land 4'
slug = 'Yoshi-Island' if YOSHI else 'Wario-Land-4'
app = out/(name+'.app'); mac=app/'Contents/MacOS'; fw=app/'Contents/Frameworks'; res=app/'Contents/Resources'; licenses=res/'Licenses'
for d in (mac,fw,licenses): d.mkdir(parents=True,exist_ok=True)
runtime=mac/('sma3-runtime' if YOSHI else 'wl4')
source=root/('build/menu/sma3_menu' if YOSHI else 'build/macos-arm64/wl4')
if not YOSHI and 'WL4_DARWIN_AUDIO_EXPERIMENT:BOOL=ON' in (root/'build/macos-arm64/CMakeCache.txt').read_text():
    raise SystemExit('Audio experiment must be OFF')
shutil.copyfile(source,runtime); runtime.chmod(0o755)
launcher=root/('host-menu/release_launcher.m' if YOSHI else 'port/macos/release_launcher.m')
subprocess.run(['clang','-arch','arm64','-O2','-fobjc-arc','-Wno-deprecated-declarations',*(['-DYOSHI_RELEASE'] if YOSHI else []),str(launcher),'-framework','Cocoa','-o',str(mac/'Launch')],check=True)
libs={'libSDL3.dylib':Path('/opt/homebrew/opt/sdl3/lib/libSDL3.0.dylib').resolve()}
if YOSHI: libs['libSDL2.dylib']=Path('/opt/homebrew/opt/sdl2/lib/libSDL2.dylib').resolve()
for dest,src in libs.items():
    shutil.copyfile(src,fw/dest); (fw/dest).chmod(0o755)
    subprocess.run(['install_name_tool','-id','@rpath/'+dest,str(fw/dest)],check=True)
for binary in [runtime,*[fw/n for n in libs]]:
    deps=subprocess.check_output(['otool','-L',str(binary)],text=True).splitlines()[1:]
    for line in deps:
        old=line.strip().split(' (')[0]
        if old.startswith('/opt/homebrew/'):
            target='libSDL2.dylib' if 'libSDL2' in old else 'libSDL3.dylib' if 'libSDL3' in old else None
            if not target: raise SystemExit('Unbundled dependency: '+old)
            new=('@executable_path/../Frameworks/' if binary==runtime else '@loader_path/')+target
            subprocess.run(['install_name_tool','-change',old,new,str(binary)],check=True)
    if '/opt/homebrew/' in subprocess.check_output(['otool','-L',str(binary)],text=True):
        raise SystemExit('Homebrew dependency remains: '+str(binary))
notices={'Project.txt':root/'LICENSE','SDL3.txt':Path('/opt/homebrew/opt/sdl3/LICENSE.txt')}
if YOSHI:
    notices.update({'GBARecomp.txt':root/'sources/gbarecomp/LICENSE','recomp-ui.txt':root/'sources/recomp-ui/LICENSE','Dear-ImGui.txt':root/'sources/recomp-ui/src/third_party/imgui/LICENSE.txt','arm-recomp-core.txt':root/'sources/gbarecomp/external/arm-recomp-core/LICENSE','rbengine.txt':root/'sources/gbarecomp/external/rbengine/LICENSE','recomp-net.txt':root/'sources/gbarecomp/external/recomp-net/LICENSE','sdl2-compat.txt':Path('/opt/homebrew/opt/sdl2-compat/LICENSE.txt'),'THIRD_PARTY_ATTRIBUTION.md':root/'sources/gbarecomp/THIRD_PARTY_ATTRIBUTION.md','NOTICE.md':root/'NOTICE.md'})
else:
    notices['minicoro.h']=root/'port/third_party/minicoro/minicoro.h'
    notices['SDL-font.txt']=root/'port/assets/fonts/SDL-LICENSE.txt'
    notices['Font-provenance.md']=root/'port/assets/fonts/README.md'
for dest,src in notices.items(): shutil.copyfile(src,licenses/dest)
info=dict(CFBundleExecutable='Launch',CFBundleIdentifier='com.eddykine.'+('yoshi-island' if YOSHI else 'warioland4'),CFBundleName=name,CFBundleDisplayName=name,CFBundlePackageType='APPL',CFBundleVersion='1',CFBundleShortVersionString='0.1.0',LSMinimumSystemVersion='27.0',NSHighResolutionCapable=True,NSSupportsAutomaticGraphicsSwitching=True)
with (app/'Contents/Info.plist').open('wb') as f: plistlib.dump(info,f)
readme=f'''{name} — v0.1.0-beta — macOS Apple Silicon

Requiere Mac arm64 con macOS 27.0 o posterior. Pruebas actuales en macOS 27.2; base validada previamente en 27.0.1.
No necesita Homebrew ni herramientas de compilación para jugar.

1. Descomprime el ZIP y copia {name}.app a la carpeta que prefieras.
2. Abre la app. Selecciona tu propia ROM compatible{' y tu propia BIOS GBA' if YOSHI else ''}.
3. La ruta se recuerda; los archivos permanecen en su ubicación original.
Mantén Option al iniciar para elegir otros archivos.

No incluye ROM, BIOS ni partidas. ROM requerida:
{('SMA3 USA rev 0, A3AE, 4194304 bytes, SHA1 7352d2bd064d9ebaec579e264228aa21c7345b80.' if YOSHI else 'Wario Land 4 USA/Europa, AWAE, 8388608 bytes, MD5 5fe47355a33e3fabec2a1607af88a404.')}
{('BIOS GBA: 16384 bytes, SHA1 300c20df6731a33952ded8c436f7f186d25d3492.' if YOSHI else '')}

Datos: ~/Library/Application Support/EddyKine/{name}
Partida: saves/game.sav. Registro: logs/last-run.log.
Estos datos son independientes de las apps privadas anteriores. No importa ni
sobrescribe sus partidas. Cierra el juego antes de copiar una partida compatible.

{('Menú: Esc o Back+Start. Controles, pantalla y volumen en el menú.' if YOSHI else 'Flechas: movimiento. X/Z: A/B. A/S: L/R. Enter: Start. Backspace: Select. F11: pantalla completa. Escape: salir.')}

Firma ad hoc, sin notarización ni Developer ID. Gatekeeper puede impedir la
apertura en otro Mac. No se garantiza ejecución sin avisos.

Beta experimental. No se ha validado todo el juego ni un segundo Mac físico.
{('Usa código recompilado con fallback al intérprete; no es totalmente estático.' if YOSHI else 'Los crujidos ocasionales del audio siguen pendientes.')}
Créditos/licencias en Contents/Resources/Licenses y en el repositorio:
https://github.com/EddyKine/{('yoshi-island-macos' if YOSHI else 'warioland4-macos')}
'''
(out/'LEEME.txt').write_text(readme); (res/'LEEME.txt').write_text(readme)
metadata=out/'metadata'
def preserve_metadata():
    for item in sorted(app.rglob('._*'),key=lambda p:len(p.parts),reverse=True):
        dest=metadata/item.relative_to(app)
        if dest.exists(): dest=dest.with_name(dest.name+'.'+str(time.time_ns()))
        dest.parent.mkdir(parents=True,exist_ok=True); item.rename(dest)
preserve_metadata()
for binary in [*[fw/n for n in libs],runtime]:
    subprocess.run(['codesign','--force','--sign','-',str(binary)],check=True)
    preserve_metadata()
subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
preserve_metadata()
subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True)
archive=out/(slug+'-v0.1.0-beta-macOS-arm64.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for f in [out/'LEEME.txt',*sorted(app.rglob('*'))]:
        if f.is_file() and not f.name.startswith('._'):
            if f.suffix.lower() in {'.gba','.bin','.sav','.srm','.state'}: raise SystemExit('Private resource blocked: '+str(f))
            z.write(f,f.relative_to(out))
digest=hashlib.sha256(archive.read_bytes()).hexdigest(); (out/'SHA256SUMS.txt').write_text(digest+'  '+archive.name+'\n')
print(archive)
