import argparse, csv, hashlib, json, math, random, struct
from pathlib import Path
from drum_maps import DRUM_MAPS, MAP_LABELS, output_note

NOTES = [36,38,42,46,45,47,50,49,51]
MAP = {35:36,36:36,37:38,38:38,40:38,42:42,44:42,46:46,41:45,43:45,45:45,47:47,48:47,50:50,49:49,52:49,55:49,57:49,51:51,53:51,59:51}

def vlq(data, p):
    value = 0
    for _ in range(4):
        b=data[p]; p+=1; value=(value<<7)|(b&127)
        if b<128: return value,p
    raise ValueError('Invalid MIDI variable-length integer')

def read_midi(path):
    data=Path(path).read_bytes()
    if data[:4]!=b'MThd': raise ValueError('Missing MIDI header')
    size=struct.unpack('>I',data[4:8])[0]
    fmt,tracks,ppq=struct.unpack('>HHH',data[8:14])
    if ppq&0x8000 or fmt>1: raise ValueError('Unsupported MIDI timing/format')
    p=8+size; hits=[]; end=0
    for _ in range(tracks):
        if data[p:p+4]!=b'MTrk': raise ValueError('Missing track')
        n=struct.unpack('>I',data[p+4:p+8])[0]; track=data[p+8:p+8+n]; p+=8+n
        q=tick=0; running=None
        while q<len(track):
            dt,q=vlq(track,q); tick+=dt; end=max(end,tick)
            status=track[q]
            if status>=128: q+=1
            else:
                if running is None: raise ValueError('Missing running status')
                status=running
            if status==255:
                kind=track[q]; q+=1; length,q=vlq(track,q); q+=length; running=None
            elif status in (240,247):
                length,q=vlq(track,q); q+=length; running=None
            else:
                running=status; count=1 if status&240 in (192,208) else 2
                args=track[q:q+count]; q+=count
                if status&240==144 and args[1]>0 and args[0] in MAP:
                    hits.append((tick/ppq,MAP[args[0]],args[1]))
    return hits,end/ppq

