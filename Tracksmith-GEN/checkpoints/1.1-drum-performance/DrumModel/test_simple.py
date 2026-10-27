import tempfile
import unittest
import subprocess
import sys
import wave
import json
from pathlib import Path
from drum_baseline import generate
from test_maps import events


class SimpleTests(unittest.TestCase):
    def test_launcher_defaults_to_bar_two(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=root/'guitar.wav';target=root/'drums.mid'
            with wave.open(str(source),'wb') as wav:
                wav.setparams((1,2,8000,0,'NONE','not compressed'))
                wav.writeframes(b'\x00\x00'*16000+b'\x10\x27'*24000)
            result=subprocess.run([sys.executable,'-B',str(Path(__file__).with_name('align_drums.py')),str(source),str(target),'--bpm','140'],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            on=[e for e in events(target) if e[1]==153]
            self.assertEqual(min(e[0] for e in on),1920)
            self.assertTrue(all((e[0]-1920)%240==0 for e in on))
            self.assertEqual({e[2] for e in on},{24,26,43})
            report=json.loads(target.with_suffix('.alignment.json').read_text())
            self.assertEqual(report['start_bar'],2)
            self.assertEqual(report['pattern'],'simple')
            self.assertTrue(report['grid_locked'])

    def test_exact_backbeat_and_no_extra_instruments(self):
        with tempfile.TemporaryDirectory() as folder:
            target=Path(folder)/'simple.mid'
            generate(Path(folder)/'no-model-needed.json',target,'all',140,2,42,drum_map='ggd-okw-metal')
            on=[e for e in events(target) if e[1]==0x99]
            expected=[]
            for bar in range(2):
                for step in range(8):expected.append((bar*1920+step*240,0x99,43,75))
                for beat in (0,2):expected.append((bar*1920+beat*480,0x99,24,100))
                for beat in (1,3):expected.append((bar*1920+beat*480,0x99,26,110))
            self.assertEqual(sorted(on),sorted(expected))
            self.assertEqual(len(on),24)
            self.assertEqual({e[2] for e in on},{24,26,43})
            self.assertTrue(all(e[0]%240==0 for e in on))

    def test_128_bars_no_drift_no_seed_variation(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for seed in (1,999):generate(root/'absent.json',root/str(seed),'all',140,128,seed)
            self.assertEqual((root/'1').read_bytes(),(root/'999').read_bytes())
            hit=[e for e in events(root/'1') if e[1]==153]
            self.assertEqual(len(hit),128*12)
            self.assertEqual([e[0] for e in hit if e[2]==36], [bar*1920+beat*480 for bar in range(128) for beat in (0,2)])
            self.assertTrue(all(e[0]%240==0 for e in hit))
            self.assertEqual(events(root/'1')[-1][0],128*1920)

    def test_offset_and_partial_end_preserved(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);target=root/'partial.mid'
            generate(root/'absent.json',target,'all',140,4,42,downbeat=1,duration=4.1,drum_map='ggd-okw-metal')
            all_events=events(target);origin=round(140/60*480);end=round(4.1*140/60*480)
            on=[e for e in all_events if e[1]==153]
            self.assertEqual(min(e[0] for e in on),origin)
            self.assertTrue(all((e[0]-origin)%240==0 and e[0]<end for e in on))
            self.assertLessEqual(max(e[0] for e in all_events),end)
            self.assertEqual(all_events[-1][0],end)
            self.assertEqual(sorted((e[2],e[3]) for e in on),sorted((e[2],{24:100,26:110,43:75}[e[2]]) for e in on))


if __name__=='__main__':unittest.main()
