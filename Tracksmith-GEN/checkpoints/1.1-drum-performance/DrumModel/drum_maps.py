"""Export mappings only. Training and statistical voice indices stay in GM."""
DRUM_MAPS = {
    'gm': {},
    'ggd-okw-metal': {
        36: 24,  # Kick
        38: 26,  # Snare
        42: 43,  # Closed hi-hat tip
        46: 46,  # Open hi-hat 2
        45: 35,  # Low tom -> floor tom 1
        47: 34,  # Middle tom -> rack tom 2
        50: 33,  # High tom -> rack tom 1
        49: 52,  # Crash 1
        51: 62,  # Ride bow
    },
}
MAP_LABELS = {'gm': 'General MIDI', 'ggd-okw-metal': 'GGD One Kit Wonder Metal - factory Invasion'}


def output_note(note, drum_map):
    if drum_map not in DRUM_MAPS:
        raise ValueError('Unknown drum map: ' + str(drum_map))
    if drum_map != 'gm' and note not in DRUM_MAPS[drum_map]:
        raise ValueError('No export mapping for model note ' + str(note))
    return DRUM_MAPS[drum_map].get(note, note)
