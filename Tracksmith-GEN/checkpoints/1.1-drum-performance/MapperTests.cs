using System;
using System.IO;
using System.Linq;
using System.Text;
using TracksmithGgdMapping;
public static class MapperTests {
 static void Check(bool ok,string why){if(!ok)throw new Exception(why);}
 static byte[] FileBytes(byte[] events){byte[] prefix={77,84,104,100,0,0,0,6,0,0,0,1,1,224,77,84,114,107,0,0,0,0};int n=events.Length;prefix[18]=(byte)(n>>24);prefix[19]=(byte)(n>>16);prefix[20]=(byte)(n>>8);prefix[21]=(byte)n;return prefix.Concat(events).ToArray();}
 public static int Main(){try{Run();return 0;}catch(Exception e){Console.Error.WriteLine(e);return 1;}}
 static void Run(){
  byte[] track={0,255,81,3,6,138,27,0,0x99,36,100,120,36,0,0,0x99,38,90,120,0x89,38,0,0,0xb9,4,80,0,0x90,60,100,0,0xa9,36,50,0,0xf0,3,1,2,0xf7,0,255,47,0};
  byte[] original=FileBytes(track),expected=FileBytes(new byte[]{0,255,81,3,6,138,27,0,0x99,24,100,120,24,0,0,0x99,26,90,120,0x89,26,0,0,0xb9,4,80,0,0x90,60,100,0,0xa9,24,50,0,0xf0,3,1,2,0xf7,0,255,47,0});
  int changed,unknown;byte[] converted=MidiRemapper.Convert(original,9,out changed,out unknown);
  Check(converted.SequenceEqual(expected),"Only intended on/off/poly-pressure pitch bytes may change; CC, melodic channel, SysEx and tempo must survive");Check(changed==5&&unknown==0,"Mapped event count");
  Check(original.SequenceEqual(FileBytes(track)),"Input mutated");bool rejected=false;try{MidiRemapper.Convert(converted,9,out changed,out unknown);}catch(InvalidDataException){rejected=true;}Check(rejected,"Double remap not rejected");
  rejected=false;try{MidiRemapper.Convert(original.Take(original.Length-2).ToArray(),9,out changed,out unknown);}catch(InvalidDataException){rejected=true;}Check(rejected,"Truncated file accepted");
  string folder=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"mapper-test-data");Directory.CreateDirectory(folder);string source=Path.Combine(folder,"source.mid"),output=Path.Combine(folder,Guid.NewGuid()+".mid");File.WriteAllBytes(source,original);MidiRemapper.Save(source,output,9);Check(File.ReadAllBytes(source).SequenceEqual(original),"Source changed on disk");rejected=false;try{MidiRemapper.Save(source,output,9);}catch(IOException){rejected=true;}Check(rejected,"Existing output overwritten");
  Console.WriteLine("PASS: running-status note on/zero-velocity off, explicit note-off, poly-pressure, melodic-channel preservation, tempo/CC/SysEx bytes unchanged, double-remap/truncation/overwrite rejection and source preservation");
 }
}
