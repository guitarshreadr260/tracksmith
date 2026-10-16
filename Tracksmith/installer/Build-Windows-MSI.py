from pathlib import Path
import ctypes,uuid,json,hashlib,subprocess,shutil,argparse
from ctypes import wintypes
root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description='Build the Tracksmith per-user x64 Windows MSI using Windows Installer and Makecab.')
parser.add_argument('--payload',type=Path,default=root/'outputs/Tracksmith-Windows-GGD-MIDI-Fix/Tracksmith')
parser.add_argument('--work',type=Path,default=root/'work/installer')
parser.add_argument('--output',type=Path,default=root/'outputs/Tracksmith-Setup-0.1.0-x64.msi')
args=parser.parse_args();payload=args.payload.resolve();stage=args.work.resolve();output=args.output.resolve();stage.mkdir(parents=True,exist_ok=True);output.parent.mkdir(parents=True,exist_ok=True)
if output.exists():raise RuntimeError('Installer output exists; preserve it before rebuilding')
msi=ctypes.WinDLL('msi');UINT=ctypes.c_uint;HANDLE=UINT
def bind(name,args,restype=UINT):
 f=getattr(msi,name);f.argtypes=args;f.restype=restype;return f
open_db=bind('MsiOpenDatabaseW',[wintypes.LPCWSTR,ctypes.c_void_p,ctypes.POINTER(HANDLE)])
open_view=bind('MsiDatabaseOpenViewW',[HANDLE,wintypes.LPCWSTR,ctypes.POINTER(HANDLE)])
execute=bind('MsiViewExecute',[HANDLE,HANDLE]);close=bind('MsiCloseHandle',[HANDLE]);create_record=bind('MsiCreateRecord',[UINT],HANDLE)
set_string=bind('MsiRecordSetStringW',[HANDLE,UINT,wintypes.LPCWSTR]);set_int=bind('MsiRecordSetInteger',[HANDLE,UINT,ctypes.c_int]);set_stream=bind('MsiRecordSetStreamW',[HANDLE,UINT,wintypes.LPCWSTR]);commit=bind('MsiDatabaseCommit',[HANDLE])
get_summary=bind('MsiGetSummaryInformationW',[HANDLE,wintypes.LPCWSTR,UINT,ctypes.POINTER(HANDLE)]);set_summary=bind('MsiSummaryInfoSetPropertyW',[HANDLE,UINT,UINT,ctypes.c_int,ctypes.c_void_p,wintypes.LPCWSTR]);persist_summary=bind('MsiSummaryInfoPersist',[HANDLE])
def ok(code,what):
 if code:raise RuntimeError(f'{what}: Windows Installer error {code}')
db=HANDLE();ok(open_db(str(output),ctypes.c_void_p(3),ctypes.byref(db)),'create MSI')
def sql(query,values=None,streams=()):
 view=HANDLE();ok(open_view(db,query,ctypes.byref(view)),query)
 record=0
 try:
  if values is not None:
   record=create_record(len(values))
   for i,v in enumerate(values,1):
    if v is None:continue
    ok(set_stream(record,i,str(v)) if i in streams else set_int(record,i,v) if isinstance(v,int) else set_string(record,i,str(v)),f'set field {i}')
  ok(execute(view,record),query)
 finally:
  if record:close(record)
  close(view)
