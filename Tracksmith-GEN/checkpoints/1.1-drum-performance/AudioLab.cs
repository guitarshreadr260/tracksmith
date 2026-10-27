using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace TracksmithGEN {
public class AudioLab:Form {
 TextBox executable=new TextBox(),prompt=new TextBox(); NumericUpDown seconds=new NumericUpDown();
 Label status=new Label(); Button run=new Button(),cancel=new Button(); Process process; bool running;
 public string OutputPath {get;private set;} public string Instrument {get;private set;}
 public static string Quote(string value) {
  var b=new StringBuilder("\"");int slashes=0;
  foreach(char c in value){if(c=='\\'){slashes++;continue;}if(c=='"'){b.Append('\\',slashes*2+1);b.Append(c);}else{b.Append('\\',slashes);b.Append(c);}slashes=0;}
  b.Append('\\',slashes*2);b.Append('"');return b.ToString();
 }
 public AudioLab(string instrument,int tempo) {
  Instrument=instrument;Text="Tracksmith GEN / Experimental AI audio";ClientSize=new Size(740,510);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;StartPosition=FormStartPosition.CenterParent;
  BackColor=Color.FromArgb(22,27,36);ForeColor=Color.White;Font=new Font("Segoe UI",10);
  LabelAt("AI AUDIO LAB",22,18,690,30).Font=new Font("Segoe UI",18,FontStyle.Bold);
  LabelAt("Generate a new instrument idea from text. No recording is required.",22,60,690,30);
  LabelAt("Experimental: does not follow an imported chorus. Isolation, chords and tempo\nneed listening checks. No audio or MIDI-to-audio conditioning is used here.",22,91,690,50);
  LabelAt("Local stable-audio.exe (created by SETUP-AI.cmd)",22,153,650,25);
  executable.Text=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"ai-env","Scripts","stable-audio.exe");executable.SetBounds(22,181,565,28);Controls.Add(executable);
  var browse=new Button{Text="Browse",Location=new Point(600,179),Size=new Size(115,32)};browse.Click+=(s,e)=>{using(var d=new OpenFileDialog{Filter="Stable Audio CLI|stable-audio.exe"})if(d.ShowDialog()==DialogResult.OK)executable.Text=d.FileName;};Controls.Add(browse);
  LabelAt("Describe only the instrument you want",22,224,650,25);
  string name=instrument=="Bass"?"electric bass guitar":instrument=="Synth pad"?"synth pad":instrument=="Lead guitar guide"?"electric lead guitar":"piano";
  prompt.Text="Solo "+name+", melodic rock phrase, "+tempo+" BPM, no drums, no vocals, no other instruments.";prompt.Multiline=true;prompt.SetBounds(22,254,693,75);Controls.Add(prompt);
  LabelAt("Duration (seconds)",22,348,180,26);seconds.Minimum=5;seconds.Maximum=30;seconds.Value=10;seconds.SetBounds(210,348,90,28);Controls.Add(seconds);
  run.Text="Generate AI audio";run.SetBounds(365,343,190,38);run.Click+=async(s,e)=>await Generate();Controls.Add(run);
  cancel.Text="Close";cancel.SetBounds(570,343,145,38);cancel.Click+=(s,e)=>{if(running)CancelProcess();else Close();};Controls.Add(cancel);
  status.Text="Engine not installed? Close this window and run SETUP-AI.cmd first.\nThe first model download requires accepted Hugging Face access terms.";status.SetBounds(22,406,693,85);Controls.Add(status);
  FormClosing+=(s,e)=>{if(running){e.Cancel=true;CancelProcess();}};
 }
 Label LabelAt(string t,int x,int y,int w,int h){var l=new Label{Text=t,Location=new Point(x,y),Size=new Size(w,h)};Controls.Add(l);return l;}
 void CancelProcess(){try{if(process!=null&&!process.HasExited)process.Kill();}catch{}status.Text="Cancelling engine...";}
 async Task Generate() {
  if(running)return;
  string output=Path.Combine(Path.GetTempPath(),"TracksmithGEN-AI-"+Guid.NewGuid()+".wav");
  try {
   string path=Path.GetFullPath(executable.Text);if(!File.Exists(path))throw new Exception("The local AI engine is not installed at this path. Run SETUP-AI.cmd, then reopen the lab.");
   string description=prompt.Text.Trim();if(description.Length<3||description.Length>2000)throw new Exception("Enter a description between 3 and 2000 characters.");
   int duration=(int)seconds.Value;
   var info=new ProcessStartInfo(path,"--model small-music -p "+Quote(description)+" --duration "+duration+" -o "+Quote(output)){UseShellExecute=false,CreateNoWindow=true,RedirectStandardError=true,RedirectStandardOutput=true,WorkingDirectory=Path.GetDirectoryName(path)};
   info.EnvironmentVariables["HF_HOME"]=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"ai-models");
   var log=new StringBuilder();object gate=new object();DataReceivedEventHandler collect=(s,e)=>{if(e.Data!=null)lock(gate){if(log.Length>16000)log.Remove(0,8000);log.AppendLine(e.Data);}};
   process=new Process{StartInfo=info};process.OutputDataReceived+=collect;process.ErrorDataReceived+=collect;
   running=true;run.Enabled=false;cancel.Text="Cancel";status.Text="Generating with Stable Audio 3 Small. First run downloads model files.\nThis can take several minutes; the window remains responsive.";
   process.Start();process.BeginOutputReadLine();process.BeginErrorReadLine();
   bool exited=await Task.Run(()=>process.WaitForExit(30*60*1000));
   if(!exited){CancelProcess();throw new Exception("Engine timed out after 30 minutes. Check the model download and retry.");}
   process.WaitForExit();if(process.ExitCode!=0){string message;lock(gate)message=log.ToString();if(message.Length>2500)message=message.Substring(message.Length-2500);throw new Exception("AI engine did not complete. Check access/login and dependencies.\n\n"+message);}
   // Reject missing, empty or unsupported output. Never substitute sketch audio.
   if(!File.Exists(output))throw new Exception("The engine returned no WAV output.");
   float[] decoded=Engine.ReadWav(output);if(decoded.Length<Engine.Rate)throw new Exception("The engine returned an empty or very short file.");
   OutputPath=output;running=false;DialogResult=DialogResult.OK;Close();
  } catch(Exception ex) {status.Text=ex.Message;MessageBox.Show(this,ex.Message,"AI generation not completed",MessageBoxButtons.OK,MessageBoxIcon.Information);try{if(File.Exists(output))File.Delete(output);}catch{}}
  finally {running=false;run.Enabled=true;cancel.Text="Close";if(process!=null){process.Dispose();process=null;}}
 }
}
}
