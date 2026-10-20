using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;

namespace TracksmithGEN {
public static class ReferenceBridge {
 public static string DefaultPython(){
  string downloads=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),"Downloads");
  if(Directory.Exists(downloads))foreach(string outer in Directory.GetDirectories(downloads,"tracksmith-gen-0.7-guitar-model-trial*")){
   var roots=new List<string>{outer};roots.AddRange(Directory.GetDirectories(outer,"tracksmith-gen-0.7-guitar-model-trial*"));
   foreach(string root in roots){string p=Path.Combine(root,"TracksmithGEN","experiments","sa3-guitar",".venv","Scripts","python.exe");if(File.Exists(p))return p;}
  }return "";
 }
 public static void Request(string path,string source,double start,double duration,double amount,int count,string prompt,bool prepare,int seed){
  var ci=CultureInfo.InvariantCulture;
  new XDocument(new XElement("request",new XElement("mode",prepare?"prepare":"generate"),new XElement("source",Path.GetFullPath(source)),new XElement("start",start.ToString(ci)),new XElement("duration",duration.ToString(ci)),new XElement("amount",amount.ToString(ci)),new XElement("count",count),new XElement("prompt",prompt),new XElement("seed",seed))).Save(path);
 }
 public static ProcessStartInfo StartInfo(string python,string request){
  if(!File.Exists(python))throw new Exception("Select python.exe from your working Stable Audio test environment using Model settings.");
  string worker=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"reference_worker.py");if(!File.Exists(worker))throw new Exception("Keep reference_worker.py beside TracksmithGEN.exe.");
  var info=new ProcessStartInfo(python,"-B -u "+AudioLab.Quote(worker)+" "+AudioLab.Quote(request)){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=AppDomain.CurrentDomain.BaseDirectory};
  info.EnvironmentVariables["HF_HUB_OFFLINE"]="1";info.EnvironmentVariables["TRANSFORMERS_OFFLINE"]="1";return info;
 }
 public static string Output(string job,string name){if(string.IsNullOrWhiteSpace(name)||Path.GetFileName(name)!=name)throw new Exception("Invalid worker output filename.");string p=Path.Combine(job,name);if(!File.Exists(p))throw new Exception("Worker audio is missing: "+name);return p;}
 public static List<SavedTake> SaveResults(TakeStore store,string job){
  var root=XDocument.Load(Path.Combine(job,"results.xml")).Root;
  var rows=root.Elements("take").ToArray();var added=new List<SavedTake>();if(rows.Length==0)return added;
  string source=(string)root.Attribute("source_name")??"Recording",prompt=(string)root.Attribute("prompt")??"Audio variation";
  string sourcePath=Output(job,(string)root.Attribute("reference"));
  foreach(var row in rows)Output(job,(string)row.Attribute("file"));
  var original=store.Add(sourcePath,Path.GetFileNameWithoutExtension(source)+" / reference", "Selected source section; no model processing.","Imported recording","Reference",0);
  original.SourceName=source;original.SourceStart=(string)root.Attribute("start");added.Add(original);
  foreach(var row in rows){var take=store.Add(Output(job,(string)row.Attribute("file")),Path.GetFileNameWithoutExtension(source)+" / variation "+(added.Count),prompt,"Stable Audio 3 Small Music","Audio-guided variation",0);
   take.ReferenceId=original.Id;take.SourceName=source;take.SourceStart=(string)root.Attribute("start");take.VariationAmount=(string)root.Attribute("amount");take.Seed=(string)row.Attribute("seed");added.Add(take);
  }store.Save();return added;
 }
 public static int Smoke(string source,string python){
  string job=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"reference-smoke-"+DateTime.Now.ToString("yyyyMMdd-HHmmss"));Directory.CreateDirectory(job);
  try{string request=Path.Combine(job,"request.xml");Request(request,source,0,5,.5,1,"Solo electric guitar riff, clear picked notes, natural string decay. No drums or vocals.",false,31415);
   using(var process=new Process{StartInfo=StartInfo(python,request)}){var log=new StringBuilder();object gate=new object();DataReceivedEventHandler read=(s,e)=>{if(e.Data!=null)lock(gate)log.AppendLine(e.Data);};process.OutputDataReceived+=read;process.ErrorDataReceived+=read;process.Start();process.BeginOutputReadLine();process.BeginErrorReadLine();if(!process.WaitForExit(10*60*1000)){process.Kill();throw new Exception("Reference smoke timed out.");}process.WaitForExit();File.WriteAllText(Path.Combine(job,"process.log"),log.ToString());if(process.ExitCode!=0)throw new Exception("Worker failed. See "+job);}
   var store=new TakeStore(Path.Combine(job,"saved-library"));var added=SaveResults(store,job);var reopened=new TakeStore(store.Root);
   if(added.Count!=2||reopened.Items.Count!=2||reopened.Items[0].ReferenceId!=added[0].Id)throw new Exception("Reference lineage did not persist.");
   if(!File.ReadAllBytes(Path.Combine(job,"variation-1.wav")).SequenceEqual(File.ReadAllBytes(Path.Combine(store.Root,added[1].File))))throw new Exception("WAV was modified while saving.");
   File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"reference-smoke-result.txt"),"PASS: native request -> cached CUDA audio-to-audio worker -> reference + variation -> saved library and lineage reopen. Sound quality is unrated. "+job);return 0;
  }catch(Exception ex){File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"reference-smoke-result.txt"),"FAIL: "+ex);return 1;}
 }
}

