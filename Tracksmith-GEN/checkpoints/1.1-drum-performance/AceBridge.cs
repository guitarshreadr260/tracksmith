using System;
using System.Diagnostics;
using System.IO;
using System.Xml.Linq;
namespace TracksmithGEN {
public static class AceBridge {
 public static string Python(string repo){return Path.Combine(repo,".venv","Scripts","python.exe");}
 public static bool Valid(string repo){return Directory.Exists(repo)&&File.Exists(Python(repo))&&File.Exists(Path.Combine(repo,"acestep","inference.py"));}
 public static string DefaultRoot(){return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),"Downloads","TracksmithGEN-ACE-Step-GPU-Test-Kit","ACE-Step-1.5");}
 public static void WriteRequest(string path,string repo,string caption,int duration,int count,int bpm,string key,int seed,string profile="balanced"){new XDocument(new XElement("request",new XElement("repo",repo),new XElement("caption",caption),new XElement("duration",duration),new XElement("count",count),new XElement("bpm",bpm),new XElement("key",key=="Any"?"":key),new XElement("seed",seed),new XElement("profile",profile))).Save(path);}
 public static ProcessStartInfo StartInfo(string repo,string request){
  if(!Valid(repo))throw new Exception("Choose the ACE-Step-1.5 folder that contains .venv and acestep using Engine settings.");
  string worker=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"ace_worker.py");if(!File.Exists(worker))throw new Exception("ace_worker.py must be beside TracksmithGEN.exe. Copy both update files into your GEN folder.");
  var info=new ProcessStartInfo(Python(repo),"-B -u "+AudioLab.Quote(worker)+" "+AudioLab.Quote(request)){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=repo};
  info.EnvironmentVariables["HF_HOME"]=Path.Combine(Directory.GetParent(repo).FullName,"model-cache");info.EnvironmentVariables["HF_HUB_OFFLINE"]="1";return info;
 }
 public static int Smoke(string profile="balanced"){string work=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"GPU-smoke-"+DateTime.Now.ToString("yyyyMMdd-HHmmss"));Directory.CreateDirectory(work);
  try{string repo=DefaultRoot(),request=Path.Combine(work,"request.xml");WriteRequest(request,repo,"Solo electric bass guitar playing a low funky plucked bassline. Bass only.",10,1,100,"D minor",31415,profile);
   using(var p=new Process{StartInfo=StartInfo(repo,request)}){string stdout="",stderr="";p.OutputDataReceived+=(s,e)=>{if(e.Data!=null)stdout+=e.Data+Environment.NewLine;};p.ErrorDataReceived+=(s,e)=>{if(e.Data!=null)stderr+=e.Data+Environment.NewLine;};p.Start();p.BeginOutputReadLine();p.BeginErrorReadLine();if(!p.WaitForExit(5*60*1000)){p.Kill();throw new Exception("GPU test timed out");}p.WaitForExit();File.WriteAllText(Path.Combine(work,"process.log"),stdout+stderr);if(p.ExitCode!=0)throw new Exception("Worker failed; see process.log");}
   var manifest=XDocument.Load(Path.Combine(work,"results.xml"));if((string)manifest.Root.Attribute("device")!="cuda")throw new Exception("GPU device missing");var take=manifest.Root.Element("take");if(take==null)throw new Exception("No generated take");string wav=Path.Combine(work,(string)take.Attribute("file"));var samples=Engine.ReadWav(wav);if(samples.Length<Engine.Rate*9)throw new Exception("Invalid WAV length");var store=new TakeStore(Path.Combine(work,"saved-library"));store.Add(wav,"GPU smoke bass","Solo bass","Prompt defined","Instrument part",100);if(new TakeStore(store.Root).Items.Count!=1)throw new Exception("Generated take did not persist");File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"gpu-smoke-result.txt"),"PASS: native C# bridge -> ACE Python GPU worker -> 10-second WAV -> saved library -> reopen. "+work);return 0;
  }catch(Exception ex){File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"gpu-smoke-result.txt"),"FAIL: "+ex+" / "+work);return 1;}
 }
}
}
