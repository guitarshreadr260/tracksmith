"""Align the statistical drum baseline to a recording's ORIGINAL file timeline.

This schedules MIDI; it does not learn guitar-to-drum relationships or time-warp
generative audio. Beat zero is explicit because a guitar attack is not necessarily
a musical downbeat. The source recording is never changed.
"""
import argparse
import hashlib
import json
import math
import struct
import wave
from pathlib import Path
from drum_baseline import generate
from guitar_accents import choose_kicks
from drum_maps import DRUM_MAPS, MAP_LABELS


def inspect_wav(path):
    """Read PCM WAV in bounded blocks, preserving leading silence in all times."""
    envelope = []
    with wave.open(str(path), 'rb') as audio:
        channels, width, rate, frames = (audio.getnchannels(), audio.getsampwidth(),
                                         audio.getframerate(), audio.getnframes())
        if channels not in (1, 2) or width not in (1, 2, 3, 4):
            raise ValueError('Use mono/stereo PCM WAV (8, 16, 24 or 32 bit).')
        hop = max(1, round(rate * .01))
        while True:
            block = audio.readframes(hop)
            if not block:
                break
            if width == 1:
                values = [v - 128 for v in block]
            elif width in (2, 4):
                values = struct.unpack('<' + ('h' if width == 2 else 'i') * (len(block)//width), block)
            else:
                values = [int.from_bytes(block[i:i+3], 'little', signed=True)
                          for i in range(0, len(block), 3)]
            envelope.append(math.sqrt(sum(float(v)*v for v in values)/len(values)) / (2**(width*8-1)))
    if frames == 0 or not envelope or max(envelope) < 1e-6:
        raise ValueError('The recording is empty or silent.')
    if frames / rate > 600:
        raise ValueError('Use a recording of ten minutes or less.')
    # Rising RMS edges, reported in ORIGINAL file seconds. No leading trim.
    novelty = [max(0, v - (envelope[i-1] if i else 0)) for i, v in enumerate(envelope)]
    threshold = max(novelty) * .15
    attacks = []
    for i, strength in enumerate(novelty):
        if strength >= threshold and strength == max(novelty[max(0,i-2):i+3]):
            t = i * hop / rate
            if not attacks or t - attacks[-1]['seconds'] >= .06:
                attacks.append({'seconds': round(t, 6), 'strength': round(strength, 6)})
    return {'duration_seconds': frames/rate, 'sample_rate': rate,
            'channels': channels, 'attacks': attacks}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path, help='New MIDI path; import at time zero beside original audio')
    parser.add_argument('--bpm', required=True, type=float, help='Known recording tempo, e.g. 140')
    start_group = parser.add_mutually_exclusive_group()
    start_group.add_argument('--downbeat', type=float, help='Legacy explicit ORIGINAL file seconds')
    start_group.add_argument('--start-bar', type=int, help='First drum bar, counted from file zero as bar 1; default 2')
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--style', default='all')
    parser.add_argument('--model', type=Path, default=Path(__file__).parent/'trained'/'drum_model.json')
    parser.add_argument('--drum-map', choices=DRUM_MAPS, default='ggd-okw-metal', help='Output note map; default is the GGD OKW Metal factory map')
    parser.add_argument('--pattern', choices=('simple','groove','learned'), default='simple')
    parser.add_argument('--swing',type=float,default=50)
    parser.add_argument('--density',type=int,choices=(0,1,2),default=1)
    parser.add_argument('--fill-every',type=int,choices=(0,4,8),default=0)
    parser.add_argument('--crashes',action='store_true')
    parser.add_argument('--timekeeper',choices=('hihat','ride'),default='hihat')
    parser.add_argument('--follow-accents',action='store_true',help='Add at most one grid-snapped kick per bar from strong RMS attacks')
    args = parser.parse_args()
    if not math.isfinite(args.bpm) or not 30 <= args.bpm <= 300:
        parser.error('BPM must be 30-300.')
    if args.start_bar is not None and args.start_bar < 1:
        parser.error('Start bar must be 1 or later.')
    if args.downbeat is None:
        args.downbeat = ((args.start_bar if args.start_bar is not None else 2)-1)*240/args.bpm
    if not math.isfinite(args.downbeat) or args.downbeat < 0:
        parser.error('Downbeat must be nonnegative seconds.')
    report_path = args.output.with_suffix('.alignment.json')
    if args.source.resolve() in (args.output.resolve(), report_path.resolve()):
        parser.error('Outputs must differ from the source recording.')
    if args.output.exists() or report_path.exists():
        parser.error('Choose a new output filename; an output already exists.')
    info = inspect_wav(args.source)
    if args.downbeat >= info['duration_seconds']:
        parser.error('The downbeat is past the end of the recording.')
    bars = math.ceil((info['duration_seconds']-args.downbeat)*args.bpm/240)
    if bars > 128:
        parser.error('Select a shorter recording (maximum 128 bars).')
    source_hash = hashlib.sha256(args.source.read_bytes()).hexdigest()
    accents=choose_kicks(info['attacks'],args.bpm,args.downbeat,info['duration_seconds'],bars) if args.follow_accents and args.pattern=='groove' else []
    generate(args.model, args.output, args.style, args.bpm, bars, args.seed,
             downbeat=args.downbeat, duration=info['duration_seconds'], drum_map=args.drum_map, pattern=args.pattern, swing=args.swing, density=args.density, fill_every=args.fill_every, crashes=args.crashes, timekeeper=args.timekeeper, accent_kicks=accents)
    beat_seconds = 60/args.bpm
    for attack in info['attacks']:
        beat = (attack['seconds']-args.downbeat)/beat_seconds
        attack['nearest_sixteenth_error_ms'] = round((beat-round(beat*4)/4)*beat_seconds*1000, 3)
    info.update({'accent_guidance': bool(args.follow_accents and args.pattern=='groove'), 'candidate_accent_kicks_relative_ticks': accents, 'start_bar': 1 + args.downbeat*args.bpm/240, 'pattern': args.pattern, 'grid_locked': args.swing==50, 'kick_snare_grid_locked': True, 'swing_percent': args.swing, 'swing_scope': 'offbeat hi-hat/open-hat/ride only', 'density': args.density, 'fill_every_bars': args.fill_every if args.pattern=='groove' else 0, 'crashes': args.crashes if args.pattern=='groove' else False, 'timekeeper': args.timekeeper, 'timing_humanization': False, 'drum_map': args.drum_map, 'drum_map_label': MAP_LABELS[args.drum_map], 'export_note_map': DRUM_MAPS[args.drum_map], 'bpm': args.bpm, 'downbeat_seconds': args.downbeat,
                 'tempo_source': 'explicit user tempo', 'bars': bars,
                 'source_sha256': source_hash, 'seed': args.seed,
                 'import_instruction': 'Import MIDI and ORIGINAL audio at timeline zero. Use the MIDI tempo.',
                 'limitations': 'Fixed 4/4 tempo. Attack diagnostics do not infer downbeats. Optional guidance uses strong RMS attacks only; it does not understand riffs or song structure. Simple mode repeats a fixed backbeat; Kick/snare stay on grid. Selected swing delays only offbeat timekeeping cymbals.'})
    report_path.write_text(json.dumps(info, indent=2), encoding='utf-8')
    print('Aligned MIDI ready. Import at zero alongside the unchanged source WAV.')


if __name__ == '__main__':
    main()