def table(name,columns,keys):sql(f'CREATE TABLE `{name}` ('+', '.join(f'`{n}` {t}' for n,t in columns)+ ' PRIMARY KEY '+', '.join(f'`{k}`' for k in keys)+')')
schemas={
'Property':([('Property','CHAR(72) NOT NULL'),('Value','CHAR(0) LOCALIZABLE')],['Property']),
'Directory':([('Directory','CHAR(72) NOT NULL'),('Directory_Parent','CHAR(72)'),('DefaultDir','CHAR(255) NOT NULL LOCALIZABLE')],['Directory']),
'Component':([('Component','CHAR(72) NOT NULL'),('ComponentId','CHAR(38)'),('Directory_','CHAR(72) NOT NULL'),('Attributes','SHORT NOT NULL'),('Condition','CHAR(255)'),('KeyPath','CHAR(72)')],['Component']),
'File':([('File','CHAR(72) NOT NULL'),('Component_','CHAR(72) NOT NULL'),('FileName','CHAR(255) NOT NULL LOCALIZABLE'),('FileSize','LONG NOT NULL'),('Version','CHAR(72)'),('Language','CHAR(20)'),('Attributes','SHORT'),('Sequence','LONG NOT NULL')],['File']),
'Feature':([('Feature','CHAR(38) NOT NULL'),('Feature_Parent','CHAR(38)'),('Title','CHAR(64) LOCALIZABLE'),('Description','CHAR(255) LOCALIZABLE'),('Display','SHORT'),('Level','SHORT NOT NULL'),('Directory_','CHAR(72)'),('Attributes','SHORT NOT NULL')],['Feature']),
'FeatureComponents':([('Feature_','CHAR(38) NOT NULL'),('Component_','CHAR(72) NOT NULL')],['Feature_','Component_']),
'Media':([('DiskId','SHORT NOT NULL'),('LastSequence','LONG NOT NULL'),('DiskPrompt','CHAR(64) LOCALIZABLE'),('Cabinet','CHAR(255)'),('VolumeLabel','CHAR(32)'),('Source','CHAR(72)')],['DiskId']),
'Registry':([('Registry','CHAR(72) NOT NULL'),('Root','SHORT NOT NULL'),('Key','CHAR(255) NOT NULL LOCALIZABLE'),('Name','CHAR(255) LOCALIZABLE'),('Value','CHAR(0) LOCALIZABLE'),('Component_','CHAR(72) NOT NULL')],['Registry']),
'Shortcut':([('Shortcut','CHAR(72) NOT NULL'),('Directory_','CHAR(72) NOT NULL'),('Name','CHAR(128) NOT NULL LOCALIZABLE'),('Component_','CHAR(72) NOT NULL'),('Target','CHAR(72) NOT NULL'),('Arguments','CHAR(255)'),('Description','CHAR(255) LOCALIZABLE'),('Hotkey','SHORT'),('Icon_','CHAR(72)'),('IconIndex','SHORT'),('ShowCmd','SHORT'),('WkDir','CHAR(72)')],['Shortcut']),
'RemoveFile':([('FileKey','CHAR(72) NOT NULL'),('Component_','CHAR(72) NOT NULL'),('FileName','CHAR(255) LOCALIZABLE'),('DirProperty','CHAR(72) NOT NULL'),('InstallMode','SHORT NOT NULL')],['FileKey']),
'LaunchCondition':([('Condition','CHAR(255) NOT NULL'),('Description','CHAR(255) NOT NULL LOCALIZABLE')],['Condition']),
'AppSearch':([('Property','CHAR(72) NOT NULL'),('Signature_','CHAR(72) NOT NULL')],['Property','Signature_']),
'RegLocator':([('Signature_','CHAR(72) NOT NULL'),('Root','SHORT NOT NULL'),('Key','CHAR(255) NOT NULL'),('Name','CHAR(255)'),('Type','SHORT')],['Signature_']),
'InstallExecuteSequence':([('Action','CHAR(72) NOT NULL'),('Condition','CHAR(255)'),('Sequence','SHORT')],['Action']),
'InstallUISequence':([('Action','CHAR(72) NOT NULL'),('Condition','CHAR(255)'),('Sequence','SHORT')],['Action']),
'Dialog':([('Dialog','CHAR(72) NOT NULL'),('HCentering','SHORT NOT NULL'),('VCentering','SHORT NOT NULL'),('Width','SHORT NOT NULL'),('Height','SHORT NOT NULL'),('Attributes','LONG'),('Title','CHAR(128) LOCALIZABLE'),('Control_First','CHAR(50) NOT NULL'),('Control_Default','CHAR(50)'),('Control_Cancel','CHAR(50)')],['Dialog']),
'Control':([('Dialog_','CHAR(72) NOT NULL'),('Control','CHAR(50) NOT NULL'),('Type','CHAR(20) NOT NULL'),('X','SHORT NOT NULL'),('Y','SHORT NOT NULL'),('Width','SHORT NOT NULL'),('Height','SHORT NOT NULL'),('Attributes','LONG'),('Property','CHAR(50)'),('Text','CHAR(0) LOCALIZABLE'),('Control_Next','CHAR(50)'),('Help','CHAR(50) LOCALIZABLE')],['Dialog_','Control']),
'ControlEvent':([('Dialog_','CHAR(72) NOT NULL'),('Control_','CHAR(50) NOT NULL'),('Event','CHAR(50) NOT NULL'),('Argument','CHAR(255) NOT NULL'),('Condition','CHAR(255)'),('Ordering','SHORT')],['Dialog_','Control_','Event','Argument','Condition']),
'TextStyle':([('TextStyle','CHAR(72) NOT NULL'),('FaceName','CHAR(32) NOT NULL'),('Size','SHORT NOT NULL'),('Color','LONG'),('StyleBits','SHORT')],['TextStyle'])}
schemas['Signature']=([('Signature','CHAR(72) NOT NULL'),('FileName','CHAR(255) NOT NULL'),('MinVersion','CHAR(20)'),('MaxVersion','CHAR(20)'),('MinSize','LONG'),('MaxSize','LONG'),('MinDate','LONG'),('MaxDate','LONG'),('Languages','CHAR(255)')],['Signature'])
schemas['CompLocator']=([('Signature_','CHAR(72) NOT NULL'),('ComponentId','CHAR(38) NOT NULL'),('Type','SHORT')],['Signature_'])
schemas['DrLocator']=([('Signature_','CHAR(72) NOT NULL'),('Parent','CHAR(72)'),('Path','CHAR(255)'),('Depth','SHORT')],['Signature_','Parent','Path'])
schemas['IniLocator']=([('Signature_','CHAR(72) NOT NULL'),('FileName','CHAR(255) NOT NULL'),('Section','CHAR(96) NOT NULL'),('Key','CHAR(128) NOT NULL'),('Field','SHORT'),('Type','SHORT')],['Signature_'])
for name,(cols,keys) in schemas.items():table(name,cols,keys)
def add(name,*values):sql(f'INSERT INTO `{name}` ('+', '.join(f'`{c[0]}`' for c in schemas[name][0])+') VALUES ('+', '.join('?' for _ in values)+')',values)
product='{1C452D66-9118-44DE-8C0D-349DB726A1E6}';upgrade='{6D80AF5C-2696-4C19-922C-5E3F8FD93021}'
properties={'ProductCode':product,'UpgradeCode':upgrade,'ProductName':'Tracksmith','ProductVersion':'0.1.0','ProductLanguage':'1033','Manufacturer':'Tracksmith','INSTALLLEVEL':'1','ARPNOMODIFY':'1','ARPURLINFOABOUT':'https://github.com/guitarshreadr260/tracksmith','ARPCOMMENTS':'GGD MIDI Fix installer preview; session format 9.','MSIRESTARTMANAGERCONTROL':'Disable','DefaultUIFont':'Normal','SecureCustomProperties':'INSTALLDIR','DiskPrompt':'Tracksmith installation'}
for k,v in properties.items():add('Property',k,v)
directories={'TARGETDIR':(None,'SourceDir'),'LocalAppDataFolder':('TARGETDIR','.'),'ProgramsDir':('LocalAppDataFolder','Programs'),'INSTALLDIR':('ProgramsDir','Tracksmith'),'ProgramMenuFolder':('TARGETDIR','.'),'TracksmithMenu':('ProgramMenuFolder','Tracksmith')}
paths={Path('.'):'INSTALLDIR'}
files=sorted(p for p in payload.rglob('*') if p.is_file() and ('Stock Plugins' in p.relative_to(payload).parts or p.parent==payload) and 'AVRX Drums' not in str(p))
for p in files:
 for parent in reversed(p.relative_to(payload).parents):
  if parent in paths:continue
  key=f'D{len(paths):04}';paths[parent]=key;directories[key]=(paths[parent.parent],f'D{len(paths):06}|{parent.name}')
