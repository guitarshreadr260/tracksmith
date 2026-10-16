from pathlib import Path
import shutil, zipfile, hashlib, json

base=Path(__file__).resolve().parents[2]
out=base/'outputs';src=base/'work/prototype/Tracksmith'
checks=json.loads((base/'work/prototype/prototype16-test-results.json').read_text(encoding='utf-8'))
assert checks['passed'], 'Native checks must pass before packaging.'
bundle=out/'Tracksmith-UI-Prototype-16';bundle.mkdir(parents=True,exist_ok=True)
shutil.copy2(base/'work/native-build/Tracksmith_artefacts/Release/Tracksmith.exe',bundle/'Tracksmith.exe')
shutil.copytree(out/'Tracksmith-UI-Prototype-15/Stock Plugins',bundle/'Stock Plugins',dirs_exist_ok=True)
shutil.copytree(src/'Native Instruments',bundle/'Native Instruments',dirs_exist_ok=True)
for name in ['NEURAL-15.md','RESEARCH-14.md','IO-SETUP-NOTES.md','TRANSPORT-GRID-ELASTIC-NOTES.md']:
 shutil.copy2(src/name,bundle/name)
shutil.copy2(base/'work/prototype/prototype16-test-results.json',bundle/'VALIDATION.json')
notes='''TRACKSMITH — WINDOWS PROTOTYPE 16
Reference UI / I/O setup / Grid / Dynamic punch

Run Tracksmith.exe with Stock Plugins and Native Instruments beside it.
Older builds and your saved sessions are kept separately.

NEW UI
The supplied design reference guides the dark panels, instrument colors,
bright waveforms, track icons, slim meters, and mixer controls.
Edit, Mix, and EZ Mode remain separate working views.
The collapsible Library sidebar opens imports, instruments, effects, and I/O.
Your Tracksmith artwork appears during startup and behind the session hub.
The reference image's video/inspector panels are not implemented here.

I/O
Open Setup > I/O (or the I/O setup button) for inputs, outputs, buses, and
track assignments. Rename and enable channels; labels save per interface.
Physical input numbering stays stable when other channels are disabled.
Limits: 32 input channels, 16 output channels, up to 32 audio recording tracks.
These limits still require sustained real-interface/disk testing.
Main can use an enabled pair or be disconnected. Stereo input paths use
adjacent channels. Aux buses feed Main or a separate output pair.
Nested buses, arbitrary nonadjacent stereo paths, bus-input recording,
talkback and hardware inserts are not included. Outputs 1/2 cannot currently
serve as an independent cue if Main is assigned elsewhere.
Interface changes require explicit review before hardware audio resumes.
Session format 10 saves the interface key and Main output assignment.
Legacy sessions open; their original interface map was not saved, so review
their input assignments before recording and save a new session copy.

GRID AND DYNAMIC PUNCH
Visible bar/beat/subdivision grid with selectable resolution and snapping.
Highlight a track range, move the independent playback start backward with
numpad 1, then press numpad 3. Playback starts at the marker; recording is
restricted to the highlighted range. Numpad 2 moves forward one 4/4 bar.
Space starts/stops. Stop recording before changing routing or hardware.
The old target take plays before/after punch and is silent inside recording.
Elastic Audio is NOT implemented: no warp markers or time-stretch DSP yet.

DRUMS / GUITAR
New native Groove tracks default to the included Chango Luke Holland kit.
Saved sessions restore their own kit choices. NAM remains built into Guitar.
Drum-learning development is paused. Neural mode starts unchecked; the
previous experimental local engine remains available if deliberately enabled.
No new neural model or learning behavior is included in this release.

VALIDATION
VALIDATION.json records this build's executed checks. Source UI interaction
checks use simulated engine state; native tests separately cover the engine.
Real ASIO/interface monitoring, latency, multitrack disk load, and musical
listening remain hardware checks. Keep included third-party notices.
'''
(bundle/'READ-ME.txt').write_text(notes,encoding='utf-8')
portable=out/'Tracksmith-Windows-UI-Prototype-16.zip';source=out/'Tracksmith-Source-UI-Prototype-16.zip'
with zipfile.ZipFile(portable,'w',zipfile.ZIP_DEFLATED) as archive:
 for p in bundle.rglob('*'):
  if p.is_file():archive.write(p,p.relative_to(out))
with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED) as archive:
 for p in src.rglob('*'):
  if not p.is_file():continue
  rel=p.relative_to(src)
  if any(v in {'node_modules','work','.git','dist-cache'} for v in rel.parts) or p.suffix in {'.tsbuildinfo','.obj','.pdb'}:continue
  archive.write(p,Path('Tracksmith')/rel)
 archive.writestr('Tracksmith/PROTOTYPE-NOTES.txt',notes)
for p in [portable,source]:
 with zipfile.ZipFile(p) as archive:assert archive.testzip() is None
(out/'Prototype-16-SHA256.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [portable,source]},indent=2))
print(json.dumps({str(p):p.stat().st_size for p in [portable,source]},indent=2))
