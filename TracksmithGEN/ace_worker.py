"""Local ACE-Step GPU worker for Tracksmith GEN. Run only in the selected ACE environment."""
import os,sys,time,json,traceback,shutil,xml.etree.ElementTree as ET
from pathlib import Path

def stage(text):print('GEN_STAGE\t'+text,flush=True)
def main():
 request=Path(sys.argv[1]).resolve();root=ET.parse(request).getroot()
 repo=Path(root.findtext('repo')).resolve();out=request.parent
 os.environ['HF_HUB_OFFLINE']='1';os.environ['TRANSFORMERS_OFFLINE']='1'
 os.environ['TQDM_DISABLE']='1';sys.path.insert(0,str(repo))
 import torch
 if not torch.cuda.is_available():raise RuntimeError('CUDA is unavailable in the selected ACE environment. CPU fallback is disabled.')
 stage('GPU: '+torch.cuda.get_device_name(0)+' / loading ACE-Step SFT')
 from loguru import logger
 logger.remove();logger.add(out/'worker.log',encoding='utf-8',level='INFO')
 from acestep.handler import AceStepHandler
 from acestep.llm_inference import LLMHandler
 from acestep.inference import GenerationParams,GenerationConfig,generate_music
 caption=root.findtext('caption','').strip();duration=int(root.findtext('duration','15'));count=int(root.findtext('count','1'))
 if not caption or len(caption)>512:raise ValueError('Keep the complete prompt between 1 and 512 characters.')
 if not 10<=duration<=120 or not 1<=count<=3:raise ValueError('Use 10-120 seconds and 1-3 takes.')
 bpm=int(root.findtext('bpm','0'));key=root.findtext('key','');seed=int(root.findtext('seed','-1'))
 profiles={'balanced':(50,7.0,'acestep-5Hz-lm-0.6B',True),'direct':(50,10.0,None,False),'planner':(50,7.0,'acestep-5Hz-lm-1.7B',True),'extended':(100,7.0,'acestep-5Hz-lm-1.7B',True)}
 profile=root.findtext('profile','balanced')
 if profile not in profiles:raise ValueError('Unknown generation approach.')
 steps,guidance,planner,thinking=profiles[profile]
 if planner and not (repo/'checkpoints'/planner).is_dir():raise RuntimeError('This planner is not cached. Choose Balanced or Direct, or install the planner separately.')
 stage('Approach: '+profile+' / '+str(steps)+' rendering steps')
 dit=AceStepHandler();status,ok=dit.initialize_service(project_root=str(repo),config_path='acestep-v15-sft',device='cuda',use_flash_attention=False,compile_model=False,offload_to_cpu=True,offload_dit_to_cpu=True,quantization=None)
 if not ok:raise RuntimeError(str(status))
 lm=None
 if planner:
  stage('Loading musical planning model: '+planner)
  lm=LLMHandler();status,ok=lm.initialize(checkpoint_dir=str(repo/'checkpoints'),lm_model_path=planner,backend='pt',device='cuda',offload_to_cpu=True,dtype=None)
  if not ok:raise RuntimeError(str(status))
 manifest=ET.Element('results',model='acestep-v15-sft',device='cuda',gpu=torch.cuda.get_device_name(0),profile=profile)
 for i in range(count):
  chosen_seed=seed+i if seed>=0 else int.from_bytes(os.urandom(4),'little')%2147483647
  stage('Creating take '+str(i+1)+' of '+str(count)+' on GPU')
  started=time.perf_counter();torch.cuda.reset_peak_memory_stats()
  params=GenerationParams(task_type='text2music',caption=caption,lyrics='[Instrumental]',instrumental=True,bpm=bpm or None,keyscale=key,timesignature='4',duration=duration,inference_steps=steps,guidance_scale=guidance,seed=chosen_seed,thinking=thinking,use_cot_caption=False,use_cot_metas=False,use_cot_language=False)
  config=GenerationConfig(batch_size=1,audio_format='wav',use_random_seed=False,seeds=[chosen_seed])
  result=generate_music(dit,lm,params=params,config=config,save_dir=str(out/'raw'))
  if not result.success or not result.audios:raise RuntimeError(str(result.error or result.status_message))
  audio=result.audios[0].get('path');destination=out/('take-'+str(i+1)+'.wav')
  if not audio or not Path(audio).is_file():raise RuntimeError('The GPU engine returned no WAV file.')
  shutil.copyfile(audio,destination)
  import soundfile as sf
  info=sf.info(destination)
  if info.frames<info.samplerate or abs(info.duration-duration)>1:raise RuntimeError('Generated WAV duration failed validation.')
  take=ET.SubElement(manifest,'take',file=destination.name,seed=str(chosen_seed),seconds=str(round(time.perf_counter()-started,2)),peak_vram_gb=str(round(torch.cuda.max_memory_allocated()/1024**3,2)))
  ET.SubElement(take,'caption').text=caption
  ET.ElementTree(manifest).write(out/'results.xml',encoding='utf-8',xml_declaration=True)
  stage('Take '+str(i+1)+' ready')
 stage('Generation complete')
if __name__=='__main__':
 try:main()
 except Exception as exc:
  print('GEN_ERROR\t'+str(exc),flush=True)
  try:Path(sys.argv[1]).resolve().parent.joinpath('failure.txt').write_text(traceback.format_exc(),encoding='utf-8')
  except Exception:pass
  sys.exit(1)
