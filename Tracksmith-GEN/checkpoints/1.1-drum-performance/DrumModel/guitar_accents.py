"""Conservative onset guidance: at most one extra eighth-grid kick per bar.

RMS attacks are not musical understanding or tempo/downbeat inference.
The user's explicit tempo and start position remain authoritative.
"""
import math


def choose_kicks(attacks,bpm,downbeat,duration,bars):
    if not attacks:return []
    strongest=max(a['strength'] for a in attacks)
    if strongest<=0:return []
    choices={}
    for attack in attacks:
        if attack['strength']<strongest*.5:continue
        seconds=attack['seconds']-downbeat
        if seconds<0 or attack['seconds']>=duration:continue
        eighth=round(seconds*bpm/60*2)
        bar,step=divmod(eighth,8)
        # Keep the original backbeat and refrain from layering kicks on snare beats.
        if bar>=bars or step not in (1,3,5,7):continue
        tick=bar*1920+step*240
        if downbeat+tick/480*60/bpm>=duration:continue
        candidate=(attack['strength'],tick)
        if bar not in choices or candidate[0]>choices[bar][0]:choices[bar]=candidate
    return sorted(candidate[1] for candidate in choices.values())
