"""Input safety and conditioning contract tests; these do not rate sound quality."""
import hashlib
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch
import xml.etree.ElementTree as ET

import numpy as np
import soundfile as sf
import torch
import reference_worker as worker


class ReferenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'original with spaces.wav'
        rate = 22050
        tone = .2 * np.sin(2 * np.pi * 220 * np.arange(rate * 8) / rate)
        sf.write(self.source, tone, rate, subtype='PCM_16')

    def test_crop_and_original_preservation(self):
        before = self.source.read_bytes()
        output = self.root / 'reference.wav'
        duration, rate = worker.prepare(self.source, 2, 5, output)
        self.assertEqual((duration, rate), (5, 22050))
        data, _ = sf.read(output, always_2d=True)
        self.assertEqual(data.shape, (110250, 2))
        np.testing.assert_array_equal(data[:, 0], data[:, 1])
        self.assertEqual(self.source.read_bytes(), before)

    def test_invalid_or_too_short_selection(self):
        for start, duration in [(-1, 5), (8, 5), (4, 5), (0, 31), (float('nan'), 5)]:
            with self.subTest(start=start, duration=duration):
                with self.assertRaises(ValueError):
                    worker.prepare(self.source, start, duration, self.root / 'out.wav')

    def test_mp3_decoding(self):
        audio, rate = sf.read(self.source)
        source = self.root / 'input.mp3'
        sf.write(source, audio, rate, format='MP3')
        before = source.read_bytes()
        duration, _ = worker.prepare(source, .5, 5, self.root / 'mp3-reference.wav')
        self.assertAlmostEqual(duration, 5)
        self.assertEqual(source.read_bytes(), before)

    def test_reference_conditioning_seed_and_amount(self):
        calls = []
        class FakeModel:
            model = types.SimpleNamespace(sample_rate=44100)
            model_config = {'sample_size': 5292032}
            @classmethod
            def from_pretrained(cls, *args, **kwargs):
                return cls()
            def generate(self, **kwargs):
                calls.append(kwargs)
                return kwargs['init_audio'][1].unsqueeze(0) * .8
        request = ET.Element('request')
        for key, value in dict(source=str(self.source), start=1, duration=5, amount=.7,
                               count=3, prompt='A guitar variation', seed=42).items():
            ET.SubElement(request, key).text = str(value)
        path = self.root / 'request.xml'
        ET.ElementTree(request).write(path)
        module = types.SimpleNamespace(StableAudioModel=FakeModel)
        with patch.dict('sys.modules', {'stable_audio_3': module}), \
             patch.object(torch.cuda, 'is_available', return_value=True), \
             patch.object(torch.cuda, 'get_device_name', return_value='Test GPU'), \
             patch.object(torch.cuda, 'reset_peak_memory_stats'), \
             patch.object(torch.cuda, 'synchronize'), \
             patch.object(torch.cuda, 'max_memory_allocated', return_value=100):
            worker.run(path)
        self.assertEqual([call['seed'] for call in calls], [42, 43, 44])
        for call in calls:
            self.assertEqual(call['init_noise_level'], .7)
            self.assertEqual(call['init_audio'][0], 44100)
            self.assertEqual(tuple(call['init_audio'][1].shape), (2, 220500))
            self.assertTrue(torch.any(call['init_audio'][1] != 0))
        manifest = ET.parse(self.root / 'results.xml').getroot()
        self.assertEqual(len(manifest.findall('take')), 3)
        self.assertEqual(manifest.get('amount'), '0.7')
        self.assertEqual(manifest.get('source_name'), self.source.name)


if __name__ == '__main__':
    unittest.main()
