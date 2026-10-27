import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
import wave
from pathlib import Path
from drum_baseline import generate, read_midi, vlq
from align_drums import inspect_wav


class AlignmentTests(unittest.TestCase):
    def test_original_timeline_and_midi_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root/'guitar.wav'
            rate, seconds = 8000, 5
            with wave.open(str(source), 'wb') as audio:
                audio.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
                audio.writeframes(b''.join(struct.pack('<h', 15000 if i % 4000 < 80 and i >= 8000 else 0)
                                           for i in range(rate*seconds)))
            original = source.read_bytes()
            info = inspect_wav(source)
            self.assertAlmostEqual(info['attacks'][0]['seconds'], 1, places=2)
            result = subprocess.run([sys.executable, str(Path(__file__).with_name('align_drums.py')),
                                     str(source), str(root/'drums.mid'), '--bpm', '140', '--downbeat', '1', '--drum-map', 'gm'],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            hits, end = read_midi(root/'drums.mid')
            self.assertTrue(hits)
            self.assertGreaterEqual(min(t for t,n,v in hits)*60/140, 1-.001)
            self.assertLessEqual(max(t for t,n,v in hits)*60/140, 5)
            self.assertAlmostEqual(end*60/140, 5, delta=.001)
            self.assertEqual(original, source.read_bytes())
            report = json.loads((root/'drums.alignment.json').read_text())
            self.assertEqual(report['source_sha256'], hashlib.sha256(original).hexdigest())
            # MIDI's embedded tempo agrees with the supplied 140 BPM.
            data = (root/'drums.mid').read_bytes()
            pos = data.index(b'\xff\x51\x03')+3
            self.assertEqual(int.from_bytes(data[pos:pos+3], 'big'), round(60000000/140))
            again = subprocess.run([sys.executable, str(Path(__file__).with_name('align_drums.py')),
                                    str(source), str(root/'drums.mid'), '--bpm', '140'], capture_output=True)
            self.assertNotEqual(again.returncode, 0)

    def test_zero_grid_no_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # Force one note at step zero of each bar; neutral learned offsets.
            stats = {'bars': 1, 'hits': [[1]+[0]*8]+[[0]*9 for _ in range(15)],
                     'velocity': [[100]*9 for _ in range(16)], 'offset': [[0]*9 for _ in range(16)]}
            model = root/'model.json'
            model.write_text(json.dumps({'groups': {'all': stats}}))
            generate(model, root/'a.mid', 'all', 140, 128, 42, pattern='learned')
            hits, end = read_midi(root/'a.mid')
            self.assertEqual([h[0] for h in hits], list(range(0,512,4)))
            self.assertEqual(end, 512)
            generate(model, root/'b.mid', 'all', 140, 128, 42, pattern='learned')
            self.assertEqual((root/'a.mid').read_bytes(), (root/'b.mid').read_bytes())

if __name__ == '__main__':
    unittest.main()
