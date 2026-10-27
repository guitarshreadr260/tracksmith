using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

namespace TracksmithGgdMapping {
public static class MidiRemapper {
 // Numeric MIDI notes; DAWs may label octaves differently.
 public static readonly Dictionary<int,int> Notes=new Dictionary<int,int>{
  {35,24},{36,24},{37,30},{38,26},{40,26},{41,36},{42,43},{43,35},{44,48},{45,35},
  {46,46},{47,34},{48,34},{49,52},{50,33},{51,62},{52,65},{53,61},{55,73},{57,54},{59,62}
 };
 static void Need(bool condition,string message){if(!condition)throw new InvalidDataException(message);}
 static int Big(byte[] data,int offset,int size){Need(offset>=0&&offset+size<=data.Length,"Truncated MIDI header.");long value=0;for(int i=0;i<size;i++)value=(value<<8)|data[offset+i];Need(value<=Int32.MaxValue,"Oversized MIDI chunk.");return (int)value;}
 static int Vlq(byte[] data,ref int offset,int end){int value=0;for(int i=0;i<4;i++){Need(offset<end,"Truncated MIDI variable length.");int b=data[offset++];value=(value<<7)|(b&127);if(b<128)return value;}throw new InvalidDataException("Invalid MIDI variable length.");}
 static bool Tag(byte[] bytes,int offset,string tag){return offset>=0&&offset+4<=bytes.Length&&Encoding.ASCII.GetString(bytes,offset,4)==tag;}
 public static byte[] Convert(byte[] source,int channel,out int changed,out int unsupported){
  Need(channel>=-1&&channel<=15,"Invalid MIDI channel.");Need(source!=null&&source.Length>=14&&Tag(source,0,"MThd"),"Choose a Standard MIDI File (.mid).");
  int header=Big(source,4,4);Need(header>=6&&header<=source.Length-8,"Invalid MIDI header size.");int format=Big(source,8,2),tracks=Big(source,10,2);Need(format<=1&&tracks>0,"Only MIDI formats 0 and 1 are supported.");
  byte[] result=(byte[])source.Clone();changed=unsupported=0;int cursor=8+header,recognized=0;bool native=false;
  for(int track=0;track<tracks;track++){
   Need(Tag(source,cursor,"MTrk"),"MIDI track is missing.");int size=Big(source,cursor+4,4);cursor+=8;Need(size<=source.Length-cursor,"Truncated MIDI track.");int end=cursor+size,running=0;
   while(cursor<end){Vlq(source,ref cursor,end);Need(cursor<end,"Truncated MIDI event.");int status=source[cursor];if(status>=128)cursor++;else{Need(running!=0,"Missing MIDI running status.");status=running;}
    if(status==0xff){Need(cursor<end,"Truncated meta event.");int type=source[cursor++],length=Vlq(source,ref cursor,end);Need(length<=end-cursor,"Truncated MIDI metadata.");if(type==3){string text=Encoding.UTF8.GetString(source,cursor,length);if(text.Contains("GGD One Kit Wonder Metal"))native=true;}cursor+=length;continue;}
    if(status==0xf0||status==0xf7){int length=Vlq(source,ref cursor,end);Need(length<=end-cursor,"Truncated SysEx.");cursor+=length;running=0;continue;}
    Need(status>=0x80&&status<=0xef,"Unsupported MIDI system event.");running=status;int kind=status&0xf0,count=kind==0xc0||kind==0xd0?1:2;Need(count<=end-cursor,"Truncated MIDI channel event.");for(int j=0;j<count;j++)Need(source[cursor+j]<128,"Invalid MIDI event data.");
    if((channel<0||(status&15)==channel)&&(kind==0x80||kind==0x90||kind==0xa0)){
     int pitch=source[cursor],mapped;if(pitch==24||pitch==26)native=true;
     if(Notes.TryGetValue(pitch,out mapped)){result[cursor]=(byte)mapped;if(mapped!=pitch)changed++;if(kind==0x90&&source[cursor+1]>0)recognized++;}
     else if(kind==0x90&&source[cursor+1]>0)unsupported++;
    }cursor+=count;
   }
  }
  Need(!native,"This MIDI already appears to use GGD mapping. Choose the original GEN / General MIDI export.");
  Need(recognized>0,"No General MIDI drum hits found on that channel. GEN exports use channel 10; select All channels only for a drum-only file.");return result;
 }
 public static string Save(string input,string output,int channel){
  if(Path.GetFullPath(input).Equals(Path.GetFullPath(output),StringComparison.OrdinalIgnoreCase))throw new IOException("Choose a different filename to keep the original.");
  if(File.Exists(output))throw new IOException("Output already exists. Choose a new filename.");
  int changed,unsupported;byte[] result=Convert(File.ReadAllBytes(input),channel,out changed,out unsupported);
  using(var stream=new FileStream(output,FileMode.CreateNew,FileAccess.Write))stream.Write(result,0,result.Length);
  return "Saved GGD Metal factory mapping. Changed "+changed+" note/pressure events; "+unsupported+" unsupported hits retained. Timing, tempo, channels and velocity bytes preserved.";
 }
}
public sealed class MapperForm:Form {
 readonly ComboBox channel=new ComboBox();readonly Label status=new Label();
 public MapperForm(){Text="Tracksmith GEN / GGD Metal MIDI mapping";ClientSize=new Size(710,360);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;StartPosition=FormStartPosition.CenterScreen;BackColor=Color.FromArgb(23,26,33);ForeColor=Color.White;Font=new Font("Segoe UI",10);
  var title=new Label{Text="Fix an existing GEN drum MIDI",Font=new Font("Segoe UI",18),Bounds=new Rectangle(24,20,660,40)};Controls.Add(title);
  Controls.Add(new Label{Text="General MIDI to One Kit Wonder: Metal - factory Invasion mapping.\r\nUse an original GEN export. Your source MIDI stays intact.",Bounds=new Rectangle(25,76,660,58)});
  Controls.Add(new Label{Text="Drum MIDI channel",Bounds=new Rectangle(25,146,210,25)});channel.DropDownStyle=ComboBoxStyle.DropDownList;channel.SetBounds(240,142,440,30);for(int i=1;i<=16;i++)channel.Items.Add("Channel "+i+(i==10?" / GEN default":""));channel.Items.Add("All channels / drum-only file");channel.SelectedIndex=9;Controls.Add(channel);
  var button=new Button{Text="Choose MIDI and save GGD copy",Bounds=new Rectangle(25,194,655,44)};button.Click+=delegate{Run();};Controls.Add(button);status.SetBounds(25,263,655,80);status.Text="Kick 36 -> 24 | Snare 38 -> 26 | Closed hat 42 -> 43\r\nCrash 49 -> 52 | Ride 51 -> 62 | Toms mapped individually";Controls.Add(status);
 }
 void Run(){try{using(var open=new OpenFileDialog{Filter="MIDI files|*.mid;*.midi"}){if(open.ShowDialog(this)!=DialogResult.OK)return;using(var save=new SaveFileDialog{Filter="MIDI files|*.mid",FileName=Path.GetFileNameWithoutExtension(open.FileName)+"-GGD-Metal.mid"}){if(save.ShowDialog(this)!=DialogResult.OK)return;status.Text=MidiRemapper.Save(open.FileName,save.FileName,channel.SelectedIndex==16?-1:channel.SelectedIndex);}}}catch(Exception e){status.Text=e.Message;MessageBox.Show(this,e.Message,"MIDI mapping",MessageBoxButtons.OK,MessageBoxIcon.Information);}}
}
public static class MapperProgram {
 [STAThread] public static int Main(string[] args){try{if(args.Length==2){Console.WriteLine(MidiRemapper.Save(args[0],args[1],9));return 0;}Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new MapperForm());return 0;}catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}}
}
}
