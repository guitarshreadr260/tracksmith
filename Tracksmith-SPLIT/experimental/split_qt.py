"""Tracksmith SPLIT: Qt studio preview."""
import os, sys, tempfile, subprocess, traceback, winsound, shutil
from pathlib import Path
from urllib.parse import urlparse
import numpy as np
import soundfile as sf
from PySide6.QtCore import Qt, QThread, Signal, QRectF, QTimer
from PySide6.QtGui import QColor, QPainter, QPen, QFont, QPainterPath
from PySide6.QtMultimedia import QAudioFormat, QAudioSink, QMediaDevices
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QFrame, QLabel,
    QHBoxLayout, QVBoxLayout, QPushButton, QComboBox, QFileDialog, QMessageBox,
    QSlider, QCheckBox, QProgressBar, QPlainTextEdit, QScrollArea, QInputDialog)
from mix_engine import render_mix

COLORS=['#38d7cd','#f5a044','#ef6a75','#75ca75','#ba80e4','#5ea7f4']

class Worker(QThread):
    done=Signal(object); error=Signal(str); log=Signal(str)
    def __init__(self, fn):
        super().__init__(); self.fn=fn
    def run(self):
        try: self.done.emit(self.fn(self.log.emit))
        except Exception as e: self.error.emit(str(e))

class Waves(QWidget):
    def __init__(self):
        super().__init__(); self.lanes=[]; self.setMinimumHeight(200)
    def paintEvent(self,event):
        p=QPainter(self); p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(),QColor('#0c1013')); w=self.width(); h=self.height()
        p.setPen(QColor('#203042'))
        for x in range(110,w,90): p.drawLine(x,0,x,h)
        if not self.lanes:
            p.setPen(QColor('#dceafb')); p.setFont(QFont('Segoe UI',13,QFont.Weight.DemiBold)); p.drawText(self.rect(),Qt.AlignmentFlag.AlignCenter,'YOUR SESSION STARTS HERE')
            p.setPen(QColor('#8194a9')); p.setFont(QFont('Segoe UI',9)); p.drawText(QRectF(0,h/2+22,w,35),Qt.AlignmentFlag.AlignCenter,'Open a song or load existing stems')
            return
        row=h/len(self.lanes)
        for i,(name,peaks) in enumerate(self.lanes):
            y=(i+.5)*row; p.setPen(QColor('#c6d6e6')); p.setFont(QFont('Segoe UI',9,QFont.Weight.DemiBold)); p.drawText(QRectF(12,y-12,95,24),Qt.AlignmentFlag.AlignVCenter,name.upper())
            p.setPen(QColor('#253647')); p.drawLine(108,round((i+1)*row),w,round((i+1)*row))
            if peaks:
                path=QPainterPath(); path.moveTo(110,y)
                for j,v in enumerate(peaks):path.lineTo(110+j*max(1,w-120)/max(1,len(peaks)-1),y-min(1,v)*row*.44)
                for j in range(len(peaks)-1,-1,-1):path.lineTo(110+j*max(1,w-120)/max(1,len(peaks)-1),y+min(1,peaks[j])*row*.44)
                path.closeSubpath(); p.fillPath(path,QColor(COLORS[i%len(COLORS)]))

class Meter(QWidget):
    def __init__(self):
        super().__init__(); self.levels=[]; self.setFixedWidth(24); self.setMinimumHeight(120)
    def set_levels(self, levels): self.levels=levels; self.update()
    def paintEvent(self,event):
        p=QPainter(self); p.fillRect(self.rect(),QColor('#0b0d0c')); count=max(1,len(self.levels)); width=8 if count>1 else 12
        for c in range(min(count,2)):
            level=self.levels[c] if self.levels else 0; db=20*np.log10(max(level,1e-6)); normalized=max(0,min(1,(db+60)/60))
            for j in range(24):
                threshold=(j+1)/24; color=('#d95047' if j>=22 else '#eab543' if j>=19 else '#4cc873') if normalized>=threshold else '#253127'
                p.fillRect(2+c*(width+3),round(self.height()-4-(j+1)*(self.height()-8)/24),width,max(2,round((self.height()-8)/24-2)),QColor(color))

