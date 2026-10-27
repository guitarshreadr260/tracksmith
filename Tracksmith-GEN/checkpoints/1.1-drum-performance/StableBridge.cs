using System;
using System.Diagnostics;
using System.IO;
using System.Xml.Linq;

namespace TracksmithGEN {
public static class StableBridge {
 public static string Python(){string file=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"gen-reference-python.txt");return File.Exists(file)?File.ReadAllText(file).Trim():ReferenceBridge.DefaultPython();}
 public static void WriteRequest(string path,string prompt,int duration,int count,int seed){new XDocument(new XElement("request",new XElement("caption",prompt),new XElement("duration",duration),new XElement("count",count),new XElement("seed",seed))).Save(path);}
 public static ProcessStartInfo StartInfo(string python,string request){
  if(!File.Exists(python))throw new Exception("Choose the Stable Audio test environment's .venv / Scripts / python.exe using Engine settings.");
  string worker=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"stable_worker.py");if(!File.Exists(worker))throw new Exception("Keep stable_worker.py beside TracksmithGEN.exe. Run the complete 0.7.2 update.");
  var info=new ProcessStartInfo(python,"-B -u "+AudioLab.Quote(worker)+" "+AudioLab.Quote(request)){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=AppDomain.CurrentDomain.BaseDirectory};
  info.EnvironmentVariables["HF_HUB_OFFLINE"]="1";info.EnvironmentVariables["TRANSFORMERS_OFFLINE"]="1";return info;
 }
}
}
