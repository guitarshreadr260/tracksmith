"""Test text-only Stable Audio routing without an ACE dependency or source WAV."""
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch
import xml.etree.ElementTree as ET

import numpy as np
import torch
import stable_worker


class StableWorkerTests(unittest.TestCase):
    def test_text_generation_preserves_prompt_and_seeds(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            prompt = ('Rock instrumental in D minor, 110 BPM. ' * 30) + 'FINAL INSTRUCTION'
            request = ET.Element('request')
            for key, value in dict(caption=prompt, duration=10, count=2, seed=91).items():
                ET.SubElement(request, key).text = str(value)
            path = root / 'request.xml'
            ET.ElementTree(request).write(path)
            calls = []
            class Model:
                model = types.SimpleNamespace(sample_rate=44100)
                model_config = {'sample_size': 5292032}
                @classmethod
                def from_pretrained(cls, name, **kwargs):
                    self.assertEqual(name, 'small-music')
                    return cls()
                def generate(self, **kwargs):
                    calls.append(kwargs)
                    return torch.ones((1, 2, 441000)) * .1
            with patch.dict('sys.modules', {'stable_audio_3': types.SimpleNamespace(StableAudioModel=Model)}), \
                 patch.object(torch.cuda, 'is_available', return_value=True), \
                 patch.object(torch.cuda, 'get_device_name', return_value='Test GPU'), \
                 patch.object(torch.cuda, 'reset_peak_memory_stats'), \
                 patch.object(torch.cuda, 'synchronize'), \
                 patch.object(torch.cuda, 'max_memory_allocated', return_value=100):
                stable_worker.run(path)
            self.assertEqual([call['seed'] for call in calls], [91, 92])
            for call in calls:
                self.assertEqual(call['prompt'], prompt)
                self.assertNotIn('init_audio', call)
                self.assertNotIn('bpm', call)
                self.assertNotIn('keyscale', call)
            manifest = ET.parse(root / 'results.xml').getroot()
            self.assertEqual(manifest.get('model'), 'stable-audio-3-small-music')
            self.assertEqual(len(manifest.findall('take')), 2)
            self.assertEqual(manifest.find('take/caption').text, prompt)

    def test_rejects_invalid_request_before_model_loading(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'request.xml'
            for prompt, duration, count in [('', 10, 1), ('rock', 121, 1), ('rock', 10, 4)]:
                with self.subTest(prompt=prompt, duration=duration, count=count):
                    root = ET.Element('request')
                    for key, value in dict(caption=prompt, duration=duration, count=count).items():
                        ET.SubElement(root, key).text = str(value)
                    ET.ElementTree(root).write(path)
                    with self.assertRaises(ValueError):
                        stable_worker.run(path)


if __name__ == '__main__':
    unittest.main()
