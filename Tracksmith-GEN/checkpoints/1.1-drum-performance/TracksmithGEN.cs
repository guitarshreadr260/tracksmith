using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Media;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace TracksmithGEN {
public class Note { public double Beat, Length; public int Pitch, Velocity; }
public class Take {
 public string Name, Instrument; public List<Note> Notes; public float[] Audio; public double Start; public int Tempo;
 public override string ToString() { return Name + "  /  " + Instrument; }
}
public static class Engine {
 public const int Rate = 44100;
 public static int[] Chord(string text) {
  Match m=Regex.Match(text.Trim(), @"^([A-Ga-g])([#b]?)(m|maj7|7)?$");
  if(!m.Success) throw new ArgumentException("Use chords such as C, Am, F#, Bb, Cmaj7 or G7, separated by spaces. One chord per bar.");
  int root=new Dictionary<char,int>{{'C',0},{'D',2},{'E',4},{'F',5},{'G',7},{'A',9},{'B',11}}[char.ToUpper(m.Groups[1].Value[0])];
  root += m.Groups[2].Value=="#"?1:m.Groups[2].Value=="b"?-1:0; root=(root+12)%12;
  string q=m.Groups[3].Value;
  return q=="7"||q=="maj7"?new[]{root,root+4,root+7,root+(q=="7"?10:11)}:new[]{root,root+(q=="m"?3:4),root+7};
 }
 public static List<Note> Compose(string chords,string instrument,int bars,int density,int seed) {
  int[][] harmony=chords.Split(new[]{' ',',','\n','\r'},StringSplitOptions.RemoveEmptyEntries).Select(Chord).ToArray();
  if(harmony.Length==0) throw new ArgumentException("Enter at least one chord.");
  var notes=new List<Note>(); var random=new Random(seed);
  for(int b=0;b<bars;b++) {
   int[] c=harmony[b%harmony.Length]; bool bass=instrument=="Bass"; bool lead=instrument=="Lead guitar guide";
   int steps=density==0?4:density==1?8:16;
   if(instrument=="Piano chords" || instrument=="Synth pad") {
    int hits=density==0||instrument=="Synth pad"?1:2;
    for(int h=0;h<hits;h++) foreach(int n in c) notes.Add(new Note{Beat=b*4+h*4.0/hits,Length=3.7/hits,Pitch=48+n,Velocity=70+random.Next(18)});
   } else for(int s=0;s<steps;s++) {
    if(s>0 && random.NextDouble()<(lead?.25:.12)) continue;
    int index=bass && s%2==0?0:random.Next(c.Length);
    int pitch=(bass?36:60)+c[index]+(lead&&random.Next(4)==0?12:0);
    notes.Add(new Note{Beat=b*4+s*4.0/steps,Length=4.0/steps*(lead?.8:.9),Pitch=pitch,Velocity=70+random.Next(35)});
   }
  }
  return notes;
 }
 public static float[] Render(List<Note> notes,string instrument,int bpm,double seconds) {
  var audio=new float[(int)Math.Ceiling(seconds*Rate)]; bool bass=instrument=="Bass", pad=instrument=="Synth pad";
  foreach(Note n in notes) {
   double start=n.Beat*60/bpm, duration=n.Length*60/bpm;
   int at=(int)(start*Rate), length=(int)(duration*Rate);
   double f=440*Math.Pow(2,(n.Pitch-69)/12.0);
   for(int i=0;i<length && at+i<audio.Length;i++) {
    double t=(double)i/Rate, phase=2*Math.PI*f*t;
    double env=Math.Min(1,t/(pad?.12:.006))*Math.Min(1,(duration-t)/(pad?.2:.035));
    if(!pad) env*=Math.Exp(-t*(bass?2:3));
    double signal=Math.Sin(phase)+.27*Math.Sin(phase*2)+.1*Math.Sin(phase*3);
    audio[at+i]+=(float)(signal*env*n.Velocity/127*.15);
   }
  }
  return audio;
 }
 public static void Wav(string path,float[] audio) {
  using(var w=new BinaryWriter(File.Create(path))) {
   w.Write(Encoding.ASCII.GetBytes("RIFF"));w.Write(36+audio.Length*2);w.Write(Encoding.ASCII.GetBytes("WAVEfmt "));
   w.Write(16);w.Write((short)1);w.Write((short)1);w.Write(Rate);w.Write(Rate*2);w.Write((short)2);w.Write((short)16);
   w.Write(Encoding.ASCII.GetBytes("data"));w.Write(audio.Length*2);
   foreach(float s in audio) w.Write((short)(Math.Max(-1,Math.Min(1,s))*32767));
  }
 }
 public static float[] ReadWav(string path) {
  using(var r=new BinaryReader(File.OpenRead(path))) {
   if(Encoding.ASCII.GetString(r.ReadBytes(4))!="RIFF") throw new ArgumentException("Import a PCM or float WAV file.");
   r.ReadInt32();if(Encoding.ASCII.GetString(r.ReadBytes(4))!="WAVE") throw new ArgumentException("Invalid WAV.");
   int format=0,channels=0,rate=0,bits=0; byte[] data=null;
   while(r.BaseStream.Position+8<=r.BaseStream.Length) {
    string id=Encoding.ASCII.GetString(r.ReadBytes(4)); uint size=r.ReadUInt32(); long end=r.BaseStream.Position+size;
    if(end>r.BaseStream.Length) throw new ArgumentException("Truncated WAV.");
    if(id=="fmt ") {format=r.ReadUInt16();channels=r.ReadUInt16();rate=r.ReadInt32();r.ReadInt32();r.ReadUInt16();bits=r.ReadUInt16();}
    else if(id=="data") {if(size>150000000) throw new ArgumentException("Please import a shorter WAV (under 150 MB).");data=r.ReadBytes((int)size);}
    r.BaseStream.Position=end+(size%2);
   }
   if(data==null || channels<1 || channels>8 || rate<8000 || rate>192000 || !((format==1&&(bits==16||bits==24||bits==32))||(format==3&&bits==32)))
    throw new ArgumentException("Supported: 16/24/32-bit PCM or 32-bit float WAV. Convert compressed or extensible WAV to PCM first.");
   int width=bits/8, frames=data.Length/(width*channels);
   if(frames==0 || (double)frames/rate>300) throw new ArgumentException("Import between one sample and five minutes of audio.");
   var mono=new float[frames];
   for(int i=0;i<frames;i++) for(int ch=0;ch<channels;ch++) {
    int p=(i*channels+ch)*width; double s;
    if(format==3) s=BitConverter.ToSingle(data,p);
    else if(bits==16) s=BitConverter.ToInt16(data,p)/32768.0;
    else if(bits==24) {int v=data[p]|data[p+1]<<8|data[p+2]<<16;if((v&0x800000)!=0)v|=unchecked((int)0xff000000);s=v/8388608.0;}
    else s=BitConverter.ToInt32(data,p)/2147483648.0;
    if(double.IsNaN(s)||double.IsInfinity(s)) s=0;
    mono[i]+=(float)(Math.Max(-1,Math.Min(1,s))/channels);
   }
   var resampled=new float[(int)((long)frames*Rate/rate)];
   for(int i=0;i<resampled.Length;i++) {double p=(double)i*rate/Rate;int a=(int)p;resampled[i]=(float)(mono[a]*(1-(p-a))+mono[Math.Min(a+1,frames-1)]*(p-a));}
   return resampled;
  }
 }
 static void BE(BinaryWriter w,int v,int count) {for(int i=count-1;i>=0;i--)w.Write((byte)(v>>(i*8)));}
 static void VL(BinaryWriter w,int v) {var b=new List<byte>{(byte)(v&127)};while((v>>=7)>0)b.Insert(0,(byte)((v&127)|128));w.Write(b.ToArray());}
 public static void Midi(string path,Take take,bool aligned) {
  var events=new List<Tuple<int,byte[]>>();int offset=aligned?(int)Math.Round(take.Start*take.Tempo/60*480):0;
  foreach(Note n in take.Notes) {int at=offset+(int)Math.Round(n.Beat*480);events.Add(Tuple.Create(at,new[]{(byte)0x90,(byte)n.Pitch,(byte)n.Velocity}));events.Add(Tuple.Create(at+(int)(n.Length*480),new[]{(byte)0x80,(byte)n.Pitch,(byte)0}));}
  using(var stream=new MemoryStream()) {using(var t=new BinaryWriter(stream,Encoding.ASCII,true)) {
   t.Write(new byte[]{0,255,81,3});BE(t,60000000/take.Tempo,3);
   int program=take.Instrument=="Bass"?33:take.Instrument=="Synth pad"?89:take.Instrument=="Lead guitar guide"?29:0;
   t.Write(new[]{(byte)0,(byte)0xc0,(byte)program});int previous=0;
   foreach(var e in events.OrderBy(e=>e.Item1).ThenBy(e=>e.Item2[0])) {VL(t,e.Item1-previous);t.Write(e.Item2);previous=e.Item1;}
   t.Write(new byte[]{0,255,47,0});
  }
  using(var w=new BinaryWriter(File.Create(path))) {w.Write(Encoding.ASCII.GetBytes("MThd"));BE(w,6,4);BE(w,0,2);BE(w,1,2);BE(w,480,2);w.Write(Encoding.ASCII.GetBytes("MTrk"));BE(w,(int)stream.Length,4);w.Write(stream.ToArray());}
  }
 }
}
public class MainForm:Form {
 readonly Color panel=Color.FromArgb(28,33,43),ink=Color.FromArgb(226,232,240),accent=Color.FromArgb(88,220,187);
 ComboBox instrument=new ComboBox(),density=new ComboBox(); TextBox chords=new TextBox();
 NumericUpDown bpm=new NumericUpDown(),bars=new NumericUpDown(),start=new NumericUpDown();
 ListBox takes=new ListBox(); Label sourceLabel=new Label(),status=new Label(); Panel timeline=new Panel();
 Button generate; CheckBox reference=new CheckBox(),aligned=new CheckBox(); TrackBar sourceLevel=new TrackBar();
 float[] source; string sourcePath; SoundPlayer player; string previewPath; int serial=0; bool busy;
 public MainForm() {
  Text="Tracksmith GEN | Standalone prototype 0.2";ClientSize=new Size(1080,750);MinimumSize=new Size(1096,789);BackColor=Color.FromArgb(16,20,28);ForeColor=ink;Font=new Font("Segoe UI",10);
  var title=new Label{Text="TRACKSMITH  /  GEN",Location=new Point(30,20),Size=new Size(700,40),Font=new Font("Segoe UI",23,FontStyle.Bold),ForeColor=accent};Controls.Add(title);
  LabelAt("Build the part your song needs.",30,67,700);
  LabelAt("ADD A PART     /     Local chord-based sketch engine",30,104,900);
  ButtonAt("Try demo / no import",825,99,225,async()=>{if(busy)return;Stop();source=null;sourcePath=null;sourceLabel.Text="Demo: Em - C - G - D / no recording needed";chords.Text="Em C G D";bpm.Value=100;bars.Value=8;start.Value=0;instrument.SelectedIndex=0;timeline.Invalidate();await Generate();});
  ButtonAt("AI audio lab",825,143,225,()=>{if(busy)return;Stop();using(var lab=new AudioLab(instrument.Text,(int)bpm.Value)){if(lab.ShowDialog(this)==DialogResult.OK){var result=Engine.ReadWav(lab.OutputPath);var take=new Take{Name="AI take "+(++serial),Instrument=lab.Instrument,Notes=null,Audio=result,Start=0,Tempo=(int)bpm.Value};takes.Items.Add(take);takes.SelectedItem=take;while(takes.Items.Count>30)takes.Items.RemoveAt(0);reference.Checked=false;status.Text="AI audio loaded. Audition solo; tempo, harmony and isolation are not guaranteed. MIDI is unavailable for AI audio.";try{File.Delete(lab.OutputPath);}catch{}}}});
  var import=ButtonAt("Import chorus WAV",30,143,180,()=>Import());sourceLabel=LabelAt("No reference loaded. You can still sketch from chords.",230,150,575);
  timeline.Location=new Point(30,195);timeline.Size=new Size(1020,100);timeline.BackColor=panel;timeline.Paint+=DrawTimeline;Controls.Add(timeline);
  LabelAt("Start (seconds)",30,310,140);Setup(start,30,336,0,300,0);start.DecimalPlaces=2;
  LabelAt("Tempo (BPM)",205,310,140);Setup(bpm,205,336,40,240,100);
  LabelAt("Length (4/4 bars)",380,310,160);Setup(bars,380,336,1,32,8);
  LabelAt("Instrument",555,310,160);instrument.Items.AddRange(new object[]{"Piano arpeggio","Piano chords","Bass","Synth pad","Lead guitar guide"});instrument.SelectedIndex=0;Choice(instrument,555,336,240);
  LabelAt("Activity",825,310,170);density.Items.AddRange(new object[]{"Sparse","Moderate","Busy"});density.SelectedIndex=1;Choice(density,825,336,225);
  LabelAt("Chord progression / one chord per bar, repeated to fill the section",30,382,950);
  chords.Text="Em C G D";chords.Location=new Point(30,408);chords.Size=new Size(1020,30);chords.BackColor=panel;chords.ForeColor=ink;Controls.Add(chords);
  generate=ButtonAt("Generate 3 sketches",30,458,210,async()=>await Generate());
  reference.Text="Include reference in playback";reference.Checked=true;reference.Location=new Point(265,462);reference.Size=new Size(290,30);Controls.Add(reference);
  sourceLevel.Minimum=0;sourceLevel.Maximum=100;sourceLevel.Value=65;sourceLevel.TickStyle=TickStyle.None;sourceLevel.Location=new Point(570,460);sourceLevel.Size=new Size(160,35);Controls.Add(sourceLevel);LabelAt("Reference level",745,465,200);
  takes.Location=new Point(30,512);takes.Size=new Size(650,135);takes.BackColor=panel;takes.ForeColor=ink;takes.BorderStyle=BorderStyle.None;takes.IntegralHeight=false;Controls.Add(takes);
  ButtonAt("Play selected",710,512,160,()=>Play());ButtonAt("Stop",890,512,160,()=>Stop());
  ButtonAt("Export WAV",710,559,160,()=>Export(false));ButtonAt("Export MIDI",890,559,160,()=>Export(true));
  aligned.Text="Export aligned to song start (leading silence)";aligned.Checked=true;aligned.Location=new Point(710,610);aligned.Size=new Size(340,40);Controls.Add(aligned);
  status=LabelAt("Ready. Try the demo, sketch from chords, or open the experimental AI audio lab (separate setup required).",30,670,1020);status.Height=55;status.ForeColor=accent;
  start.ValueChanged+=(s,e)=>timeline.Invalidate();bars.ValueChanged+=(s,e)=>timeline.Invalidate();bpm.ValueChanged+=(s,e)=>timeline.Invalidate();
  Shown+=(s,e)=>{instrument.SelectedIndex=0;density.SelectedIndex=1;};
  FormClosed+=(s,e)=>{Stop();if(previewPath!=null)try{File.Delete(previewPath);}catch{}};
 }
 Label LabelAt(string text,int x,int y,int width) {var c=new Label{Text=text,Location=new Point(x,y),Size=new Size(width,27)};Controls.Add(c);return c;}
 Button ButtonAt(string text,int x,int y,int width,Action action) {var b=new Button{Text=text,Location=new Point(x,y),Size=new Size(width,36),FlatStyle=FlatStyle.Flat,BackColor=panel,ForeColor=ink};b.Click+=(s,e)=>{try{action();}catch(Exception ex){Fail(ex);}};Controls.Add(b);return b;}
 void Setup(NumericUpDown c,int x,int y,int min,int max,int value) {c.Minimum=min;c.Maximum=max;c.Value=value;c.Location=new Point(x,y);c.Size=new Size(145,30);c.BackColor=panel;c.ForeColor=ink;Controls.Add(c);}
 void Choice(ComboBox c,int x,int y,int width) {c.DropDownStyle=ComboBoxStyle.DropDownList;c.FlatStyle=FlatStyle.Standard;c.Location=new Point(x,y);c.Size=new Size(width,30);c.BackColor=Color.White;c.ForeColor=Color.Black;Controls.Add(c);c.SelectedIndex=c==density?1:0;}
 void Fail(Exception ex) {status.Text=ex.Message;MessageBox.Show(this,ex.Message,"Tracksmith GEN",MessageBoxButtons.OK,MessageBoxIcon.Information);}
 void Import() {if(busy)return;using(var d=new OpenFileDialog{Filter="WAV audio|*.wav"})if(d.ShowDialog()==DialogResult.OK){var loaded=Engine.ReadWav(d.FileName);Stop();source=loaded;sourcePath=d.FileName;start.Value=0;sourceLabel.Text=Path.GetFileName(sourcePath)+"  /  "+(source.Length/(double)Engine.Rate).ToString("0.0")+" s";timeline.Invalidate();status.Text="Reference loaded for playback. Enter its tempo and chords manually.";}}
 async Task Generate() {
  if(busy)return;
  try {
   string ins=instrument.Text, progression=chords.Text;int tempo=(int)bpm.Value,count=(int)bars.Value,activity=density.SelectedIndex;double offset=(double)start.Value;
   double seconds=count*4*60.0/tempo;
   if(source!=null && offset+seconds>source.Length/(double)Engine.Rate+.01)throw new ArgumentException("The section extends past the reference. Reduce bars, adjust tempo, or move the start earlier.");
   // Validate before starting work.
   Engine.Compose(progression,ins,1,activity,1);busy=true;generate.Enabled=false;status.Text="Rendering three chord-based sketches...";
   int first=serial;serial+=3;
   var batch=await Task.Run(()=>{var result=new List<Take>();for(int i=0;i<3;i++){var notes=Engine.Compose(progression,ins,count,activity,first+i+1701);result.Add(new Take{Name="Take "+(first+i+1),Instrument=ins,Tempo=tempo,Start=offset,Notes=notes,Audio=Engine.Render(notes,ins,tempo,seconds)});}return result;});
   foreach(var take in batch)takes.Items.Add(take);takes.SelectedIndex=takes.Items.Count-3;
   while(takes.Items.Count>30)takes.Items.RemoveAt(0);
   status.Text="Three sketches ready. Compare with your chorus; use MIDI with your own instrument sounds.";
  }catch(Exception ex){Fail(ex);}finally{busy=false;generate.Enabled=true;}
 }
 Take Selected() {var t=takes.SelectedItem as Take;if(t==null)throw new ArgumentException("Generate and select a take first.");return t;}
 void Stop(){if(player!=null){player.Stop();player.Dispose();player=null;}}
 void Play() {
  Take t=Selected();Stop();var audio=(float[])t.Audio.Clone();
  if(reference.Checked&&source!=null){int at=(int)(t.Start*Engine.Rate);double level=sourceLevel.Value/100.0;for(int i=0;i<audio.Length&&at+i<source.Length;i++)audio[i]=(float)(audio[i]+source[at+i]*level);}
  if(previewPath==null)previewPath=Path.Combine(Path.GetTempPath(),"TracksmithGEN-"+Guid.NewGuid()+".wav");
  Engine.Wav(previewPath,audio);player=new SoundPlayer(previewPath);player.Play();status.Text="Playing "+t.Name+(reference.Checked&&source!=null?" with reference.":" solo.");
 }
 void Export(bool midi) {
  Take t=Selected();if(midi&&t.Notes==null)throw new ArgumentException("This take contains generated audio, not MIDI notes. Export WAV instead.");using(var d=new SaveFileDialog{Filter=midi?"MIDI|*.mid":"WAV audio|*.wav",FileName="TracksmithGEN-"+t.Name.Replace(" ","-")+(midi?".mid":".wav")})if(d.ShowDialog()==DialogResult.OK){
   if(midi)Engine.Midi(d.FileName,t,aligned.Checked);
   else {int offset=aligned.Checked?(int)(t.Start*Engine.Rate):0;var audio=new float[offset+t.Audio.Length];Array.Copy(t.Audio,0,audio,offset,t.Audio.Length);Engine.Wav(d.FileName,audio);}
   status.Text="Exported isolated "+t.Instrument+" part. "+(aligned.Checked?"Position preserved with leading silence.":"Clip starts at zero.");
  }
 }
 void DrawTimeline(object sender,PaintEventArgs e) {
  int w=timeline.Width,h=timeline.Height;double duration=source==null?Math.Max(1,(double)start.Value+(double)bars.Value*240/(double)bpm.Value):source.Length/(double)Engine.Rate;
  using(var pen=new Pen(accent)) {
   if(source!=null)for(int x=0;x<w;x++){int a=(int)((long)x*source.Length/w),b=(int)((long)(x+1)*source.Length/w);float peak=0;for(int i=a;i<b;i++)peak=Math.Max(peak,Math.Abs(source[i]));e.Graphics.DrawLine(pen,x,h/2-peak*40,x,h/2+peak*40);}
   else e.Graphics.DrawString("Import a chorus to see its waveform",Font,Brushes.Gray,20,40);
  }
  float left=(float)((double)start.Value/duration*w),width=(float)((double)bars.Value*240/(double)bpm.Value/duration*w);
  using(var fill=new SolidBrush(Color.FromArgb(45,88,220,187))) e.Graphics.FillRectangle(fill,left,0,width,h);
  e.Graphics.DrawString("Selected section / "+start.Value+" s",Font,Brushes.White,12,7);
 }
}
static class Program {
 [STAThread] static int Main(string[] args) {
  if(args.Length==2&&args[0]=="--stable-ui-test"){
   Application.EnableVisualStyles();int code=1;using(var form=new StudioForm()){form.Shown+=async(s,e)=>{try{await form.TestStableWorkflow(args[1]);File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"stable-ui-test-result.txt"),"PASS: Multi Platinum -> selected Stable Audio 3 engine with ACE unavailable -> real CUDA text generation -> stereo WAV -> full-prompt saved take -> reopen and playback duration. Sound quality remains unrated.");code=0;}catch(Exception ex){File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"stable-ui-test-result.txt"),"FAIL: "+ex);}finally{form.Close();}};Application.Run(form);}return code;
  }

  if(args.Length==3&&args[0]=="--reference-ui-test"){
   Application.EnableVisualStyles();int code=1;string testDir=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"reference-ui-test-"+DateTime.Now.ToString("yyyyMMdd-HHmmss"));using(var form=new ReferenceForm(new TakeStore(testDir),args[1])){form.Shown+=async(s,e)=>{try{await form.TestWorkflow(args[2]);File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"reference-ui-test-result.txt"),"PASS: reference dialog -> real CUDA audio-guided generation -> saved original/variation -> playback length and seek. Audible quality remains unrated.");code=0;}catch(Exception ex){File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"reference-ui-test-result.txt"),"FAIL: "+ex);}finally{form.Close();}};Application.Run(form);}return code;
  }
  if(args.Length==2&&args[0]=="--render-reference-ui"){
   Application.EnableVisualStyles();using(var form=new ReferenceForm(new TakeStore(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"ui-reference-library")),args[1])){form.Show();Application.DoEvents();using(var bitmap=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(bitmap,new Rectangle(0,0,form.Width,form.Height));bitmap.Save(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"reference-ui-check.png"));}form.Close();}return 0;
  }
  if(args.Length==3&&args[0]=="--reference-smoke-test")return ReferenceBridge.Smoke(args[1],args[2]);
  if(args.Length>0&&args[0]=="--gpu-smoke-test")return AceBridge.Smoke(args.Length>1?args[1]:"balanced");
  if(args.Length==3&&args[0]=="--echo-arg") {File.WriteAllText(args[2],args[1]);return 0;}
  if(args.Length>0&&args[0]=="--self-test")return SelfTest();
  if(args.Length>0&&args[0]=="--render-ui") {
   Application.EnableVisualStyles();using(var form=new StudioForm()) {form.Show();Application.DoEvents();using(var bitmap=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(bitmap,new Rectangle(0,0,form.Width,form.Height));bitmap.Save(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"ui-check.png"));}form.Close();}return 0;
  }
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new StudioForm());return 0;
 }
 static int SelfTest() {
  try {
   string dir=Path.Combine(Path.GetTempPath(),"TracksmithGEN-test-"+Guid.NewGuid());Directory.CreateDirectory(dir);
   foreach(string ins in new[]{"Piano arpeggio","Piano chords","Bass","Synth pad","Lead guitar guide"}) {
    var notes=Engine.Compose("Em C G D",ins,4,1,42);if(notes.Count==0||notes.Any(n=>n.Pitch<0||n.Pitch>127))throw new Exception("Invalid notes");
    var audio=Engine.Render(notes,ins,120,8);string path=Path.Combine(dir,ins+".wav");Engine.Wav(path,audio);var decoded=Engine.ReadWav(path);
    if(decoded.Length!=352800||!decoded.Any(s=>Math.Abs(s)>.001))throw new Exception("Invalid audio");
    var take=new Take{Notes=notes,Instrument=ins,Tempo=120,Start=2};Engine.Midi(Path.Combine(dir,ins+".mid"),take,true);
   }
   bool rejected=false;try{Engine.Chord("nonsense");}catch(ArgumentException){rejected=true;}if(!rejected)throw new Exception("Chord validation failed");
   foreach(string text in new[]{"A \"gentle\" piano; $(nothing) & no vocals",@"C:\a space\end\"}) {
    string echo=Path.Combine(dir,"echo.txt");var info=new System.Diagnostics.ProcessStartInfo(Application.ExecutablePath,"--echo-arg "+AudioLab.Quote(text)+" "+AudioLab.Quote(echo)){UseShellExecute=false,CreateNoWindow=true};
    using(var p=System.Diagnostics.Process.Start(info)){if(!p.WaitForExit(10000)){p.Kill();throw new Exception("Argument roundtrip timeout");}if(p.ExitCode!=0||File.ReadAllText(echo)!=text)throw new Exception("AI prompt argument quoting failed");}
   }
   string expanded=PromptBuilder.Expand("hope","Piano",false,0);if(!expanded.ToLowerInvariant().Contains("hopeful")||!expanded.Contains("piano"))throw new Exception("Idea expansion failed");
   if(expanded==PromptBuilder.Expand("hope","Piano",false,1))throw new Exception("Prompt alternative failed");
   if(!PromptBuilder.Expand("a quiet night drive","Bass",true,0).Contains("song idea"))throw new Exception("Song prompt mode failed");
   var saved=new TakeStore(Path.Combine(dir,"saved"));string source=Path.Combine(dir,"Bass.wav");var item=saved.Add(source,"Rock & <Hope>",expanded,"Piano","Instrument part",100);item.Favorite=true;saved.Save();var reopened=new TakeStore(saved.Root);if(reopened.Items.Count!=1||reopened.Items[0].Title!="Rock & <Hope>"||!reopened.Items[0].Favorite)throw new Exception("Saved library roundtrip failed");
   if(!File.ReadAllBytes(source).SequenceEqual(File.ReadAllBytes(Path.Combine(saved.Root,item.File))))throw new Exception("Original audio preservation failed");
   using(var audioPlayer=new NativePlayer()){audioPlayer.Open(source);if(Math.Abs(audioPlayer.Length-8000)>30)throw new Exception("Playback length incorrect");audioPlayer.Volume(600);audioPlayer.Seek(1000);if(Math.Abs(audioPlayer.Position-1000)>40)throw new Exception("Playback seek failed");}
   using(var ui=new StudioForm()){ui.Show();Application.DoEvents();ui.TestInstrumentSelection();ui.TestRememberedSettings();ui.TestPromptOnly();ui.TestEngineSelection();ui.Close();}

   File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"test-results.txt"),"PASS: five sketch instruments; WAV roundtrip; MIDI; invalid chords; subprocess prompt escaping; word/sentence prompt expansion; prompt alternatives; saved library rename/favorite roundtrip; original WAV byte preservation; playback length/volume/seek API (fallback in this environment). 50,000-character prompt editor/request/preferences roundtrip; no key/tempo selector defaults; This regression run does not assess inference or audible quality. Output: "+dir);return 0;
  }catch(Exception ex){File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"test-results.txt"),"FAIL: "+ex);return 1;}
 }
}
}
