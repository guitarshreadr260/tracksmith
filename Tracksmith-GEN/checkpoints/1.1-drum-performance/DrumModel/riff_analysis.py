"""Inspectable guitar-rhythm hypotheses, not pitch transcription or full riff understanding."""
import argparse
import hashlib
import json
import math
import statistics
import struct
import wave
from pathlib import Path


def analyze(source,bpm=140,start_bar=2):
    if not math.isfinite(bpm) or not 30<=bpm<=300:raise ValueError('Tempo must be 30-300 BPM.')
    if not isinstance(start_bar,int) or start_bar<1:raise ValueError('Start bar must be a positive integer.')
    source=Path(source)
    levels=[];highs=[];previous=None
    with wave.open(str(source),'rb') as audio:
        channels,width,rate,frames=audio.getnchannels(),audio.getsampwidth(),audio.getframerate(),audio.getnframes()
        if channels not in (1,2) or width not in (1,2,3,4):raise ValueError('Use mono/stereo PCM WAV, 8/16/24/32 bit.')
        duration=frames/rate
        if duration<=0 or duration>600:raise ValueError('Use a nonempty WAV of ten minutes or less.')
        if math.ceil(duration*bpm/240)>129:raise ValueError('Use at most 128 playing bars plus the lead-in.')
        hop=max(1,round(rate*.005));scale=2**(width*8-1)
        while True:
            raw=audio.readframes(hop)
            if not raw:break
            if width==1:samples=[v-128 for v in raw]
            elif width in (2,4):samples=struct.unpack('<'+('h' if width==2 else 'i')*(len(raw)//width),raw)
            else:samples=[int.from_bytes(raw[i:i+3],'little',signed=True) for i in range(0,len(raw),3)]
            energy=sum(float(v)*v for v in samples)/len(samples)
            # Difference energy per channel preserves stereo even when opposite in phase.
            differences=[]
            for i,v in enumerate(samples):
                before=samples[i-channels] if i>=channels else (previous[i] if previous else v)
                differences.append(float(v-before)**2)
            previous=list(samples[-channels:])
            levels.append(math.sqrt(energy)/scale)
            highs.append(math.sqrt(sum(differences)/len(differences))/scale)
    peak=max(levels,default=0)
    if peak<1e-6:raise ValueError('The recording is silent.')
    start_seconds=(start_bar-1)*240/bpm
    if start_seconds>=duration:raise ValueError('The selected drum start is past the end of this WAV.')
    novelty=[]
    for i in range(len(levels)):
        prior=max(levels[max(0,i-3):i],default=1e-7)
        high_prior=max(highs[max(0,i-3):i],default=1e-7)
        novelty.append(max(0,math.log((levels[i]+1e-7)/(prior+1e-7)))+.5*max(0,math.log((highs[i]+1e-7)/(high_prior+1e-7))))
    candidates=[];last=-1
    for i,value in enumerate(novelty):
        if levels[i]<peak*.02 or value<.18 or value<max(novelty[max(0,i-2):i+3]):continue
        neighborhood=novelty[max(0,i-50):min(len(novelty),i+51)]
        center=statistics.median(neighborhood);mad=statistics.median(abs(v-center) for v in neighborhood)
        if value<center+max(.18,2*mad):continue
        seconds=i*hop/rate
        if seconds-last<.045:continue
        candidates.append((seconds,value));last=seconds
    maximum=max((value for _,value in candidates),default=1)
    onsets=[];seen=set();end_tick=round(duration*bpm/60*480)
    for seconds,value in candidates:
        tick=round(seconds*bpm/60*4)*120
        if tick<(start_bar-1)*1920 or tick>=end_tick or tick in seen:continue
        seen.add(tick)
        onsets.append({'id':len(onsets)+1,'seconds':round(seconds,6),'tick':tick,'strength':round(min(1,(value/maximum)**.4),3),'enabled':True})
    rests=[];begin=None;floor=peak*.025
    for i,level in enumerate(levels+[peak]):
        if level<floor and begin is None:begin=i
        elif level>=floor and begin is not None:
            a,b=begin*hop/rate,min(duration,i*hop/rate)
            if b-a>=max(.18,30/bpm):rests.append({'start_tick':round(a*bpm/60*480),'end_tick':round(b*bpm/60*480)})
            begin=None
    bars=[];patterns=[]
    for index in range(math.ceil(duration*bpm/240)):
        points={round((o['tick']-index*1920)/120) for o in onsets if index*1920<=o['tick']<(index+1)*1920}
        group='Rest' if not points else None
        if points:
            for number,pattern in enumerate(patterns):
                if len(points&pattern)/max(1,len(points|pattern))>=.75:group='R'+str(number+1);break
            if group is None:patterns.append(points);group='R'+str(len(patterns))
        bars.append({'number':index+1,'group':group,'phrase_start':index+1==start_bar,'mute':False})
    digest=hashlib.sha256()
    with source.open('rb') as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b''):digest.update(chunk)
    stride=max(1,math.ceil(len(levels)/2400))
    return {'format':'tracksmith-riff-lab-1','source_name':source.name,'source_sha256':digest.hexdigest(),
            'bpm':bpm,'start_bar':start_bar,'duration_seconds':duration,'end_tick':end_tick,
            'envelope':[round(max(levels[i:i+stride])/peak,4) for i in range(0,len(levels),stride)],
            'onsets':onsets,'rests':rests,'bars':bars,
            'limitations':'Energy and high-frequency-change attack detection; no pitch/chord transcription. Rhythm groups compare bar attack patterns, not riff identity. Quiet spans may include decay/noise rather than true musical rests.'}


def write_lab(source,output,bpm=140,start_bar=2):
    output=Path(output)
    if output.exists():raise ValueError('Choose a new HTML output filename.')
    if output.resolve()==Path(source).resolve():raise ValueError('Keep the source recording intact.')
    data=analyze(source,bpm,start_bar)
    base=Path(__file__).parent
    html=(base/'riff_template.html').read_text(encoding='utf-8')
    html=html.replace('__RIFF_CORE_JS__',(base/'riff_core.js').read_text(encoding='utf-8'))
    html=html.replace('__ANALYSIS_JSON__',json.dumps(data).replace('<','\\u003c'))
    output.write_text(html,encoding='utf-8')
    print('Riff Lab ready:',output)
    return data


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--bpm',type=float,default=140);parser.add_argument('--start-bar',type=int,default=2)
    args=parser.parse_args()
    try:write_lab(args.source,args.output,args.bpm,args.start_bar)
    except (ValueError,wave.Error,OSError) as error:parser.exit(1,str(error)+'\n')
