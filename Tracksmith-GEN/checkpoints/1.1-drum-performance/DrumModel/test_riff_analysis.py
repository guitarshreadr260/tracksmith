import hashlib
import math
import struct
import tempfile
import unittest
import wave
from pathlib import Path
from riff_analysis import analyze,write_lab


def fixture(path):
    rate=8000;bpm=140;bar=240/bpm;seconds=5*bar
    attacks=[bar*number+beat*60/bpm for number in range(1,5) for beat in (0,.75,2,2.75)]
    with wave.open(str(path),'wb') as wav:
        wav.setparams((1,2,rate,0,'NONE','not compressed'))
        pcm=[]
        for i in range(round(rate*seconds)):
            t=i/rate;v=0
            for onset in attacks:
                dt=t-onset
                if 0<=dt<.1:v+=12000*math.exp(-dt*35)*math.sin(2*math.pi*(170*dt+140*dt*dt))
            pcm.append(round(max(-32000,min(32000,v))))
        wav.writeframes(struct.pack('<'+'h'*len(pcm),*pcm))
    return attacks


class RiffAnalysisTests(unittest.TestCase):
    def test_repeated_rhythm_attacks_original_timeline_and_hash(self):
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)/'demo.wav';expected=fixture(source);original=source.read_bytes()
            result=analyze(source,140,2)
            self.assertEqual(source.read_bytes(),original)
            self.assertEqual(result['source_sha256'],hashlib.sha256(original).hexdigest())
            self.assertEqual(min(o['tick'] for o in result['onsets']),1920)
            self.assertEqual(len(result['onsets']),len(expected))
            for wanted in expected:self.assertTrue(any(abs(o['seconds']-wanted)<.02 for o in result['onsets']))
            groups=[b['group'] for b in result['bars'][1:]]
            self.assertEqual(len(set(groups)),1)
            self.assertTrue(result['rests'])
            self.assertTrue(all(o['tick']%120==0 for o in result['onsets']))

    def test_standalone_html_is_script_safe_and_overwrite_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=root/'demo.wav';fixture(source);target=root/'lab.html'
            result=write_lab(source,target)
            html=target.read_text(encoding='utf-8')
            self.assertNotIn('__RIFF_CORE_JS__',html);self.assertNotIn('__ANALYSIS_JSON__',html)
            self.assertIn('Export reviewed GGD drum MIDI',html)
            self.assertNotIn('<script src=',html)
            self.assertIn(result['source_sha256'],html)
            with self.assertRaises(ValueError):write_lab(source,target)

    def test_silent_recording_and_invalid_start_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)/'silent.wav'
            with wave.open(str(source),'wb') as wav:wav.setparams((1,2,8000,0,'NONE','not compressed'));wav.writeframes(b'\0\0'*8000)
            with self.assertRaises(ValueError):analyze(source)
            with self.assertRaises(ValueError):analyze(source,start_bar=0)


if __name__=='__main__':unittest.main()