def examples(root,row):
    if row['time_signature']!='4-4': return []
    hits,end=read_midi(root/row['midi_filename'])
    # Full bars only; retain highest velocity if multiple strikes quantize together.
    bars=[{} for _ in range(int((end+1e-6)//4))]
    for beat,note,vel in hits:
        step=round(beat*4); bar=step//16
        if bar<len(bars):
            key=(step%16,NOTES.index(note)); offset=beat*4-step
            if key not in bars[bar] or bars[bar][key][0]<vel: bars[bar][key]=(vel,offset)
    return bars

def newstats(): return {'bars':0,'hits':[[0]*9 for _ in range(16)],'velocity':[[0]*9 for _ in range(16)],'offset':[[0.]*9 for _ in range(16)]}

def train(root,out):
    rows=list(csv.DictReader((root/'info.csv').open())); groups={'all':newstats()}; splits={}; skipped=0; dig=hashlib.sha256()
    for row in rows:
        splits[row['split']]=splits.get(row['split'],0)+1
        if row['split']!='train': continue
        if row['time_signature']!='4-4': skipped+=1; continue
        bars=examples(root,row); style=row['style'].split('/')[0]; groups.setdefault(style,newstats())
        dig.update((root/row['midi_filename']).read_bytes())
        for bar in bars:
            for name in ('all',style):
                g=groups[name]; g['bars']+=1
                for (s,v),(velocity,offset) in bar.items():
                    g['hits'][s][v]+=1; g['velocity'][s][v]+=velocity; g['offset'][s][v]+=offset
    model={'format':1,'type':'style-conditioned step-frequency baseline','steps':16,'meter':'4/4','notes':NOTES,'groups':groups,'training_sha256':dig.hexdigest(),'source':'Google LLC Groove MIDI Dataset v1.0.0; CC BY 4.0','limitations':'Not guitar-conditioned. Sixteenth-note grid loses triplets and rolls. Independent voices/steps; no learned song structure.'}
    out.mkdir(parents=True,exist_ok=True); (out/'drum_model.json').write_text(json.dumps(model,separators=(',',':')))
    scores={}
    for split in ('validation','test'):
        losses=[0.,0.]; count=bars_seen=0
        for row in rows:
            if row['split']!=split: continue
            g=groups.get(row['style'].split('/')[0],groups['all'])
            for bar in examples(root,row):
                bars_seen+=1
                for s in range(16):
                    for v in range(9):
                        y=(s,v) in bar
                        for k,stats in enumerate((groups['all'],g)):
                            p=(stats['hits'][s][v]+.5)/(stats['bars']+1)
                            losses[k]-=math.log(p if y else 1-p)
                        count+=1
        scores[split]={'bars':bars_seen,'binary_cross_entropy_global':losses[0]/count,'binary_cross_entropy_style':losses[1]/count}
    report={'files_by_split':splits,'training_bars':groups['all']['bars'],'training_non_4_4_files_skipped':skipped,'evaluation':scores,'note':'Lower cross entropy is better; this does not measure musical quality or guitar alignment.'}
    (out/'metrics.json').write_text(json.dumps(report,indent=2)); print(json.dumps(report,indent=2))

def encode_vlq(n):
    b=[n&127]; n>>=7
    while n: b.insert(0,(n&127)|128); n>>=7
    return bytes(b)

def generate(model,path,style,bpm,bars,seed,downbeat=0.0,duration=None,drum_map="gm",pattern="simple",swing=50,density=1,fill_every=0,crashes=False,timekeeper="hihat",accent_kicks=None):
    if drum_map not in DRUM_MAPS: raise ValueError('Unknown drum map: '+str(drum_map))
    if pattern not in ('simple', 'groove', 'learned'): raise ValueError('Unknown pattern')
    if not math.isfinite(bpm) or not 30<=bpm<=300 or not 1<=bars<=128: raise ValueError('BPM 30-300; bars 1-128')
    if not math.isfinite(downbeat) or downbeat<0: raise ValueError('Downbeat must be nonnegative seconds')
    if duration is not None and (not math.isfinite(duration) or duration<=downbeat): raise ValueError('Duration must extend beyond the downbeat')
    if not math.isfinite(swing) or not 50<=swing<=75: raise ValueError('Swing must be 50-75 percent; 50 is straight')
    if density not in (0,1,2) or fill_every not in (0,4,8): raise ValueError('Density 0-2; fills off, every 4 or every 8 bars')
    if timekeeper not in ('hihat','ride'): raise ValueError('Timekeeper must be hihat or ride')
    origin=round(downbeat*bpm/60*480)
    end_tick=round(duration*bpm/60*480) if duration is not None else origin+bars*1920
    events=[]
    used=set()
    def hit(tick,note,velocity):
        if note in (42,46,51) and (tick-origin)%480==240:
            tick+=round(480*swing/100)-240
        if tick>=end_tick: return
        if (tick,note) in used:return
        used.add((tick,note))
        mapped=output_note(note,drum_map)
        events.extend([(tick,bytes([153,mapped,velocity])),(min(tick+60,end_tick),bytes([137,mapped,0]))])
    if pattern=='simple':
        # Identical 4/4 bars: kick 1/3, snare 2/4, closed hat straight eighths.
        # No model offsets, random hits, fills or timing humanization.
        for bar in range(bars):
            base=origin+bar*1920
            for step in range(8): hit(base+step*240,42,75)
            for beat in (0,2): hit(base+beat*480,36,100)
            for beat in (1,3): hit(base+beat*480,38,110)
    elif pattern=='groove':
        for bar in range(bars):
            base=origin+bar*1920
            # Fill only a completed phrase bar; never truncate a fill at source end.
            fill=fill_every>0 and (bar+1)%fill_every==0 and base+1920<=end_tick
            for step in range(8):
                if fill and step>=7: continue
                cymbal=51 if timekeeper=='ride' else (46 if step==7 and bar%2==1 and not fill else 42)
                hit(base+step*240,cymbal,80 if step%2==0 else 65)
            for beat in (0,2): hit(base+beat*480,36,100)
            if density>=1: hit(base+720,36,92)
            if density>=2:
                hit(base+(1200 if bar%2==0 else 1680),36,92)
            for beat in (1,3): hit(base+beat*480,38,110)
            if crashes and bar%4==0: hit(base,49,105)
            if fill:
                for tick,note,velocity in ((1560,50,95),(1680,47,100),(1800,45,105)):
                    hit(base+tick,note,velocity)
    else:
        m=json.loads(model.read_text());g=m['groups'].get(style,m['groups']['all']);rng=random.Random(seed)
        for bar in range(bars):
            for step in range(16):
                for voice,note in enumerate(NOTES):
                    count=g['hits'][step][voice]
                    if rng.random()<count/max(1,g['bars']):
                        velocity=max(1,min(127,round(g['velocity'][step][voice]/max(1,count)+rng.gauss(0,5))))
                        # Legacy probabilities remain optional; all onsets are quantized.
                        hit(origin+bar*1920+step*120,note,velocity)
    if pattern=='groove':
        for tick in accent_kicks or []:
            if tick>=0 and tick%240==0:hit(origin+tick,36,95)
    tempo=round(60000000/bpm)
    data=b'\x00\xff\x51\x03'+tempo.to_bytes(3,'big')+b'\x00\xff\x58\x04\x04\x02\x18\x08'
    label=('Tracksmith GEN / '+pattern+' / swing '+str(swing)+'% / '+MAP_LABELS[drum_map]).encode('utf-8')
    data+=b'\x00\xff\x03'+encode_vlq(len(label))+label
    last=0
    for tick,msg in sorted(events,key=lambda event:event[0]):
        data+=encode_vlq(tick-last)+msg;last=tick
    data+=encode_vlq(max(last,end_tick)-last)+b'\xff\x2f\x00'
    path.write_bytes(b'MThd'+struct.pack('>IHHH',6,0,1,480)+b'MTrk'+struct.pack('>I',len(data))+data)
    print(f'Generated {path} ({len(events)//2} hits; {pattern}; swing {swing}%)')

if __name__=='__main__':
    p=argparse.ArgumentParser(); sub=p.add_subparsers(dest='command',required=True)
    t=sub.add_parser('train'); t.add_argument('dataset',type=Path); t.add_argument('output',type=Path)
    g=sub.add_parser('generate'); g.add_argument('model',type=Path); g.add_argument('output',type=Path); g.add_argument('--style',default='all'); g.add_argument('--bpm',type=float,default=120); g.add_argument('--bars',type=int,default=8); g.add_argument('--seed',type=int,default=42)
    g.add_argument('--drum-map', choices=DRUM_MAPS, default='gm')
    g.add_argument('--pattern', choices=('simple','groove','learned'), default='simple')
    g.add_argument('--swing',type=float,default=50)
    g.add_argument('--density',type=int,choices=(0,1,2),default=1)
    g.add_argument('--fill-every',type=int,choices=(0,4,8),default=0)
    g.add_argument('--crashes',action='store_true')
    g.add_argument('--timekeeper',choices=('hihat','ride'),default='hihat')
    a=p.parse_args()
    if a.command=='train': train(a.dataset,a.output)
    else: generate(a.model,a.output,a.style,a.bpm,a.bars,a.seed,drum_map=a.drum_map,pattern=a.pattern,swing=a.swing,density=a.density,fill_every=a.fill_every,crashes=a.crashes,timekeeper=a.timekeeper)
