"""Verify the GEN worker forwards long prompt text without selector defaults."""
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch
import xml.etree.ElementTree as ET

import numpy as np
import soundfile as sf
import ace_worker


class AcePromptTests(unittest.TestCase):
    def test_long_caption_forwarding(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            request = ET.Element('request')
            prompt = ('Heavy guitars in D minor, 110 BPM, natural dynamics. ' * 40) + 'END OF PROMPT'
            for key, value in dict(repo=str(root), caption=prompt, duration=10, count=1,
                                   bpm=0, key='', seed=42, profile='direct').items():
                ET.SubElement(request, key).text = str(value)
            path = root / 'request.xml'
            ET.ElementTree(request).write(path)
            calls = []
            class Handler:
                def initialize_service(self, **kwargs):
                    return 'ready', True
            def generate(dit, lm, params, config, save_dir):
                calls.append(params)
                dest = root / 'raw-output.wav'
                sf.write(dest, np.ones((441000, 2), dtype=np.float32) * .1, 44100)
                return types.SimpleNamespace(success=True, audios=[{'path': str(dest)}])
            cuda = types.SimpleNamespace(is_available=lambda: True, get_device_name=lambda _: 'Test GPU',
                reset_peak_memory_stats=lambda: None, max_memory_allocated=lambda: 1024)
            logger = types.SimpleNamespace(remove=lambda: None, add=lambda *args, **kw: None)
            fake = {'torch': types.SimpleNamespace(cuda=cuda), 'loguru': types.SimpleNamespace(logger=logger),
                'acestep': types.ModuleType('acestep'),
                'acestep.handler': types.SimpleNamespace(AceStepHandler=Handler),
                'acestep.llm_inference': types.SimpleNamespace(LLMHandler=object),
                'acestep.inference': types.SimpleNamespace(GenerationParams=types.SimpleNamespace,
                    GenerationConfig=types.SimpleNamespace, generate_music=generate)}
            with patch.dict('sys.modules', fake), patch('sys.argv', ['ace_worker.py', str(path)]):
                ace_worker.main()
            self.assertEqual(len(calls), 1)
            self.assertEqual(calls[0].caption, prompt)
            self.assertIsNone(calls[0].bpm)
            self.assertEqual(calls[0].keyscale, '')
            manifest = ET.parse(root / 'results.xml').getroot()
            self.assertEqual(manifest.find('take/caption').text, prompt)


if __name__ == '__main__':
    unittest.main()