for k,(parent,name) in directories.items():add('Directory',k,parent,name)
add('Feature','Core',None,'Tracksmith','Application, stock NAM plugin, guides and Start menu shortcut',1,1,'INSTALLDIR',0)
manifest=[];ddf=['.OPTION EXPLICIT','.Set Cabinet=on','.Set Compress=on','.Set CompressionType=MSZIP','.Set CabinetNameTemplate=payload.cab',f'.Set DiskDirectoryTemplate={stage}','.Set MaxDiskSize=0']
appcomponent=None;appfile=None
for i,p in enumerate(files,1):
 rel=p.relative_to(payload);fid=f'F{i:04}';comp=f'C{i:04}';reg=f'R{i:04}';guid='{'+str(uuid.uuid5(uuid.UUID(upgrade.strip('{}')),rel.as_posix().lower())).upper()+'}'
 add('Component',comp,guid,paths[rel.parent],260,None,reg);add('FeatureComponents','Core',comp)
 version='0.1.0.0' if rel.as_posix()=='Tracksmith.exe' else None
 add('File',fid,comp,f'F{i:07}|{p.name}',p.stat().st_size,version,None,16384,i)
 add('Registry',reg,1,r'Software\Tracksmith\Installer\Components',fid,f'[#{fid}]',comp)
 ddf.append(f'"{p}" {fid}')
 manifest.append({'id':fid,'path':rel.as_posix(),'size':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
 if rel.as_posix()=='Tracksmith.exe':appcomponent=comp;appfile=fid
assert appcomponent and len(files)>20
add('Shortcut','StartMenu','TracksmithMenu','Tracksmith',appcomponent,f'[#{appfile}]',None,'Tracksmith DAW',None,None,None,1,'INSTALLDIR')
add('RemoveFile','RemoveMenuFolder',appcomponent,None,'TracksmithMenu',2)
add('Media',1,len(files),None,'#payload.cab',None,None)
webkey=r'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'
for prop,rootkey,key,typename in [('WEBVIEWUSER',1,webkey,2),('WEBVIEWMACHINE',2,webkey,2),('WEBVIEW64',2,webkey,18),('VCX64',2,r'Software\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',18),('VCX64WOW',2,r'Software\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',2)]:
 add('AppSearch',prop,prop);add('RegLocator',prop,rootkey,key,'pv' if prop.startswith('WEBVIEW') else 'Installed',typename)
add('LaunchCondition','Installed OR VersionNT64','Tracksmith requires 64-bit Windows. Use Windows 10 or Windows 11.')
add('LaunchCondition','Installed OR (WEBVIEWUSER AND WEBVIEWUSER <> "0.0.0.0") OR (WEBVIEWMACHINE AND WEBVIEWMACHINE <> "0.0.0.0") OR (WEBVIEW64 AND WEBVIEW64 <> "0.0.0.0")','Install Microsoft Edge WebView2 Runtime first: https://developer.microsoft.com/microsoft-edge/webview2/')
add('LaunchCondition','Installed OR VCX64 = "#1" OR VCX64WOW = "#1"','Install the latest Microsoft Visual C++ v14 Redistributable (x64) first: https://aka.ms/vc14/vc_redist.x64.exe')
execute_sequence=[('AppSearch',50),('LaunchConditions',100),('ValidateProductID',700),('CostInitialize',800),('FileCost',900),('CostFinalize',1000),('InstallValidate',1400),('InstallInitialize',1500),('ProcessComponents',1600),('UnpublishFeatures',1800),('RemoveRegistryValues',2600),('RemoveShortcuts',3200),('RemoveFiles',3500),('RemoveFolders',3600),('CreateFolders',3700),('InstallFiles',4000),('CreateShortcuts',4500),('WriteRegistryValues',5000),('RegisterUser',6000),('RegisterProduct',6100),('PublishFeatures',6300),('PublishProduct',6400),('InstallFinalize',6600)]
for action,sequence in execute_sequence:add('InstallExecuteSequence',action,None,sequence)
for action,condition,sequence in [('AppSearch',None,50),('LaunchConditions',None,100),('CostInitialize',None,800),('FileCost',None,900),('CostFinalize',None,1000),('SetupDlg','NOT Installed',1200),('ExecuteAction',None,1300),('ExitDlg','NOT Installed',-1)]:add('InstallUISequence',action,condition,sequence)
add('TextStyle','Normal','Segoe UI',9,0,0);add('TextStyle','Heading','Segoe UI',14,0,1)
for dialog,heading,body,button in [('SetupDlg','Install Tracksmith','Install the working GGD MIDI Fix build for your Windows account.\n\nTracksmith and the stock NAM plugin will be added to your local Programs folder, with a Start menu shortcut.\n\nYour sessions, recordings, plugins and saved settings are preserved.\n\nClose an older Tracksmith before using the installed copy.','Install'),('ExitDlg','Tracksmith is installed','Open Tracksmith from the Start menu.\n\nTo remove it later, use Windows Settings > Apps > Tracksmith > Uninstall. Sessions, recordings and saved settings stay intact.','Finish')]:
 add('Dialog',dialog,50,50,370,230,3,'Tracksmith Setup','Primary','Primary','Cancel' if dialog=='SetupDlg' else 'Primary')
 add('Control',dialog,'Title','Text',20,15,330,25,3,None,'{\\Heading}'+heading,None,None)
 add('Control',dialog,'Body','Text',20,55,330,130,3,None,body,None,None)
 add('Control',dialog,'Primary','PushButton',265,200,80,18,3,None,button,'Cancel' if dialog=='SetupDlg' else 'Primary',None)
 add('ControlEvent',dialog,'Primary','EndDialog','Return','1',1)
 if dialog=='SetupDlg':add('Control',dialog,'Cancel','PushButton',175,200,80,18,3,None,'Cancel','Primary',None);add('ControlEvent',dialog,'Cancel','EndDialog','Exit','1',1)
ddfpath=stage/'payload.ddf';ddfpath.write_text('\n'.join(ddf)+'\n',encoding='utf-8')
result=subprocess.run(['makecab.exe','/F',str(ddfpath)],cwd=stage,capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW);(stage/'makecab.log').write_text(result.stdout+result.stderr,encoding='utf-8');assert result.returncode==0,result.stdout+result.stderr
sql('INSERT INTO `_Streams` (`Name`,`Data`) VALUES (?,?)',['payload.cab',stage/'payload.cab'],streams=(2,))
summary=HANDLE();ok(get_summary(db,None,20,ctypes.byref(summary)),'summary')
for prop,value in {2:'Tracksmith installation database',3:'Tracksmith Windows x64 installer preview',4:'Tracksmith',5:'Installer;MSI;Tracksmith',7:'x64;1033',9:'{'+str(uuid.uuid4()).upper()+'}',14:500,15:10,18:'Tracksmith native MSI builder'}.items():ok(set_summary(summary,prop,3 if isinstance(value,int) else 30,value if isinstance(value,int) else 0,None,None if isinstance(value,int) else value),'set summary')
ok(persist_summary(summary),'persist summary');close(summary);ok(commit(db),'commit MSI');close(db)
(stage/'manifest.json').write_text(json.dumps({'productCode':product,'upgradeCode':upgrade,'msi':str(output),'files':manifest},indent=2),encoding='utf-8')
print('Created',output,'files=',len(files),'size=',output.stat().st_size)
