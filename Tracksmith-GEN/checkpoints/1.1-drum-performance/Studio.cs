using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Media;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;

namespace TracksmithGEN {
public static class PromptBuilder {
 public static string Compose(string text,bool song){if(string.IsNullOrWhiteSpace(text))throw new ArgumentException("Describe the music you want first.");return (song?"Instrumental song idea. ":"Isolated instrumental part. ")+text.Trim()+". Instrumental, no vocals.";}
 public static string Expand(string idea,string instrument,bool song,int version){
  if(string.IsNullOrWhiteSpace(idea))throw new ArgumentException("Type a word or sentence first, such as hope or a quiet night drive.");
  string s=idea.Trim(),q=s.ToLowerInvariant();string mood="expressive and evocative",feel="a steady groove",texture="warm tone and clear melodic phrasing";
  if(s.Split(new[]{' '},StringSplitOptions.RemoveEmptyEntries).Length>5)return s+". Preserve the requested instrumentation, genre and emotional direction. Use clear phrasing, intentional dynamics and a recognizable musical motif.";
  if(new[]{"sad","loss","lonely","heartbreak","grief"}.Any(q.Contains)){mood="melancholic and intimate";feel="a restrained, spacious pulse";texture="delicate dynamics, lingering notes and a bittersweet melody";}
  else if(new[]{"hope","happy","joy","sunrise","bright","uplift"}.Any(q.Contains)){mood="hopeful and uplifting";feel="a gently driving groove";texture="bright harmonies, an ascending melodic motif and a confident resolution";}
  else if(new[]{"angry","heavy","rage","fight","storm"}.Any(q.Contains)){mood="intense and powerful";feel="a forceful, syncopated pulse";texture="strong accents, rhythmic tension and a memorable hook";}
  else if(new[]{"dream","night","space","ambient","calm","ocean"}.Any(q.Contains)){mood="dreamlike and atmospheric";feel="an unhurried, spacious pulse";texture="soft attacks, sustained phrases and subtle dynamic movement";}
  else if(new[]{"rock","guitar","drive","energy"}.Any(q.Contains)){mood="energetic and anthemic";feel="a driving rock groove";texture="a distinctive melodic hook, strong articulation and contrasting phrases";}
  string[] direction={"Develop a short recurring motif with a clear ending.","Start sparsely, answer the opening phrase, and build toward a melodic lift.","Use rhythmic variation and pauses, with a contrasting response to the main motif."};
  return (song?"An instrumental song idea":instrument=="From prompt"?"An instrumental part":"A solo "+instrument.ToLowerInvariant()+" part")+" inspired by: "+s+". "+char.ToUpper(mood[0])+mood.Substring(1)+", with "+feel+", "+texture+". "+direction[Math.Abs(version%3)]+(song?" Give the arrangement an opening, a contrasting section and a return to its main theme.":" Leave room between phrases; focus on the instrument described in the idea.");
 }
}
public class SavedTake {
 public string Id,Title,Prompt,Instrument,Mode,File,Created,ReferenceId,SourceName,SourceStart,VariationAmount,Seed;public bool Favorite; public int Tempo; public float[] Peaks;
}
public class TakeStore {
 public readonly string Root; public List<SavedTake> Items=new List<SavedTake>();
 public TakeStore(string root){Root=root;Directory.CreateDirectory(root);Load();}
 public void Load(){string path=Path.Combine(Root,"library.xml");if(!System.IO.File.Exists(path))return;
  foreach(var x in XDocument.Load(path).Root.Elements("take")){string file=(string)x.Attribute("file");if(file==null||Path.GetFileName(file)!=file)continue;Items.Add(new SavedTake{Id=(string)x.Attribute("id"),Title=(string)x.Element("title"),Prompt=(string)x.Element("prompt"),Instrument=(string)x.Element("instrument"),Mode=(string)x.Element("mode"),File=file,Created=(string)x.Element("created"),Favorite=(bool?)x.Element("favorite")??false,ReferenceId=(string)x.Element("referenceId"),SourceName=(string)x.Element("sourceName"),SourceStart=(string)x.Element("sourceStart"),VariationAmount=(string)x.Element("variationAmount"),Seed=(string)x.Element("seed"),Tempo=(int?)x.Element("tempo")??100});}}
 public void Save(){var xml=new XDocument(new XElement("library",Items.Select(t=>new XElement("take",new XAttribute("id",t.Id),new XAttribute("file",t.File),new XElement("title",t.Title),new XElement("prompt",t.Prompt),new XElement("instrument",t.Instrument),new XElement("mode",t.Mode),new XElement("created",t.Created),new XElement("favorite",t.Favorite),new XElement("tempo",t.Tempo),new XElement("referenceId",t.ReferenceId),new XElement("sourceName",t.SourceName),new XElement("sourceStart",t.SourceStart),new XElement("variationAmount",t.VariationAmount),new XElement("seed",t.Seed)))));string path=Path.Combine(Root,"library.xml"),temp=path+".tmp";xml.Save(temp);if(System.IO.File.Exists(path))System.IO.File.Copy(path,path+".bak",true);System.IO.File.Copy(temp,path,true);System.IO.File.Delete(temp);}
 public SavedTake Add(string path,string title,string prompt,string instrument,string mode,int tempo){string id=Guid.NewGuid().ToString("N"),file=id+".wav";System.IO.File.Copy(path,Path.Combine(Root,file));var take=new SavedTake{Id=id,File=file,Title=title,Prompt=prompt,Instrument=instrument,Mode=mode,Tempo=tempo,Created=DateTime.Now.ToString("yyyy-MM-dd HH:mm"),Peaks=WavePeaks(path)};Items.Insert(0,take);try{Save();}catch{Items.Remove(take);throw;}return take;}
 public static float[] WavePeaks(string path){float[] samples=Engine.ReadWav(path),peaks=new float[180];for(int x=0;x<peaks.Length;x++){int a=(int)((long)x*samples.Length/peaks.Length),b=(int)((long)(x+1)*samples.Length/peaks.Length);for(int i=a;i<b;i++)peaks[x]=Math.Max(peaks[x],Math.Abs(samples[i]));}return peaks;}
}
public class NativePlayer:IDisposable {
 [DllImport("winmm.dll",CharSet=CharSet.Unicode)]static extern int mciSendString(string cmd,StringBuilder result,int length,IntPtr hwnd);
 readonly string alias="gen"+Guid.NewGuid().ToString("N");bool opened,fallback;public bool Playing;float[] samples;SoundPlayer sound;string scratch;int offset,gain=750;Stopwatch watch=new Stopwatch();
 string Send(string c){var b=new StringBuilder(256);int error=mciSendString(c,b,b.Capacity,IntPtr.Zero);if(error!=0)throw new Exception("Windows audio playback failed ("+error+"). Try exporting the WAV to your DAW.");return b.ToString();}
 public void Open(string path){Close();try{Send("open \""+path+"\" type mpegvideo alias "+alias);opened=true;Send("set "+alias+" time format milliseconds");fallback=false;}catch{mciSendString("close "+alias,null,0,IntPtr.Zero);samples=Engine.ReadWav(path);fallback=true;opened=true;offset=0;}}
 public int Length{get{return !opened?0:fallback?(int)((long)samples.Length*1000/Engine.Rate):int.Parse(Send("status "+alias+" length"));}}
 public int Position{get{return !opened?0:fallback?Math.Min(Length,offset+(Playing?(int)watch.ElapsedMilliseconds:0)):int.Parse(Send("status "+alias+" position"));}}
 public void Play(){if(!opened)return;if(fallback){int at=(int)((long)offset*Engine.Rate/1000);var clip=new float[Math.Max(1,samples.Length-at)];for(int i=0;i<clip.Length&&at+i<samples.Length;i++)clip[i]=samples[at+i]*gain/1000f;if(scratch==null)scratch=Path.Combine(Path.GetTempPath(),"GEN-play-"+Guid.NewGuid()+".wav");if(sound!=null){sound.Stop();sound.Dispose();}Engine.Wav(scratch,clip);sound=new SoundPlayer(scratch);sound.Load();sound.Play();watch.Restart();}else Send("play "+alias);Playing=true;}
 public void Pause(){if(!opened)return;if(fallback){offset=Position;if(sound!=null)sound.Stop();watch.Reset();}else Send("pause "+alias);Playing=false;}
 public void Seek(int ms){if(opened){bool resume=Playing;if(fallback){Pause();offset=Math.Max(0,Math.Min(Length,ms));}else Send("seek "+alias+" to "+ms);if(resume)Play();}}
 public void Volume(int value){gain=value;if(opened){if(fallback){if(Playing){Pause();Play();}}else Send("setaudio "+alias+" volume to "+value);}}
 public void Close(){if(opened&&!fallback)mciSendString("close "+alias,null,0,IntPtr.Zero);if(sound!=null){sound.Stop();sound.Dispose();sound=null;}if(scratch!=null){try{File.Delete(scratch);}catch{}scratch=null;}opened=false;Playing=false;watch.Reset();samples=null;offset=0;}
 public void Dispose(){Close();}
}
public class StudioForm:Form {
 readonly Color bg=Color.FromArgb(14,16,21),surface=Color.FromArgb(23,26,33),border=Color.FromArgb(46,50,62),muted=Color.FromArgb(151,157,174),accent=Color.FromArgb(128,112,245);
 Panel create=new Panel(),center=new Panel(),details=new Panel(),transport=new Panel();FlowLayoutPanel cards=new FlowLayoutPanel();
 ComboBox mode=new ComboBox(),instrument=new ComboBox(),approach=new ComboBox(),engine=new ComboBox();TextBox prompt=new TextBox(),search=new TextBox(),title=new TextBox(),detailPrompt=new TextBox();NumericUpDown duration=new NumericUpDown(),count=new NumericUpDown();
 Label status=new Label(),detailMeta=new Label(),playingTitle=new Label(),clock=new Label(),engineLabel=new Label();Button generate,play,favorite;CheckBox favorites=new CheckBox();TrackBar seek=new TrackBar(),volume=new TrackBar();
 TakeStore store;SavedTake selected,loaded;NativePlayer player=new NativePlayer();Process inference;bool busy,cancelled,scrubbing,testing;string enginePath,testFailure,testPython;Timer timer=new Timer();int generationSerial,promptVersion;string lastIdea,lastExpansion;ProgressBar progress=new ProgressBar();Stopwatch generationWatch=new Stopwatch();string generationStage="Starting GPU worker";
 string BaseDir{get{return AppDomain.CurrentDomain.BaseDirectory;}}
 public StudioForm(){
  Text="Tracksmith GEN / GPU Studio 0.8.1 / Audio Import";ClientSize=new Size(1360,850);MinimumSize=new Size(1260,740);BackColor=bg;ForeColor=Color.FromArgb(237,238,244);Font=new Font("Segoe UI",10);StartPosition=FormStartPosition.CenterScreen;
  enginePath=AceBridge.DefaultRoot();string settings=Path.Combine(BaseDir,"gen-ace-root.txt");if(File.Exists(settings))enginePath=File.ReadAllText(settings).Trim();
  create.AutoScroll=true;details.AutoScroll=true;create.BackColor=surface;details.BackColor=surface;transport.BackColor=surface;Controls.AddRange(new Control[]{create,center,details,transport});
  LabelAt(create,"TRACKSMITH",24,20,260,35,19,true);var gen=LabelAt(create,"GEN  /  CREATE",25,58,260,28,11,true);gen.ForeColor=accent;
  LabelAt(create,"Make something yours.",25,103,280,28,16,true);
  LabelAt(create,"Create",25,147,270,22,10,false);Choice(mode,create,25,175,280,new[]{"Instrument part","Full song idea"});
  LabelAt(create,"Generation engine",25,215,280,22,10,false).ForeColor=muted;Choice(engine,create,25,240,280,new[]{"Stable Audio 3 / new engine","ACE-Step / original engine"});
  LabelAt(create,"ACE generation approach",25,278,280,22,10,false).ForeColor=muted;instrument.Items.Add("From prompt");instrument.SelectedIndex=0;
  Choice(approach,create,25,302,280,new[]{"Balanced / current settings","Direct / no musical planner","Larger musical planner","Extended render / 100 steps"});
  LabelAt(create,"One word, a sentence, or a full prompt",25,344,280,25,10,false);prompt.Multiline=true;prompt.MaxLength=0;prompt.ScrollBars=ScrollBars.Vertical;prompt.Text="Hope";TextStyle(prompt);prompt.SetBounds(25,373,280,163);create.Controls.Add(prompt);
  ButtonAt(create,"Expand idea",25,544,90,31,()=>{string idea=prompt.Text==lastExpansion?lastIdea:prompt.Text;lastIdea=idea;lastExpansion=PromptBuilder.Expand(idea,instrument.Text,mode.SelectedIndex==1,promptVersion++);prompt.Text=lastExpansion;status.Text="Expanded your idea with local musical rules. Edit the prompt freely, then click Multi Platinum.";});
  ButtonAt(create,"Restore",122,544,80,31,()=>{if(lastIdea!=null)prompt.Text=lastIdea;});
  ButtonAt(create,"Preview",209,544,96,31,()=>{MessageBox.Show(this,PromptBuilder.Compose(prompt.Text,mode.SelectedIndex==1),"Exact prompt sent to the engine",MessageBoxButtons.OK,MessageBoxIcon.Information);});
  LabelAt(create,"Duration / seconds",25,586,155,22,9,false);Number(duration,create,25,612,128,10,120,15);
  LabelAt(create,"Takes",177,586,128,22,9,false);Number(count,create,177,612,128,1,3,1);
  LabelAt(create,"Write key and tempo in your prompt.",25,650,280,22,9,false).ForeColor=muted;
  generate=ButtonAt(create,"Multi Platinum",25,677,280,45,async()=>await Generate(false));generate.BackColor=accent;
  ButtonAt(create,"Engine settings",25,724,135,30,()=>EngineSettings());ButtonAt(create,"Sketch tools",170,724,135,30,()=>{using(var old=new MainForm())old.ShowDialog(this);});
  LabelAt(center,"Your creations",24,23,350,42,24,true);LabelAt(center,"Ideas become parts. Parts become music.",25,70,550,26,10,false).ForeColor=muted;
  LabelAt(center,"Search titles, instruments or prompts",25,98,330,18,8,false).ForeColor=muted;search.SetBounds(25,116,330,32);TextStyle(search);center.Controls.Add(search);search.TextChanged+=(s,e)=>RefreshCards();
  favorites.Text="Favorites";favorites.SetBounds(380,116,135,30);center.Controls.Add(favorites);favorites.CheckedChanged+=(s,e)=>RefreshCards();
  cards.AutoScroll=true;cards.FlowDirection=FlowDirection.TopDown;cards.WrapContents=false;cards.BackColor=bg;center.Controls.Add(cards);
  status.ForeColor=muted;status.SetBounds(25,680,550,70);center.Controls.Add(status);progress.Style=ProgressBarStyle.Marquee;progress.MarqueeAnimationSpeed=35;progress.Visible=false;center.Controls.Add(progress);
  LabelAt(details,"SELECTED TAKE",22,28,240,25,10,true).ForeColor=muted;title.SetBounds(22,77,236,31);TextStyle(title);details.Controls.Add(title);
  detailMeta.SetBounds(22,126,236,90);detailMeta.ForeColor=muted;details.Controls.Add(detailMeta);
  LabelAt(details,"Prompt",22,220,230,24,10,true);detailPrompt.Multiline=true;detailPrompt.ReadOnly=true;detailPrompt.ScrollBars=ScrollBars.Vertical;detailPrompt.SetBounds(22,254,236,150);TextStyle(detailPrompt);details.Controls.Add(detailPrompt);
  ButtonAt(details,"Save name",22,426,236,36,()=>{NeedSelected();if(string.IsNullOrWhiteSpace(title.Text))throw new Exception("Enter a name first.");selected.Title=title.Text.Trim();store.Save();RefreshCards();playingTitle.Text=selected.Title;});
  favorite=ButtonAt(details,"Favorite",22,477,236,36,()=>{NeedSelected();selected.Favorite=!selected.Favorite;store.Save();SelectTake(selected,false);RefreshCards();});
  ButtonAt(details,"Create variation",22,528,236,36,()=>OpenReference());
  ButtonAt(details,"Export WAV",22,579,236,42,()=>Export());
  ButtonAt(details,"Import WAV / MP3",22,636,236,36,()=>Import());
  play=ButtonAt(transport,"Play",25,23,90,40,()=>TogglePlay());playingTitle=LabelAt(transport,"Select a take",135,13,550,26,11,true);
  seek.Minimum=0;seek.Maximum=1000;seek.TickStyle=TickStyle.None;seek.SetBounds(128,44,650,30);transport.Controls.Add(seek);seek.MouseDown+=(s,e)=>scrubbing=true;seek.MouseUp+=(s,e)=>{scrubbing=false;try{if(loaded!=null)player.Seek((int)((long)player.Length*seek.Value/1000));}catch(Exception ex){Error(ex);}};
  clock=LabelAt(transport,"0:00 / 0:00",805,28,160,28,10,false);volume.Minimum=0;volume.Maximum=100;volume.Value=75;volume.TickStyle=TickStyle.None;volume.SetBounds(1000,29,150,35);transport.Controls.Add(volume);volume.ValueChanged+=(s,e)=>{try{player.Volume(volume.Value*10);}catch(Exception ex){Error(ex);}};LabelAt(transport,"Volume",1005,8,150,20,9,false).ForeColor=muted;
  engineLabel=LabelAt(transport,"ACE / NVIDIA GPU",1180,29,150,25,9,true);engineLabel.ForeColor=accent;
  mode.SelectedIndexChanged+=(s,e)=>{RefreshExpandedIdea();status.Text="Describe any instruments in your prompt. No default instrument is added.";};
  timer.Interval=200;timer.Tick+=(s,e)=>UpdateTransport();timer.Start();Resize+=(s,e)=>LayoutStudio();
  try{store=new TakeStore(Path.Combine(BaseDir,"GEN-Library"));status.Text=AceBridge.Valid(enginePath)?"ACE-Step GPU engine found. Ready for Multi Platinum.":"Choose your installed ACE-Step-1.5 folder using Engine settings.";RefreshCards();}catch(Exception ex){status.Text="Cannot open the saved library: "+ex.Message;generate.Enabled=false;}
  approach.SelectedIndexChanged+=(s,e)=>{string[] messages={"Balanced: the settings already tested in GEN 0.5.","Direct: skip the musical planner and use stronger guidance. Instrument accuracy still needs listening.","Larger planner: use the cached 1.7B planning model. This is an alternative, not a guaranteed quality upgrade.","Extended: larger planner and 100 rendering steps. Slower; listen to decide whether it helps."};if(engine.SelectedIndex==1&&approach.SelectedIndex>=0)status.Text=messages[approach.SelectedIndex];};
  KeyPreview=true;KeyDown+=async(s,e)=>{if(e.Control&&e.KeyCode==Keys.Enter){e.SuppressKeyPress=true;await Generate(false);}else if(e.KeyCode==Keys.Space&&!prompt.Focused&&!search.Focused&&!title.Focused&&!detailPrompt.Focused&&selected!=null){e.SuppressKeyPress=true;try{TogglePlay();}catch(Exception ex){Error(ex);}}};
  engine.SelectedIndexChanged+=(s,e)=>ApplyEngine();LoadPreferences();ApplyEngine();FormClosing+=(s,e)=>{if(busy){e.Cancel=true;Cancel();status.Text="Cancelling. Close the app again when generation stops.";}else{try{SavePreferences();}catch{}timer.Stop();player.Dispose();}};LayoutStudio();
 }
 void LayoutStudio(){int h=ClientSize.Height-92;create.SetBounds(0,0,330,h);details.SetBounds(ClientSize.Width-280,0,280,h);center.SetBounds(330,0,ClientSize.Width-610,h);transport.SetBounds(0,h,ClientSize.Width,92);cards.SetBounds(20,165,center.Width-40,Math.Max(200,h-275));status.SetBounds(25,h-83,center.Width-45,76);progress.SetBounds(25,h-105,center.Width-45,8);foreach(Control card in cards.Controls)card.Width=cards.ClientSize.Width-25;engineLabel.Left=ClientSize.Width-170;volume.Left=ClientSize.Width-350;clock.Left=ClientSize.Width-550;seek.Width=ClientSize.Width-720;}
 Label LabelAt(Control p,string text,int x,int y,int w,int h,int size,bool bold){var l=new Label{Text=text,Location=new Point(x,y),Size=new Size(w,h),Font=new Font("Segoe UI",size,bold?FontStyle.Bold:FontStyle.Regular)};p.Controls.Add(l);return l;}
 Button ButtonAt(Control p,string text,int x,int y,int w,int h,Action action){var b=new Button{Text=text,Location=new Point(x,y),Size=new Size(w,h),FlatStyle=FlatStyle.Flat,BackColor=Color.FromArgb(35,39,50),ForeColor=ForeColor};b.FlatAppearance.BorderColor=border;b.Click+=(s,e)=>{try{action();}catch(Exception ex){Error(ex);}};p.Controls.Add(b);return b;}
 void TextStyle(TextBox t){t.BackColor=Color.FromArgb(32,36,46);t.ForeColor=ForeColor;t.BorderStyle=BorderStyle.FixedSingle;}
 void Choice(ComboBox c,Control p,int x,int y,int w,string[] items){c.DropDownStyle=ComboBoxStyle.DropDownList;c.DrawMode=DrawMode.Normal;c.DropDownHeight=240;c.Items.AddRange(items);c.SetBounds(x,y,w,30);c.BackColor=Color.White;c.ForeColor=Color.Black;p.Controls.Add(c);c.SelectedIndex=0;}
 void RefreshExpandedIdea(){if(lastIdea!=null&&prompt.Text==lastExpansion){lastExpansion=PromptBuilder.Expand(lastIdea,instrument.Text,mode.SelectedIndex==1,promptVersion);prompt.Text=lastExpansion;}}
 public void TestInstrumentSelection(){foreach(bool song in new[]{true,false}){string p=PromptBuilder.Compose("metalcore with heavy guitars in D minor",song);if(p.ToLowerInvariant().Contains("piano")||!p.Contains("heavy guitars"))throw new Exception("Unexpected instrument injection.");}string detailed="metal core in the key of d minor. emotional, hopeful. heavy guitars and melodic phrasing";string expanded=PromptBuilder.Expand(detailed,"From prompt",true,0);if(!expanded.StartsWith(detailed)||expanded.Contains("gently driving"))throw new Exception("Detailed direction was overwritten.");}
 void Number(NumericUpDown n,Control p,int x,int y,int w,int min,int max,int val){n.Minimum=min;n.Maximum=max;n.Value=val;n.SetBounds(x,y,w,30);n.BackColor=Color.FromArgb(32,36,46);n.ForeColor=ForeColor;p.Controls.Add(n);}
 void Error(Exception ex){status.Text=ex.Message;if(testing)testFailure=ex.ToString();else MessageBox.Show(this,ex.Message,"Tracksmith GEN",MessageBoxButtons.OK,MessageBoxIcon.Information);}
 void NeedSelected(){if(selected==null)throw new Exception("Select a take first.");}
 public void TestRememberedSettings(){prompt.Text="Remember this guitar idea";mode.SelectedIndex=1;approach.SelectedIndex=2;duration.Value=20;count.Value=2;engine.SelectedIndex=1;SavePreferences();engine.SelectedIndex=0;prompt.Text="Changed";approach.SelectedIndex=0;duration.Value=10;LoadPreferences();if(prompt.Text!="Remember this guitar idea"||approach.SelectedIndex!=2||duration.Value!=20||count.Value!=2||mode.SelectedIndex!=1||engine.SelectedIndex!=1)throw new Exception("Preference roundtrip failed");}
 public void TestPromptOnly(){
  string text="D minor, 110 BPM. "+new string('x',50000)+" END OF PROMPT";prompt.Text=text;if(prompt.Text!=text||prompt.MaxLength!=0)throw new Exception("Prompt editor truncated long text.");
  string sent=PromptBuilder.Compose(prompt.Text,true);if(!sent.Contains(text)||sent.Contains("100 BPM"))throw new Exception("Prompt changed or default tempo injected.");
  string request=Path.Combine(Path.GetTempPath(),"GEN-long-prompt-"+Guid.NewGuid()+".xml");try{AceBridge.WriteRequest(request,"model",sent,10,1,0,"Any",42);var root=XDocument.Load(request).Root;if((string)root.Element("caption")!=sent||(string)root.Element("bpm")!="0"||(string)root.Element("key")!="")throw new Exception("Prompt request roundtrip failed.");}finally{File.Delete(request);}
  SavePreferences();prompt.Text="changed";LoadPreferences();if(prompt.Text!=text)throw new Exception("Long prompt preferences truncated.");
  var prefs=XDocument.Load(Path.Combine(BaseDir,"gen-preferences.xml"));if(prefs.Root.Element("tempo")!=null||prefs.Root.Element("key")!=null||prefs.Root.Element("suggestions")!=null)throw new Exception("Selector preferences still persisted.");
  prompt.Text="Hope";
 }
 void ApplyEngine(){bool stable=engine.SelectedIndex==0;approach.Enabled=!stable&&!busy;engineLabel.Text=stable?"Stable / NVIDIA GPU":"ACE / NVIDIA GPU";if(store!=null&&!busy)status.Text=stable?"Stable Audio 3 Small Music selected. Multi Platinum generates from your prompt.":"ACE-Step selected. Choose an ACE generation approach.";}
 public void TestEngineSelection(){engine.SelectedIndex=0;if(approach.Enabled)throw new Exception("ACE approach remains active for Stable Audio.");engine.SelectedIndex=1;if(!approach.Enabled)throw new Exception("ACE approach was not enabled.");engine.SelectedIndex=0;SavePreferences();engine.SelectedIndex=1;LoadPreferences();if(engine.SelectedIndex!=0)throw new Exception("Stable engine preference did not persist.");}
 public async Task TestStableWorkflow(string python){testing=true;testPython=python;enginePath="ACE intentionally unavailable for test";engine.SelectedIndex=0;mode.SelectedIndex=1;duration.Value=10;count.Value=1;prompt.Text=string.Join(" ",Enumerable.Repeat("Alternative rock instrumental in D minor, 110 BPM. Tight distorted guitars, warm bass and punchy drums. Clear melodic hook and natural dynamics.",5));int before=store.Items.Count;await Generate(false);if(testFailure!=null)throw new Exception(testFailure);if(store.Items.Count!=before+1||selected==null||selected.Instrument!="Stable Audio 3 Small Music"||selected.Prompt!=PromptBuilder.Compose(prompt.Text,true))throw new Exception("Multi Platinum did not save a Stable Audio take with the full prompt.");var reopened=new TakeStore(store.Root);if(reopened.Items[0].Instrument!=selected.Instrument||reopened.Items[0].Prompt!=selected.Prompt)throw new Exception("Stable Audio take did not reopen.");using(var audition=new NativePlayer()){audition.Open(Path.Combine(store.Root,selected.File));if(Math.Abs(audition.Length-10000)>100)throw new Exception("Stable Audio output duration failed.");}}
 string ProfileId(){return new[]{"balanced","direct","planner","extended"}[Math.Max(0,approach.SelectedIndex)];}
 void SavePreferences(){new XDocument(new XElement("preferences",new XElement("prompt",prompt.Text),new XElement("scope",mode.SelectedIndex),new XElement("approach",approach.SelectedIndex),new XElement("duration",duration.Value),new XElement("count",count.Value),new XElement("engine",engine.SelectedIndex==0?"stable":"ace"))).Save(Path.Combine(BaseDir,"gen-preferences.xml"));}
 void LoadPreferences(){string path=Path.Combine(BaseDir,"gen-preferences.xml");if(!File.Exists(path))return;try{var p=XDocument.Load(path).Root;prompt.Text=(string)p.Element("prompt")??"Hope";engine.SelectedIndex=(string)p.Element("engine")=="ace"?1:0;mode.SelectedIndex=Math.Max(0,Math.Min(1,(int?)p.Element("scope")??0));approach.SelectedIndex=Math.Max(0,Math.Min(3,(int?)p.Element("approach")??0));duration.Value=Math.Max(duration.Minimum,Math.Min(duration.Maximum,(decimal?)p.Element("duration")??15));count.Value=Math.Max(1,Math.Min(3,(decimal?)p.Element("count")??1));}catch(Exception ex){status.Text="Saved preferences could not be loaded: "+ex.Message;}}
 void RefreshCards(){if(store==null)return;cards.SuspendLayout();while(cards.Controls.Count>0){var old=cards.Controls[0];cards.Controls.RemoveAt(0);old.Dispose();}string q=search.Text.Trim();
  var visible=store.Items.Where(t=>(!favorites.Checked||t.Favorite)&&(q.Length==0||(t.Title+" "+t.Instrument+" "+t.Prompt).IndexOf(q,StringComparison.OrdinalIgnoreCase)>=0)).ToArray();
  foreach(var t in visible){var card=new Panel{Width=Math.Max(350,cards.ClientSize.Width-25),Height=132,Margin=new Padding(4,4,4,12),BackColor=selected==t?Color.FromArgb(38,35,60):surface,Cursor=Cursors.Hand};
   card.Paint+=(s,e)=>{using(var pen=new Pen(selected==t?accent:border))e.Graphics.DrawRectangle(pen,0,0,card.Width-1,card.Height-1);if(t.Peaks!=null){using(var pen=new Pen(accent)){for(int i=0;i<t.Peaks.Length;i++){float x=22+(card.Width-44)*i/(float)t.Peaks.Length,amp=Math.Max(1,t.Peaks[i]*17);e.Graphics.DrawLine(pen,x,103-amp,x,103+amp);}}}};
   var name=LabelAt(card,(t.Favorite?"*  ":"")+t.Title,20,12,card.Width-115,30,12,true);var meta=LabelAt(card,t.Instrument+"  /  "+t.Mode+"  /  "+t.Created,20,45,card.Width-30,25,9,false);name.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;meta.Anchor=name.Anchor;meta.ForeColor=muted;
   card.Click+=(s,e)=>SelectTake(t,true);name.Click+=(s,e)=>SelectTake(t,true);meta.Click+=(s,e)=>SelectTake(t,true);ButtonAt(card,"Play",card.Width-85,15,65,31,()=>{SelectTake(t,true);TogglePlay();}).Anchor=AnchorStyles.Top|AnchorStyles.Right;cards.Controls.Add(card);
  }
  if(visible.Length==0)LabelAt(cards,store.Items.Count==0?"Your first idea starts here.\nDescribe your music and click Multi Platinum.":"No takes match this view.",10,10,500,100,12,false).ForeColor=muted;
  cards.ResumeLayout();
 }
 void SelectTake(SavedTake t,bool refresh){selected=t;title.Text=t.Title;detailPrompt.Text=t.Prompt;detailMeta.Text=t.Instrument+"\n"+t.Mode+"\n"+t.Created+(string.IsNullOrEmpty(t.ReferenceId)?"":"\nReference: "+t.SourceName+" / change "+t.VariationAmount);favorite.Text=t.Favorite?"Remove favorite":"Add favorite";
  if(t.Peaks==null){try{t.Peaks=TakeStore.WavePeaks(Path.Combine(store.Root,t.File));}catch(Exception ex){status.Text="Audio unavailable: "+ex.Message;}}
  if(refresh)RefreshCards();
 }
 void TogglePlay(){NeedSelected();if(loaded!=selected){player.Open(Path.Combine(store.Root,selected.File));loaded=selected;player.Volume(volume.Value*10);playingTitle.Text=selected.Title;}if(player.Playing){player.Pause();play.Text="Play";}else{if(player.Position>=player.Length-30)player.Seek(0);player.Play();play.Text="Pause";}}
 static string Time(int ms){return (ms/60000)+":"+((ms/1000)%60).ToString("00");}
 void UpdateTransport(){if(busy)status.Text=generationStage+" / elapsed "+generationWatch.Elapsed.ToString(@"m\:ss");if(loaded==null)return;try{int length=player.Length,position=player.Position;if(!scrubbing)seek.Value=length==0?0:Math.Min(1000,(int)((long)position*1000/length));clock.Text=Time(position)+" / "+Time(length);if(position>=length-20&&player.Playing){player.Playing=false;play.Text="Play";}}catch{player.Close();loaded=null;play.Text="Play";}}
 void Export(){NeedSelected();using(var d=new SaveFileDialog{Filter="WAV audio|*.wav",FileName=string.Concat(selected.Title.Select(c=>Path.GetInvalidFileNameChars().Contains(c)?'_':c))+".wav"})if(d.ShowDialog()==DialogResult.OK){string source=Path.GetFullPath(Path.Combine(store.Root,selected.File));if(string.Equals(source,Path.GetFullPath(d.FileName),StringComparison.OrdinalIgnoreCase))return;File.Copy(source,d.FileName,true);status.Text="Exported original WAV: "+Path.GetFileName(d.FileName);}}
 void Import(){if(busy)return;using(var d=new OpenFileDialog{Filter="Audio recordings|*.wav;*.mp3"})if(d.ShowDialog(this)==DialogResult.OK)OpenReference(d.FileName);}
 void OpenReference(){if(busy)return;NeedSelected();OpenReference(Path.Combine(store.Root,selected.File));}
 void OpenReference(string path){if(busy)return;player.Close();loaded=null;play.Text="Play";using(var form=new ReferenceForm(store,path))form.ShowDialog(this);if(store.Items.Count>0)SelectTake(store.Items[0],true);else RefreshCards();status.Text="Audio-guided takes are saved in Your creations. Multi Platinum uses the generation engine selected on the left.";}