class Strip(QFrame):
    def __init__(self,path,color,preview,export_stem):
        super().__init__(); self.path=path; self.setObjectName('strip')
        lay=QVBoxLayout(self); lay.setContentsMargins(5,5,5,8); lay.setSpacing(5)
        name=QLabel(path.stem.upper()); name.setAlignment(Qt.AlignmentFlag.AlignCenter); name.setStyleSheet('background:#1c1d1d;color:'+color+';border-radius:4px;padding:7px;font-weight:700;'); lay.addWidget(name)
        controls=QHBoxLayout(); controls.addStretch(); self.meter=Meter(); controls.addWidget(self.meter); controls.addSpacing(12)
        self.slider=QSlider(Qt.Orientation.Vertical); self.slider.setRange(0,150); self.slider.setValue(100); self.slider.setMinimumHeight(140); controls.addWidget(self.slider); controls.addStretch(); lay.addLayout(controls,1)
        self.value=QLabel('100%'); self.value.setAlignment(Qt.AlignmentFlag.AlignCenter); self.value.setObjectName('muted'); lay.addWidget(self.value)
        self.slider.valueChanged.connect(lambda v:self.value.setText(str(v)+'%'))
        self.include=QCheckBox('Include'); self.include.setChecked(True); lay.addWidget(self.include,0,Qt.AlignmentFlag.AlignHCenter)
        exp=QPushButton('EXPORT STEM'); exp.setObjectName('stemExport'); exp.clicked.connect(lambda:export_stem(path)); lay.addWidget(exp)
        btn=QPushButton('Play stem'); btn.clicked.connect(lambda:preview(path)); lay.addWidget(btn)

