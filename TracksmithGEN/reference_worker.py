"""GEN audio-guided variations using the user's existing Stable Audio 3 environment."""
import hashlib
import math
import os
import sys
import time
import traceback
import xml.etree.ElementTree as ET
from pathlib import Path


def stage(message):
    print('GEN_STAGE\t' + message, flush=True)


def prepare(source, start, seconds, output):
    import numpy as np
    import soundfile as sf
    if not source.is_file() or source.suffix.lower() not in ('.wav', '.mp3'):
        raise ValueError('Choose an existing WAV or MP3 recording.')
    if not math.isfinite(start) or start < 0 or not math.isfinite(seconds) or not 5 <= seconds <= 30:
        raise ValueError('Use a nonnegative start and a section from 5 to 30 seconds.')
    with sf.SoundFile(str(source)) as stream:
        if stream.channels not in (1, 2):
            raise ValueError('Use a mono or stereo recording.')
        rate = stream.samplerate
        offset = round(start * rate)
        if offset >= len(stream):
            raise ValueError('The section starts after the end of this recording.')
        stream.seek(offset)
        audio = stream.read(round(seconds * rate), dtype='float32', always_2d=True)
    actual = len(audio) / rate
    if actual < 5:
        raise ValueError('Choose a section with at least five seconds remaining.')
    if not np.isfinite(audio).all() or np.max(np.abs(audio)) < 1e-6:
        raise ValueError('The selected section is silent or contains invalid samples.')
    if audio.shape[1] == 1:
        audio = np.repeat(audio, 2, axis=1)
    # No resampling or normalization is applied to the comparison reference.
    sf.write(str(output), audio, rate, subtype='FLOAT')
    return actual, rate


def run(request):
    import numpy as np
    import soundfile as sf
    root = ET.parse(request).getroot()
    out = request.parent
    source = Path(root.findtext('source', '')).resolve()
    start = float(root.findtext('start', '0'))
    seconds = float(root.findtext('duration', '15'))
    amount = float(root.findtext('amount', '0.5'))
    count = int(root.findtext('count', '1'))
    prompt = root.findtext('prompt', '').strip()
    mode = root.findtext('mode', 'generate')
    if mode not in ('prepare', 'generate'):
        raise ValueError('Unknown reference operation.')
    if not 0.05 <= amount <= 0.95 or not 1 <= count <= 3:
        raise ValueError('Use 5-95% variation and 1-3 takes.')
    if mode == 'generate' and not 1 <= len(prompt) <= 512:
        raise ValueError('Describe the variation using 1-512 characters.')
    reference = out / 'reference.wav'
    if source == reference:
        raise ValueError('Input and prepared output must be different files.')
    stage('Preparing your selected audio section')
    actual, rate = prepare(source, start, seconds, reference)
    manifest = ET.Element('results', model='stable-audio-3-small-music', device='cuda',
        source_name=source.name, start=str(start), duration=str(actual), amount=str(amount),
        reference=reference.name, prompt=prompt)
    ET.ElementTree(manifest).write(out / 'results.xml', encoding='utf-8', xml_declaration=True)
    if mode == 'prepare':
        stage('Reference ready for comparison')
        return
    import torch
    import torchaudio
    from stable_audio_3 import StableAudioModel
    if not torch.cuda.is_available():
        raise RuntimeError('CUDA is unavailable in the selected Stable Audio environment.')
    # Models were installed and accepted during the prior trial. Do not download
    # missing assets silently during a reference job.
    stage('Loading cached Stable Audio 3 Small Music on ' + torch.cuda.get_device_name(0))
    model = StableAudioModel.from_pretrained('small-music', device='cuda', model_half=True)
    data, source_rate = sf.read(str(reference), dtype='float32', always_2d=True)
    wave = torch.from_numpy(data.T.copy())
    model_rate = int(model.model.sample_rate)
    if source_rate != model_rate:
        wave = torchaudio.functional.resample(wave, source_rate, model_rate)
    base_seed = int(root.findtext('seed', '-1'))
    if base_seed < 0:
        base_seed = int.from_bytes(os.urandom(4), 'little') % 2147483647
    manifest.set('gpu', torch.cuda.get_device_name(0))
    for index in range(count):
        stage('Generating audio-guided variation ' + str(index + 1) + ' of ' + str(count))
        torch.cuda.reset_peak_memory_stats()
        torch.cuda.synchronize()
        started = time.perf_counter()
        chosen_seed = base_seed + index
        generated = model.generate(prompt=prompt, duration=actual, steps=8, cfg_scale=1.0,
            seed=chosen_seed, batch_size=1, sample_size=model.model_config['sample_size'],
            init_audio=(model_rate, wave), init_noise_level=amount, chunked_decode=True)
        torch.cuda.synchronize()
        audio = generated[0].detach().float().cpu().numpy().T
        if audio.ndim != 2 or audio.shape[1] != 2 or not np.isfinite(audio).all():
            raise RuntimeError('The model returned invalid stereo samples.')
        if abs(len(audio) / model_rate - actual) > 0.1 or np.max(np.abs(audio)) < 1e-6:
            raise RuntimeError('The model returned silence or an unexpected duration.')
        name = 'variation-' + str(index + 1) + '.wav'
        sf.write(str(out / name), audio, model_rate, subtype='PCM_24')
        ET.SubElement(manifest, 'take', file=name, seed=str(chosen_seed),
            seconds=str(round(time.perf_counter() - started, 2)),
            peak_vram_gb=str(round(torch.cuda.max_memory_allocated() / 1024**3, 3)))
        ET.ElementTree(manifest).write(out / 'results.xml', encoding='utf-8', xml_declaration=True)
        del generated, audio
    stage('Audio-guided variations ready')


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