 void EngineSettings(){if(busy)return;if(engine.SelectedIndex==0){using(var file=new OpenFileDialog{Title="Choose python.exe inside the working Stable Audio .venv / Scripts folder",Filter="Python executable|python.exe",FileName=StableBridge.Python()})if(file.ShowDialog(this)==DialogResult.OK){File.WriteAllText(Path.Combine(BaseDir,"gen-reference-python.txt"),file.FileName);status.Text="Stable Audio environment selected. Cached models will be reused.";}return;}
using(var d=new FolderBrowserDialog{Description="Choose ACE-Step-1.5 inside your existing TracksmithGEN-ACE-Step-GPU-Test-Kit folder",SelectedPath=Directory.Exists(enginePath)?enginePath:Environment.GetFolderPath(Environment.SpecialFolder.UserProfile)})if(d.ShowDialog()==DialogResult.OK){if(!AceBridge.Valid(d.SelectedPath))throw new Exception("Choose the ACE-Step-1.5 folder containing .venv and acestep. No reinstall is required.");enginePath=d.SelectedPath;File.WriteAllText(Path.Combine(BaseDir,"gen-ace-root.txt"),enginePath);status.Text="ACE-Step GPU installation selected. Existing cached model will be reused.";}}
 void Cancel(){cancelled=true;generationStage="Cancelling GPU job";try{if(inference!=null&&!inference.HasExited)inference.Kill();}catch{}}
 async Task Generate(bool variation){
  if(busy){Cancel();return;}if(store==null)return;
  string description=prompt.Text.Trim(),chosenMode=mode.Text,repo=enginePath,chosenProfile=ProfileId();bool useStable=engine.SelectedIndex==0;string python=testPython??StableBridge.Python();
  int bpm=0,length=(int)duration.Value,total=(int)count.Value;
  string chosenKey="Any";
  try{
   if(variation){NeedSelected();description=selected.Prompt;chosenMode=selected.Mode;bpm=0;chosenKey="Any";}
   else description=PromptBuilder.Compose(description,mode.SelectedIndex==1);
   if(!useStable&&!AceBridge.Valid(repo))throw new Exception("Use Engine settings to choose your existing ACE-Step-1.5 folder. No reinstall needed.");
   if(useStable&&!File.Exists(python))throw new Exception("Use Engine settings to choose python.exe from your working Stable Audio .venv / Scripts folder.");
   string worker=Path.Combine(BaseDir,useStable?"stable_worker.py":"ace_worker.py");if(!File.Exists(worker))throw new Exception("Copy "+Path.GetFileName(worker)+" beside TracksmithGEN.exe from the complete update ZIP.");
   busy=true;cancelled=false;engine.Enabled=false;approach.Enabled=false;generate.Text="Cancel generation";progress.Visible=true;generationWatch.Restart();generationStage="Starting GPU worker";
   string job=Path.Combine(Path.GetTempPath(),"GEN-GPU-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(job);
   string request=Path.Combine(job,"request.xml");if(useStable)StableBridge.WriteRequest(request,description,length,total,-1);else AceBridge.WriteRequest(request,repo,description,length,total,bpm,chosenKey,-1,chosenProfile);SavePreferences();
   status.Text="Starting "+(useStable?"Stable Audio 3":"ACE-Step")+" on GPU. First take includes model loading.";
   var log=new StringBuilder();object gate=new object();
   DataReceivedEventHandler collect=(s,e)=>{if(e.Data==null)return;lock(gate){if(log.Length>18000)log.Remove(0,9000);log.AppendLine(e.Data);}if(e.Data.StartsWith("GEN_STAGE\t")&&!IsDisposed){string stage=e.Data.Substring(10);try{BeginInvoke(new Action(()=>{if(busy)generationStage=stage;}));}catch{}}};
   inference=new Process{StartInfo=useStable?StableBridge.StartInfo(python,request):AceBridge.StartInfo(repo,request)};inference.OutputDataReceived+=collect;inference.ErrorDataReceived+=collect;
   inference.Start();inference.BeginOutputReadLine();inference.BeginErrorReadLine();bool exited=await Task.Run(()=>inference.WaitForExit(30*60*1000));
   if(!exited){Cancel();throw new Exception("GPU generation timed out after 30 minutes. Diagnostics: "+job);}
   inference.WaitForExit();int exitCode=inference.ExitCode;int completed=0;
   string results=Path.Combine(job,"results.xml");if(File.Exists(results)){
    var document=XDocument.Load(results);if((string)document.Root.Attribute("device")!="cuda")throw new Exception("The worker did not confirm CUDA generation.");if(useStable&&(string)document.Root.Attribute("model")!="stable-audio-3-small-music")throw new Exception("The worker returned an unexpected audio engine.");
    foreach(var item in document.Root.Elements("take")){
     string name=(string)item.Attribute("file");if(name==null||Path.GetFileName(name)!=name)throw new Exception("Invalid worker output filename.");
     string wav=Path.Combine(job,name);string label=chosenMode=="Instrument part"?"Instrument part":"Song idea";
     var t=store.Add(wav,label+" / "+DateTime.Now.ToString("HH:mm")+" / "+(++generationSerial),description,useStable?"Stable Audio 3 Small Music":"ACE SFT / "+chosenProfile,chosenMode,bpm);t.Seed=(string)item.Attribute("seed");store.Save();completed++;SelectTake(t,true);
    }
   }
   if(cancelled){status.Text="Cancelled. "+completed+" completed take(s) saved.";return;}
   if(exitCode!=0){string error;lock(gate)error=log.ToString();if(error.Length>2000)error=error.Substring(error.Length-2000);throw new Exception("GPU generation failed. "+completed+" completed take(s) saved.\n"+error+"\nDiagnostics: "+job);}
   if(completed==0)throw new Exception("The GPU worker returned no completed takes. Diagnostics: "+job);
   status.Text=(useStable?"Stable Audio 3":"ACE-Step")+" finished on NVIDIA GPU. "+completed+" WAV take(s) saved automatically.";
  }catch(Exception ex){busy=false;Error(ex);}finally{if(inference!=null){inference.Dispose();inference=null;}busy=false;progress.Visible=false;generationWatch.Stop();generate.Text="Multi Platinum";engine.Enabled=true;approach.Enabled=engine.SelectedIndex==1;}
 }
}
}
