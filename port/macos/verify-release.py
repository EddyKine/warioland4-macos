"""Verify a relocated public ZIP with private inputs and disposable data only."""
from pathlib import Path
import argparse, hashlib, os, subprocess, tempfile, zipfile
p=argparse.ArgumentParser();p.add_argument('--archive',type=Path,required=True);p.add_argument('--rom',type=Path,required=True);p.add_argument('--bios',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1 if 'scripts' in Path(__file__).resolve().parts else 2]
qa=Path(tempfile.mkdtemp(prefix='release-qa-',dir=root/'build'))
archive=a.archive.resolve();rom=a.rom.resolve();bios=a.bios.resolve() if a.bios else None
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for n in z.namelist():
        path=Path(n)
        assert not path.is_absolute() and '..' not in path.parts
        assert not any(x.startswith('._') for x in path.parts)
        assert path.suffix.lower() not in {'.gba','.bin','.sav','.srm','.state'}
subprocess.run(['ditto','-x','-k',str(archive),str(qa/'Relocated folder')],check=True)
app=next((qa/'Relocated folder').glob('*.app'));launch=app/'Contents/MacOS/Launch'
subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True)
for f in [launch,*list((app/'Contents/Frameworks').glob('*.dylib')),*list((app/'Contents/MacOS').glob('*runtime')),*list((app/'Contents/MacOS').glob('wl4'))]:
    assert '/opt/homebrew/' not in subprocess.check_output(['otool','-L',str(f)],text=True)
raw=[rom.read_bytes()]+([bios.read_bytes()] if bios else [])
for f in app.rglob('*'):
    if f.is_file():
        data=f.read_bytes()
        assert all(image not in data for image in raw),f
print('Archive allowlist, no raw ROM/BIOS, relocation, signature and load paths: PASS',flush=True)
env=os.environ.copy();env.update(PORT_DATA_DIR=str(qa/'User data'),PORT_ROM=str(rom),DYLD_PRINT_LIBRARIES='1',SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software')
if bios:env['PORT_BIOS']=str(bios)
for tag,change,expected in [('missing',{'PORT_ROM':str(qa/'missing.gba')},1),('valid',{},0)]:
    e=env|change
    result=subprocess.run([str(launch),'--headless','--frames','600'],env=e,cwd=qa,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=60)
    (qa/(tag+'-launcher.log')).write_bytes(result.stdout+result.stderr)
    assert result.returncode==expected,(tag,result.returncode,result.stderr[-1000:])
log=(qa/'User data/logs/last-run.log').read_text(errors='replace')
assert '/opt/homebrew/' not in log
assert 'Frameworks/libSDL3.dylib' in log
if bios:assert 'Frameworks/libSDL2.dylib' in log
# Exercise remembered paths without the environment shortcuts, then reload own save.
(qa/'User data/rom-path.txt').write_text(str(rom));env.pop('PORT_ROM')
if bios:
    (qa/'User data/bios-path.txt').write_text(str(bios));env.pop('PORT_BIOS')
result=subprocess.run([str(launch),'--headless','--frames','30'],env=env,cwd=qa,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30)
assert result.returncode==0,result.stderr[-1000:]
print('Missing-ROM rejection, 600-frame run, bundled SDL and remembered paths: PASS',flush=True)
print('QA artifacts:',qa)