class Window(QMainWindow):
    def __init__(self):
        super().__init__(); self.setWindowTitle('Tracksmith SPLIT - Qt Studio'); self.resize(1480,900); self.setMinimumSize(1050,700)
        self.work=None; self.pending_wave=None; self.pending_import=None; self.strips=[]; self.result=None; self.source=''; self.output=str(Path.home()/'Music'/'Tracksmith SPLIT'); self.preview_file=str(Path(tempfile.gettempdir())/('split-qt-'+str(os.getpid())+'.wav'))
        self.audio_sink=None; self.audio_stream=None; self.audio_files=[]; self.audio_pending=b''; self.audio_timer=QTimer(self); self.audio_timer.timeout.connect(self.audio_pump); self.play_start_ms=0; self.play_generation=0
        self.setStyleSheet('''QMainWindow,QWidget{background:#090a0b;color:#ede6da;font-family:Segoe UI;font-size:10pt} QFrame#panel{background:#171a1c;border:1px solid #514537;border-radius:6px} QFrame#strip{background:#1c1d1d;border:1px solid #66513a;border-radius:5px} QLabel{background:transparent;border:0} QLabel#muted{color:#ad9d89;font-size:9pt} QLabel#heading{font-weight:700;font-size:11pt;color:#efe2d0} QPushButton{background:#302f2d;border:1px solid #65513a;border-radius:4px;padding:8px;color:#efe4d5} QPushButton:hover{background:#493d2d} QPushButton#accent{background:#cc852f;border-color:#f0b256;font-weight:700} QPushButton#accent:hover{background:#e59d40} QPushButton:disabled{color:#778797;background:#263443} QComboBox{background:#151819;border:1px solid #514537;border-radius:4px;padding:7px;color:#f1e5d5} QComboBox QAbstractItemView{background:#172330;color:#e7f1ff;selection-background-color:#2867a9} QSlider::groove:vertical{background:#0b121b;width:12px;border-radius:5px} QSlider::sub-page:vertical{background:#0b121b} QSlider::add-page:vertical{background:#b78549;border-radius:5px} QSlider::handle:vertical{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #ebe8db,stop:0.25 #999182,stop:0.48 #3d3b36,stop:0.72 #b2ab99,stop:1 #403d38);border:1px solid #9d8c76;height:21px;margin:0 -6px;border-radius:2px} QPushButton#stemExport{background:#30291f;border:1px solid #9c7139;color:#f2c277;font-weight:700;padding:5px} QCheckBox{background:transparent} QPlainTextEdit{background:#0c1013;border:1px solid #443b30;color:#b7a894} QProgressBar{background:#0c1013;border:0;height:5px} QProgressBar::chunk{background:#d59840}''')
        root=QWidget(); self.setCentralWidget(root); outer=QVBoxLayout(root); outer.setContentsMargins(10,10,10,10); outer.setSpacing(8)
        head=QFrame(); head.setObjectName('panel'); h=QHBoxLayout(head); h.setContentsMargins(16,8,16,8); h.setSpacing(10)
        brand=QLabel('<b>TRACKSMITH</b> <span style="color:#eaa63c">SPLIT</span><br><small>MUSIC STEM SEPARATION</small>'); brand.setMinimumWidth(230); brand.setStyleSheet('font-size:19px;letter-spacing:1px'); h.addWidget(brand)
        h.addWidget(self.button('?   PLAY MIX',self.preview_mix,True)); h.addWidget(self.button('�   STOP',self.stop_audio))
        h.addWidget(self.label('LIVE MIX  /  FADERS UPDATE DURING PLAYBACK','muted')); h.addStretch()
        self.source_label=self.label('No audio selected','muted'); self.source_label.setMinimumWidth(150); h.addWidget(self.source_label)
        h.addWidget(self.button('OPEN AUDIO',self.pick_source)); h.addWidget(self.button('YOUTUBE LINK',self.import_youtube)); outer.addWidget(head)
        settings=QFrame(); settings.setObjectName('panel'); st=QHBoxLayout(settings); st.setContentsMargins(12,6,12,6); st.addWidget(self.label('SEPARATION','heading'))
        self.model=QComboBox(); self.model.addItem('6 stems - guitar + piano','htdemucs_6s'); self.model.addItem('4 stems - quality','htdemucs_ft'); self.model.addItem('4 stems - standard','htdemucs'); st.addWidget(self.model)
        self.device=QComboBox(); self.device.addItems(['cuda','cpu']); st.addWidget(self.device)
        self.start=self.button('SEPARATE STEMS',self.separate,True); st.addWidget(self.start)
        st.addWidget(self.button('LOAD STEMS',self.load_stems)); st.addWidget(self.button('OUTPUT FOLDER',self.pick_output))
        st.addWidget(self.button('SPLIT GUITAR',lambda:self.advanced_split('guitar'))); st.addWidget(self.button('SPLIT DRUMS',lambda:self.advanced_split('drums')))
        self.output_label=self.label(self.output,'muted'); st.addWidget(self.output_label,1)
        self.progress=QProgressBar(); self.progress.setTextVisible(False); self.progress.setRange(0,1); self.progress.setFixedWidth(70); st.addWidget(self.progress); outer.addWidget(settings)
        self.wave=Waves(); self.wave.setMinimumHeight(200); outer.addWidget(self.wave,3)
        position=QHBoxLayout(); position.addWidget(self.label('POSITION','muted')); self.seek=QSlider(Qt.Orientation.Horizontal); self.seek.setRange(0,0); self.seek.setEnabled(False); self.seek.sliderReleased.connect(self.seek_released); self.seek.valueChanged.connect(self.update_time_label); position.addWidget(self.seek,1); self.time_label=self.label('0:00 / 0:00','muted'); position.addWidget(self.time_label); outer.addLayout(position)
        bottom=QHBoxLayout(); bottom.setSpacing(7); outer.addLayout(bottom,3)
        self.mixer_container=QWidget(); self.mixer=QHBoxLayout(self.mixer_container); self.mixer.setContentsMargins(0,0,0,0); self.mixer.setSpacing(6); self.mixer_scroll=QScrollArea(); self.mixer_scroll.setWidgetResizable(True); self.mixer_scroll.setWidget(self.mixer_container); bottom.addWidget(self.mixer_scroll,1)
        self.empty=self.label('Separate a song or load existing stems to populate the mixer.','muted'); self.mixer.addWidget(self.empty)
        right,r=self.panel(); right.setFixedWidth(235); bottom.addWidget(right); r.addWidget(self.label('CUSTOM MIX  /  EXPORT','heading'))
        info=self.label('Select included channels and adjust levels before export.','muted'); info.setWordWrap(True); r.addWidget(info); r.addSpacing(10)
        r.addWidget(self.label('OUTPUT FORMAT','muted')); self.export_format=QComboBox(); self.export_format.addItem('WAV  -  32-bit float',('wav','WAV','FLOAT')); self.export_format.addItem('FLAC  -  24-bit lossless',('flac','FLAC','PCM_24')); self.export_format.addItem('MP3  -  compressed',('mp3','MP3','MPEG_LAYER_III')); r.addWidget(self.export_format); r.addSpacing(15)
        r.addWidget(self.button('EXPORT MIX',self.export,True)); r.addWidget(self.button('OPEN STEM FOLDER',self.open_folder)); r.addStretch()
        r.addWidget(self.label('Peak protection on export','muted'))
        self.log=QPlainTextEdit(); self.log.setReadOnly(True); self.log.setMaximumHeight(52); self.log.appendPlainText('Ready. Guitar is a combined stem; lead / rhythm separation is not yet available.'); outer.addWidget(self.log)
    def label(self,text,obj=None):
        x=QLabel(text); x.setObjectName(obj or ''); return x
    def button(self,text,fn,accent=False):
        b=QPushButton(text); b.setObjectName('accent' if accent else ''); b.clicked.connect(fn); return b
    def panel(self):
        x=QFrame(); x.setObjectName('panel'); l=QVBoxLayout(x); l.setContentsMargins(14,14,14,14); l.setSpacing(9); return x,l
    def run_worker(self,fn,callback):
        if self.work: return
        self.work=Worker(fn); self.start.setEnabled(False); self.progress.setRange(0,0); self.work.log.connect(self.log.appendPlainText); self.work.done.connect(callback); self.work.error.connect(lambda e:QMessageBox.critical(self,'Operation failed',e)); self.work.finished.connect(self.finished); self.work.start()
    def finished(self):
        self.progress.setRange(0,1); self.progress.setValue(0); self.start.setEnabled(True); self.work=None
        if self.pending_wave:
            calc=self.pending_wave; self.pending_wave=None
            self.run_worker(calc,lambda data:self.set_waves(data))
        elif self.pending_import:
            source=self.pending_import; self.pending_import=None; self.source=source; self.source_label.setText(Path(source).name)
            self.log.appendPlainText('YouTube audio imported. Starting separation...'); QTimer.singleShot(0,self.separate)
    def pick_source(self):
        p,_=QFileDialog.getOpenFileName(self,'Choose audio','','Audio (*.wav *.mp3 *.flac *.ogg *.m4a *.aiff);;All files (*)')
        if p: self.source=p; self.source_label.setText(Path(p).name)
    def import_youtube(self):
        if self.work:return
        url,ok=QInputDialog.getText(self,'YouTube import','Paste a YouTube video link:')
        if not ok or not url.strip():return
        url=url.strip(); parsed=urlparse(url)
        if parsed.scheme not in ('http','https') or (parsed.hostname or '').lower() not in ('youtube.com','www.youtube.com','m.youtube.com','music.youtube.com','youtu.be'):
            QMessageBox.warning(self,'Invalid link','Enter a valid YouTube video link.'); return
        def job(log):
            import yt_dlp
            import imageio_ffmpeg
            folder=Path(self.output)/'YouTube Imports'; folder.mkdir(parents=True,exist_ok=True)
            class Logger:
                def debug(self,msg):
                    if msg and not msg.startswith('[debug]'):log(msg)
                def warning(self,msg):log('Warning: '+msg)
                def error(self,msg):log('Error: '+msg)
            opts={'format':'bestaudio[ext=m4a]/bestaudio','noplaylist':True,'outtmpl':str(folder/'%(title).120B [%(id)s].%(ext)s'),'restrictfilenames':True,'logger':Logger(),'quiet':True}
            with yt_dlp.YoutubeDL(opts) as ydl:
                info=ydl.extract_info(url,download=True)
                if not info:raise RuntimeError('No audio was downloaded.')
                path=ydl.prepare_filename(info)
            if not Path(path).is_file():raise RuntimeError('Download completed but the audio file could not be located.')
            log('Preparing audio for separation...')
            tmp=tempfile.NamedTemporaryFile(prefix='youtube-',suffix='.wav',dir=str(folder),delete=False); tmp.close()
            try:
                result=subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),'-nostdin','-v','error','-y','-i',path,'-vn','-c:a','pcm_s24le',tmp.name],capture_output=True,text=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
                if result.returncode:raise RuntimeError('Audio conversion failed: '+result.stderr[-1500:])
                return tmp.name
            except Exception:
                Path(tmp.name).unlink(missing_ok=True); raise
        import importlib.util
        if any(importlib.util.find_spec(name) is None for name in ('yt_dlp','imageio_ffmpeg')):
            QMessageBox.warning(self,'Setup needed','YouTube support is not installed. Close SPLIT and run SETUP-YOUTUBE.cmd included with this build.'); return
        self.log.appendPlainText('Importing YouTube audio...')
        self.run_worker(job,lambda path:setattr(self,'pending_import',path))
    def pick_output(self):
        p=QFileDialog.getExistingDirectory(self,'Choose output folder',self.output)
        if p: self.output=p; self.output_label.setText(p)
    def separate(self):
        if not Path(self.source).is_file(): QMessageBox.information(self,'Choose audio','Choose an existing audio file first.'); return
        model=self.model.currentData(); device=self.device.currentText(); src=self.source; output=self.output; folder=Path(output)/model/Path(src).stem
        if folder.exists() and any(folder.glob('*.wav')) and QMessageBox.question(self,'Replace stems?','Existing stems will be overwritten. Continue?') != QMessageBox.StandardButton.Yes: return
        def job(log):
            Path(output).mkdir(parents=True,exist_ok=True); cmd=[sys.executable,'-m','demucs','-n',model,'-d',device,'--float32','-o',output,src]
            p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace',bufsize=1,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            for line in p.stdout: log(line.rstrip())
            code=p.wait()
            if code: raise RuntimeError('Separation failed (exit code '+str(code)+'). See activity log.')
            return str(folder)
        self.run_worker(job,lambda p:self.open_stems(Path(p)))
    def load_stems(self):
        if self.work:return
        p=QFileDialog.getExistingDirectory(self,'Choose folder containing aligned WAV stems')
        if p:self.open_stems(Path(p))
    def open_stems(self,folder,extra_files=None,exclude_names=None):
        self.stop_audio()
        order={'vocals':0,'drums':1,'bass':2,'guitar':3,'piano':4,'other':5}; files=sorted(folder.glob('*.wav'),key=lambda p:(order.get(p.stem.lower(),99),p.stem.lower())) + list(extra_files or [])
        if not files: QMessageBox.warning(self,'No stems','No WAV files found.'); return
        try:
            with sf.SoundFile(str(files[0])) as info: duration_ms=round(len(info)*1000/info.samplerate)
        except Exception as exc: QMessageBox.warning(self,'Cannot read stem',str(exc)); return
        self.seek.setRange(0,max(0,duration_ms)); self.seek.setValue(0); self.seek.setEnabled(duration_ms>0); self.update_time_label(0)
        self.result=folder
        while self.mixer.count():
            item=self.mixer.takeAt(0)
            if item.widget(): item.widget().deleteLater()
        self.strips=[]
        for i,p in enumerate(files):
            strip=Strip(p,COLORS[i%len(COLORS)],self.preview_one,self.export_stem); strip.setMinimumWidth(120); self.strips.append(strip); self.mixer.addWidget(strip,1)
            if p.stem.lower() in (exclude_names or set()):strip.include.setChecked(False)
        self.wave.lanes=[]; self.wave.update()
        def calc(log):
            lanes=[]
            for file in files:
                with sf.SoundFile(str(file)) as f:
                    n=max(1,(len(f)+599)//600); peaks=[]
                    while True:
                        b=f.read(n,dtype='float32',always_2d=True)
                        if not len(b):break
                        peaks.append(float(np.max(np.abs(b))))
                    lanes.append((file.stem,peaks))
            return lanes
        self.log.appendPlainText('Loading waveforms...')
        if self.work:self.pending_wave=calc
        else:self.run_worker(calc,lambda data:self.set_waves(data))
    def set_waves(self,lanes): self.wave.lanes=lanes; self.wave.update(); self.log.appendPlainText('Stems ready.')
    def advanced_split(self,kind):
        if self.work:return
        parent=next((s.path for s in self.strips if s.path.stem.lower()==kind),None)
        if parent is None:
            QMessageBox.information(self,'Stem needed','Load or separate a song containing a '+kind+'.wav stem first.');return
        engine=Path(__file__).resolve().parent/'.advanced-env'/'Scripts'/'python.exe'
        if not engine.is_file():
            QMessageBox.information(self,'Setup needed','Close SPLIT and run SETUP-ADVANCED.cmd in this folder first.');return
        self.stop_audio(); existing=[s.path for s in self.strips if s.path.parent != self.result]; excluded={s.path.stem.lower() for s in self.strips if not s.include.isChecked() and s.path.parent == self.result}; excluded.add(kind)
        def job(log):
            import uuid
            stage=self.result/'advanced'/('guitar-' if kind=='guitar' else 'drums-')
            stage=Path(str(stage)+uuid.uuid4().hex[:8]); stage.mkdir(parents=True,exist_ok=False)
            if kind=='guitar':
                args=[str(engine),str(Path(__file__).resolve().parent/'guitar_gpu.py'),'split-guitar','--file',str(parent),'--output',str(stage),'--cache',str(Path(__file__).resolve().parent/'model-cache')]
            else:
                args=[str(engine),'-m','mdxnet_infer.cli',str(parent),'-o',str(stage),'--model','drumsep-6stem','--device','cuda']
            log('Starting '+kind+' separation on CUDA. First use downloads model weights; this may take time.')
            proc=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace',bufsize=1,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            for line in proc.stdout:log(line.rstrip())
            if proc.wait()!=0:raise RuntimeError('Advanced separation failed. See the activity log. Original stems remain unchanged.')
            files=sorted(stage.rglob('*.wav'))
            expected=2 if kind=='guitar' else 6
            if len(files)!=expected:raise RuntimeError(f'Expected {expected} output WAV files, found {len(files)}. Original stems remain unchanged.')
            with sf.SoundFile(str(parent)) as original: ref=(original.samplerate,original.channels,len(original))
            for file in files:
                with sf.SoundFile(str(file)) as f:
                    if (f.samplerate,f.channels,len(f))!=ref:raise RuntimeError('Advanced output does not match the original stem length or format; it was not added to the mix.')
            return files
        def complete(files):
            keep=[p for p in existing if not p.parent.name.startswith(kind+'-')]
            self.open_stems(self.result,keep+files,excluded)
            self.log.appendPlainText('Advanced stems added. The combined '+kind+' stem is excluded to avoid doubling.')
        self.run_worker(job,complete)
    def chosen(self):
        selected=[s for s in self.strips if s.include.isChecked()]
        if not selected: QMessageBox.information(self,'Select stems','Include at least one stem.'); return [],[]
        return [s.path for s in selected],[s.slider.value()/100 for s in selected]
    def preview_one(self,path): self.preview_paths([path],[1.0])
    def export_stem(self,path):
        if self.work:return
        ext,fmt,subtype=self.export_format.currentData()
        target,_=QFileDialog.getSaveFileName(self,'Export individual stem',path.stem+'.'+ext,fmt+' (*.'+ext+')')
        if not target:return
        if not target.lower().endswith('.'+ext):target += '.'+ext
        if Path(target).resolve() in {s.path.resolve() for s in self.strips}:
            QMessageBox.warning(self,'Choose another name','A loaded stem cannot be overwritten.'); return
        def write_stem(log):
            if fmt == 'WAV': shutil.copyfile(path,target); return 1.0
            return render_mix([path],target,[1.0],subtype=subtype,format=fmt)
        self.run_worker(write_stem,lambda gain:QMessageBox.information(self,'Stem exported',f'Saved to:\n{target}\n\nPeak protection gain: {gain:.3f}'))
    def preview_mix(self):
        if self.work:return
        files,levels=self.chosen()
        if not files:return
        self.stop_audio()
        try:
            self.audio_files=[sf.SoundFile(str(s.path)) for s in self.strips]
            first=self.audio_files[0]; rate=first.samplerate; channels=first.channels; frames=len(first)
            if any((f.samplerate,f.channels,len(f))!=(rate,channels,frames) for f in self.audio_files):raise ValueError('Stems must have matching sample rates, channels and lengths.')
            self.play_start_ms=self.seek.value()
            if self.play_start_ms >= self.seek.maximum():self.play_start_ms=0; self.seek.setValue(0)
            frame=min(frames,round(self.play_start_ms*rate/1000))
            for f in self.audio_files:f.seek(frame)
            fmt=QAudioFormat(); fmt.setSampleRate(rate); fmt.setChannelCount(channels); fmt.setSampleFormat(QAudioFormat.SampleFormat.Float)
            device=QMediaDevices.defaultAudioOutput()
            if not device.isFormatSupported(fmt):raise ValueError('The default output device does not support this sample rate or channel format.')
            self.audio_sink=QAudioSink(device,fmt); self.audio_sink.setBufferSize(4096*channels*4); self.audio_stream=self.audio_sink.start()
            if self.audio_stream is None:raise RuntimeError('Could not open the default audio output.')
            self.audio_timer.start(10); self.log.appendPlainText('Playing live mix. Faders and Include controls update during playback.')
        except Exception as e:
            self.stop_audio(); QMessageBox.critical(self,'Playback failed',str(e))
    def audio_pump(self):
        if not self.audio_sink or not self.audio_stream:return
        try:
            if not self.seek.isSliderDown():self.seek.setValue(min(self.seek.maximum(),self.play_start_ms+self.audio_sink.processedUSecs()//1000))
            if self.audio_pending:
                wrote=self.audio_stream.write(self.audio_pending)
                if wrote<0:raise RuntimeError('Audio output write failed.')
                self.audio_pending=self.audio_pending[wrote:]
                if self.audio_pending:return
            space=self.audio_sink.bytesFree(); channels=self.audio_files[0].channels
            count=min(1024,space//(channels*4))
            if count<1:return
            blocks=[f.read(count,dtype='float32',always_2d=True) for f in self.audio_files]
            if not len(blocks[0]):
                self.audio_timer.stop(); generation=self.play_generation; QTimer.singleShot(250,lambda:self.finish_audio(generation)); return
            mix=np.zeros_like(blocks[0])
            for block,strip in zip(blocks,self.strips):
                adjusted=block*(strip.slider.value()/100.0) if strip.include.isChecked() else np.zeros_like(block)
                strip.meter.set_levels(np.max(np.abs(adjusted),axis=0).tolist()); mix += adjusted
            # Hard clipping keeps real-time output within the supported float range.
            self.audio_pending=np.clip(mix,-1,1).astype('<f4').tobytes()
        except Exception as e:
            self.stop_audio(); self.log.appendPlainText('Playback error: '+str(e))
    def stop_audio(self):
        self.play_generation += 1
        self.audio_timer.stop()
        if self.audio_sink:self.audio_sink.stop()
        self.audio_sink=None; self.audio_stream=None; self.audio_pending=b''
        for f in self.audio_files:f.close()
        self.audio_files=[]
        for strip in self.strips:strip.meter.set_levels([])
        winsound.PlaySound(None,winsound.SND_PURGE)
    def finish_audio(self,generation):
        if not self.audio_sink or generation!=self.play_generation:return
        self.stop_audio(); self.seek.setValue(self.seek.maximum())
    def seek_released(self):
        if self.audio_sink:self.preview_mix()
    def update_time_label(self,value):
        if not hasattr(self,'time_label'):return
        def clock(ms):
            sec=ms//1000; return f'{sec//60}:{sec%60:02d}'
        self.time_label.setText(clock(value)+' / '+clock(self.seek.maximum()))
    def preview_paths(self,files,levels):
        if self.work:return
        self.stop_audio(); self.log.appendPlainText('Rendering preview...')
        self.run_worker(lambda log:render_mix(files,self.preview_file,levels,subtype='PCM_16'),lambda _:self.play())
    def play(self):
        try: winsound.PlaySound(self.preview_file,winsound.SND_FILENAME|winsound.SND_ASYNC|winsound.SND_NODEFAULT); self.log.appendPlainText('Playing preview.')
        except RuntimeError as e: QMessageBox.critical(self,'Playback error',str(e))
    def export(self):
        if self.work:return
        files,levels=self.chosen()
        if not files:return
        ext,fmt,subtype=self.export_format.currentData()
        p,_=QFileDialog.getSaveFileName(self,'Export custom mix','Custom Mix.'+ext,fmt+' (*.'+ext+')')
        if not p:return
        if not p.lower().endswith('.'+ext):p += '.'+ext
        if Path(p).resolve() in {s.path.resolve() for s in self.strips}: QMessageBox.warning(self,'Choose another name','Do not overwrite a stem.'); return
        self.run_worker(lambda log:render_mix(files,p,levels,subtype=subtype,format=fmt),lambda gain:QMessageBox.information(self,'Mix exported',f'Saved to:\n{p}\n\nPeak protection gain: {gain:.3f}'))
    def open_folder(self):
        folder=self.result or Path(self.output)
        if folder.is_dir():os.startfile(str(folder))
        else:QMessageBox.information(self,'Output folder','The folder will be created when you separate a song.')

if __name__=='__main__':
    app=QApplication(sys.argv); window=Window(); window.show(); sys.exit(app.exec())