public class ReferenceForm:Form {
 readonly TakeStore store;string source,python,prepared,variation,playingPath,testFailure;Process process;bool busy,cancelled,testing;readonly NativePlayer player=new NativePlayer();
 TextBox sourceText=new TextBox(),prompt=new TextBox();NumericUpDown start=new NumericUpDown(),duration=new NumericUpDown(),count=new NumericUpDown();TrackBar amount=new TrackBar();Label status=new Label(),amountLabel=new Label();Button generate;Timer timer=new Timer();Stopwatch elapsed=new Stopwatch();string stage="";
 public ReferenceForm(TakeStore library,string path){
  store=library;source=path;Text="Tracksmith GEN / Audio-guided variation";ClientSize=new Size(760,650);MinimumSize=new Size(780,690);BackColor=Color.FromArgb(23,26,33);ForeColor=Color.White;Font=new Font("Segoe UI",10);StartPosition=FormStartPosition.CenterParent;
  string config=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"gen-reference-python.txt");python=File.Exists(config)?File.ReadAllText(config).Trim():ReferenceBridge.DefaultPython();
  LabelAt("Build on your recording",24,20,650,35,20);LabelAt("Choose a section, describe a change, then compare both takes.",25,65,700,25,10);
  sourceText.ReadOnly=true;sourceText.SetBounds(25,105,550,30);Controls.Add(sourceText);sourceText.Text=Path.GetFileName(source);
  ButtonAt("Choose audio",590,103,140,34,()=>Choose());
  LabelAt("Start / seconds",25,155,180,22,10);Number(start,25,181,150,0,86400,0);start.DecimalPlaces=1;start.Increment=.5M;
  LabelAt("Section / seconds",220,155,180,22,10);Number(duration,220,181,150,5,30,15);duration.DecimalPlaces=1;
  LabelAt("Takes",420,155,160,22,10);Number(count,420,181,100,1,3,1);
  ButtonAt("Preview section",550,179,180,34,async()=>await Run(true));
  LabelAt("Describe your variation",25,235,650,24,10);prompt.Multiline=true;prompt.ScrollBars=ScrollBars.Vertical;prompt.SetBounds(25,266,705,78);prompt.MaxLength=0;prompt.Text="A melodic electric guitar variation with clear phrasing and natural articulation. Keep the musical character of the recording.";Controls.Add(prompt);
  amountLabel=LabelAt("Variation amount: 50%",25,362,650,25,11);amount.SetBounds(20,392,715,40);amount.Minimum=5;amount.Maximum=95;amount.Value=50;amount.TickFrequency=10;Controls.Add(amount);amount.ValueChanged+=(s,e)=>amountLabel.Text="Variation amount: "+amount.Value+"%";
  LabelAt("Lower: stays closer to source                    Higher: allows more change",25,435,705,25,10);
  generate=ButtonAt("Generate variation",25,478,260,43,async()=>await Run(false));
  ButtonAt("A / Original",305,478,200,43,()=>Audition(prepared));ButtonAt("B / Variation",525,478,205,43,()=>Audition(variation));
  ButtonAt("Model settings",25,540,170,33,()=>Settings());ButtonAt("Export variation",220,540,200,33,()=>Export());ButtonAt("Stop playback",445,540,180,33,()=>{player.Pause();});
  status=LabelAt("WAV / MP3. Source files are kept intact. Results are saved in Your creations.",25,590,705,52,10);
  Action invalidate=()=>{prepared=null;variation=null;playingPath=null;player.Close();};start.ValueChanged+=(s,e)=>invalidate();duration.ValueChanged+=(s,e)=>invalidate();
  timer.Interval=250;timer.Tick+=(s,e)=>{if(busy)status.Text=stage+" / elapsed "+elapsed.Elapsed.ToString(@"m\:ss");};timer.Start();
  FormClosing+=(s,e)=>{if(busy){e.Cancel=true;Cancel();status.Text="Cancelling. Close again when the job stops.";}else{timer.Stop();player.Dispose();}};
 }
 Label LabelAt(string text,int x,int y,int w,int h,int size){var label=new Label{Text=text,Location=new Point(x,y),Size=new Size(w,h),Font=new Font("Segoe UI",size)};Controls.Add(label);return label;}
 Button ButtonAt(string text,int x,int y,int w,int h,Action action){var b=new Button{Text=text,Location=new Point(x,y),Size=new Size(w,h),BackColor=Color.FromArgb(45,48,65),ForeColor=Color.White,FlatStyle=FlatStyle.Flat};b.Click+=(s,e)=>{try{action();}catch(Exception ex){Error(ex);}};Controls.Add(b);return b;}
 void Number(NumericUpDown n,int x,int y,int w,int min,int max,int value){n.SetBounds(x,y,w,30);n.Minimum=min;n.Maximum=max;n.Value=value;Controls.Add(n);}
 void Error(Exception ex){status.Text=ex.Message;if(testing)testFailure=ex.ToString();else MessageBox.Show(this,ex.Message,"Audio-guided variation",MessageBoxButtons.OK,MessageBoxIcon.Information);}
 public async Task TestWorkflow(string interpreter){testing=true;python=interpreter;duration.Value=5;await Run(false);if(testFailure!=null)throw new Exception(testFailure);if(string.IsNullOrEmpty(prepared)||string.IsNullOrEmpty(variation)||store.Items.Count<2)throw new Exception("Reference UI did not save both takes.");using(var audition=new NativePlayer()){audition.Open(prepared);if(Math.Abs(audition.Length-5000)>100)throw new Exception("Reference comparison duration failed.");audition.Seek(1000);audition.Open(variation);if(Math.Abs(audition.Length-5000)>100)throw new Exception("Variation comparison duration failed.");audition.Seek(1000);if(Math.Abs(audition.Position-1000)>100)throw new Exception("Variation comparison seek failed.");}}
 void Choose(){if(busy)return;using(var d=new OpenFileDialog{Filter="Audio recordings|*.wav;*.mp3"})if(d.ShowDialog(this)==DialogResult.OK){source=d.FileName;sourceText.Text=Path.GetFileName(source);start.Value=0;prepared=null;variation=null;playingPath=null;player.Close();}}
 void Settings(){if(busy)return;using(var d=new OpenFileDialog{Title="Choose python.exe inside the working Stable Audio .venv / Scripts folder",Filter="Python executable|python.exe",FileName=python})if(d.ShowDialog(this)==DialogResult.OK){python=d.FileName;File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"gen-reference-python.txt"),python);status.Text="Stable Audio environment selected. Cached models will be reused.";}}
 void Audition(string path){if(busy)return;if(string.IsNullOrEmpty(path)||!File.Exists(path))throw new Exception("Preview a section or generate a variation first.");if(path==playingPath&&player.Playing){player.Pause();return;}int position=player.Position;if(path!=playingPath){player.Open(path);playingPath=path;player.Seek(Math.Min(position,Math.Max(0,player.Length-1)));}if(player.Position>=player.Length-50)player.Seek(0);player.Volume(750);player.Play();}
 void Export(){if(string.IsNullOrEmpty(variation))throw new Exception("Generate a variation first.");using(var d=new SaveFileDialog{Filter="WAV audio|*.wav",FileName=Path.GetFileNameWithoutExtension(source)+"-variation.wav"})if(d.ShowDialog(this)==DialogResult.OK){if(string.Equals(Path.GetFullPath(d.FileName),Path.GetFullPath(source),StringComparison.OrdinalIgnoreCase))throw new Exception("Choose a different filename to keep your source intact.");if(!string.Equals(Path.GetFullPath(variation),Path.GetFullPath(d.FileName),StringComparison.OrdinalIgnoreCase))File.Copy(variation,d.FileName,true);}}
 void Cancel(){cancelled=true;try{if(process!=null&&!process.HasExited)process.Kill();}catch{}}
 async Task Run(bool preview){
  if(busy){Cancel();return;}string job=Path.Combine(Path.GetTempPath(),"GEN-Reference-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(job);
  try{
   if(!File.Exists(source))throw new Exception("Choose an audio recording first.");
   string request=Path.Combine(job,"request.xml");ReferenceBridge.Request(request,source,(double)start.Value,(double)duration.Value,amount.Value/100.0,(int)count.Value,prompt.Text,preview,-1);
   var info=ReferenceBridge.StartInfo(python,request);player.Close();playingPath=null;prepared=null;if(!preview)variation=null;busy=true;cancelled=false;elapsed.Restart();stage=preview?"Preparing reference":"Starting audio-guided generation";generate.Text="Cancel job";
   foreach(Control control in Controls)control.Enabled=control==generate||control==status;
   var log=new StringBuilder();object gate=new object();DataReceivedEventHandler read=(s,e)=>{if(e.Data==null)return;lock(gate){if(log.Length>20000)log.Remove(0,10000);log.AppendLine(e.Data);}if(e.Data.StartsWith("GEN_STAGE\t"))try{BeginInvoke(new Action(()=>stage=e.Data.Substring(10)));}catch{}};
   process=new Process{StartInfo=info};process.OutputDataReceived+=read;process.ErrorDataReceived+=read;process.Start();process.BeginOutputReadLine();process.BeginErrorReadLine();
   bool done=await Task.Run(()=>process.WaitForExit(30*60*1000));if(!done){Cancel();throw new Exception("Generation timed out. Diagnostics: "+job);}process.WaitForExit();File.WriteAllText(Path.Combine(job,"process.log"),log.ToString());
   string manifest=Path.Combine(job,"results.xml");List<SavedTake> saved=new List<SavedTake>();if(File.Exists(manifest)){
    prepared=ReferenceBridge.Output(job,(string)XDocument.Load(manifest).Root.Attribute("reference"));
    if(!preview){saved=ReferenceBridge.SaveResults(store,job);if(saved.Count>1){prepared=Path.Combine(store.Root,saved[0].File);variation=Path.Combine(store.Root,saved[saved.Count-1].File);}}
   }
   if(cancelled){status.Text="Cancelled. Completed variations have been saved.";return;}
   if(process.ExitCode!=0){string failure=File.Exists(Path.Combine(job,"failure.txt"))?File.ReadAllText(Path.Combine(job,"failure.txt")):log.ToString();throw new Exception("Audio processing failed. Completed takes were saved.\n"+failure.Substring(Math.Max(0,failure.Length-1600))+"\nDiagnostics: "+job);}
   if(!preview&&saved.Count<2)throw new Exception("No generated variation was returned.");
   status.Text=preview?"Section ready. Click A / Original to listen.":"Saved "+(saved.Count-1)+" variation(s). A and B switch at the current playback position; B plays the last take.";
  }catch(Exception ex){Error(ex);}finally{if(process!=null){process.Dispose();process=null;}busy=false;elapsed.Stop();foreach(Control control in Controls)control.Enabled=true;generate.Text="Generate variation";}
 }
}
}
