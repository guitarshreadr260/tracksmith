"""Render aligned stem WAV files into a 32-bit floating-point custom mix."""
from contextlib import ExitStack
import numpy as np
import soundfile as sf

def render_mix(paths, output, levels=None, subtype='FLOAT', format='WAV'):
    levels = levels if levels is not None else [1.0] * len(paths)
    if len(levels) != len(paths) or any(not np.isfinite(g) or g < 0 for g in levels):
        raise ValueError('Invalid stem levels.')
    with ExitStack() as stack:
        sources = [stack.enter_context(sf.SoundFile(str(p))) for p in paths]
        if not sources: raise ValueError('Choose at least one stem.')
        rate, channels, frames = sources[0].samplerate, sources[0].channels, len(sources[0])
        if any((f.samplerate, f.channels, len(f)) != (rate, channels, frames) for f in sources):
            raise ValueError('Stems must have matching sample rates, channels and lengths.')
        peak = 0.0
        while True:
            blocks = [f.read(65536, dtype='float32', always_2d=True) for f in sources]
            if not len(blocks[0]): break
            mixed = sum((block * gain for block, gain in zip(blocks, levels)), np.zeros_like(blocks[0]))
            if not np.isfinite(mixed).all(): raise ValueError('Audio contains invalid samples.')
            peak = max(peak, float(np.max(np.abs(mixed))))
        ceiling = 0.95 if format == 'MP3' else 0.999
        gain = min(1.0, ceiling / peak) if peak else 1.0
        for f in sources: f.seek(0)
        with sf.SoundFile(str(output), mode='w', samplerate=rate, channels=channels, subtype=subtype, format=format) as dest:
            while True:
                blocks = [f.read(65536, dtype='float32', always_2d=True) for f in sources]
                if not len(blocks[0]): break
                dest.write(sum((block * level for block, level in zip(blocks, levels)), np.zeros_like(blocks[0])) * gain)
        return gain

