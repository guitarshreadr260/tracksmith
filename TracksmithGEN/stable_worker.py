"""Text-only Stable Audio 3 Small Music worker for Multi Platinum."""
import os
import sys
import time
import traceback
import xml.etree.ElementTree as ET
from pathlib import Path


def stage(message):
    print('GEN_STAGE\t' + message, flush=True)


def run(request):
    root = ET.parse(request).getroot()
    prompt = root.findtext('caption', '').strip()
    seconds = int(root.findtext('duration', '15'))
    count = int(root.findtext('count', '1'))
    seed = int(root.findtext('seed', '-1'))
    if not prompt:
        raise ValueError('Describe the music you want first.')
    if not 10 <= seconds <= 120 or not 1 <= count <= 3:
        raise ValueError('Use 10-120 seconds and 1-3 takes.')
    import numpy as np
    import soundfile as sf
    import torch
    from stable_audio_3 import StableAudioModel
    if not torch.cuda.is_available():
        raise RuntimeError('CUDA is unavailable. Choose the working Stable Audio .venv in Engine settings.')
    out = request.parent
    stage('Loading cached Stable Audio 3 Small Music on ' + torch.cuda.get_device_name(0))
    model = StableAudioModel.from_pretrained('small-music', device='cuda', model_half=True)
    manifest = ET.Element('results', model='stable-audio-3-small-music', device='cuda',
        gpu=torch.cuda.get_device_name(0), profile='small-music')
    if seed < 0:
        seed = int.from_bytes(os.urandom(4), 'little') % 2147483647
    for index in range(count):
        stage('Stable Audio 3: creating take ' + str(index + 1) + ' of ' + str(count))
        torch.cuda.reset_peak_memory_stats()
        torch.cuda.synchronize()
        started = time.perf_counter()
        generated = model.generate(prompt=prompt, duration=seconds, seed=seed + index,
            steps=8, cfg_scale=1.0, batch_size=1,
            sample_size=model.model_config['sample_size'], chunked_decode=True)
        torch.cuda.synchronize()
        audio = generated[0].detach().float().cpu().numpy().T
        rate = int(model.model.sample_rate)
        if audio.ndim != 2 or audio.shape[1] != 2 or not np.isfinite(audio).all():
            raise RuntimeError('The model returned invalid stereo audio.')
        if abs(len(audio) / rate - seconds) > .1 or np.max(np.abs(audio)) < 1e-6:
            raise RuntimeError('The model returned silence or an unexpected duration.')
        name = 'take-' + str(index + 1) + '.wav'
        sf.write(out / name, audio, rate, subtype='PCM_24')
        take = ET.SubElement(manifest, 'take', file=name, seed=str(seed + index),
            seconds=str(round(time.perf_counter() - started, 2)),
            peak_vram_gb=str(round(torch.cuda.max_memory_allocated() / 1024**3, 3)))
        ET.SubElement(take, 'caption').text = prompt
        ET.ElementTree(manifest).write(out / 'results.xml', encoding='utf-8', xml_declaration=True)
        del generated, audio
        stage('Stable Audio 3 take ' + str(index + 1) + ' ready')
    stage('Stable Audio 3 generation complete')


if __name__ == '__main__':
    try:
        os.environ['HF_HUB_OFFLINE'] = '1'
        os.environ['TRANSFORMERS_OFFLINE'] = '1'
        run(Path(sys.argv[1]).resolve())
    except Exception as exc:
        print('GEN_ERROR\t' + str(exc), flush=True)
        if len(sys.argv) > 1:
            Path(sys.argv[1]).resolve().parent.joinpath('failure.txt').write_text(traceback.format_exc(), encoding='utf-8')
        sys.exit(1)
