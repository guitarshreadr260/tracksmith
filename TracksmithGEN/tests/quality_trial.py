"""Compare cached ACE configurations; record sound-quality verdicts only after listening."""
import os,sys,json,time,shutil,gc
from pathlib import Path
os.environ.update(HF_HUB_OFFLINE='1',TRANSFORMERS_OFFLINE='1',TQDM_DISABLE='1')
repo=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True);sys.path.insert(0,str(repo))
import torch,numpy as np,soundfile as sf
from loguru import logger
logger.remove();logger.add(out/'engine.log',encoding='utf-8',level='INFO')
from acestep.handler import AceStepHandler
from acestep.llm_inference import LLMHandler
from acestep.inference import GenerationParams,GenerationConfig,generate_music
assert torch.cuda.is_available(),'GPU required'
profiles=[('A_current',50,7.0,'acestep-5Hz-lm-0.6B',True),('B_direct',50,10.0,None,False),('C_larger_planner',50,7.0,'acestep-5Hz-lm-1.7B',True),('D_more_steps',100,7.0,'acestep-5Hz-lm-1.7B',True)]
cases=[('lead_guitar','Solo distorted electric lead guitar in D minor. Expressive sustained bends, wide vibrato and short melodic runs. A memorable repeating lead melody. Guitar alone, no backing band.'),('metalcore','Instrumental metalcore in D minor at 140 BPM. Heavy palm-muted distorted guitars, tight electric bass, punchy drums with double-kick patterns, and an emotional melodic lead guitar hook.')]
records=[];dit=AceStepHandler();msg,ok=dit.initialize_service(project_root=str(repo),config_path='acestep-v15-sft',device='cuda',offload_to_cpu=True,offload_dit_to_cpu=True,quantization=None,use_flash_attention=False,compile_model=False)
if not ok:raise RuntimeError(str(msg))
lm=None;loaded=None
for name,steps,guidance,lm_name,thinking in profiles:
 print(json.dumps({'stage':'profile','name':name,'steps':steps,'guidance':guidance,'planner':lm_name}),flush=True)
 if lm_name!=loaded:
  lm=None;gc.collect();torch.cuda.empty_cache()
  if lm_name:
   lm=LLMHandler();msg,ok=lm.initialize(checkpoint_dir=str(repo/'checkpoints'),lm_model_path=lm_name,backend='pt',device='cuda',offload_to_cpu=True,dtype=None)
   if not ok:raise RuntimeError(str(msg))
  loaded=lm_name
 for ident,caption in cases:
  case=name+'_'+ident;started=time.perf_counter();print(json.dumps({'stage':'generating','case':case}),flush=True)
  try:
   torch.cuda.reset_peak_memory_stats()
   params=GenerationParams(task_type='text2music',caption=caption,lyrics='[Instrumental]',instrumental=True,bpm=140 if ident=='metalcore' else 100,keyscale='D minor',timesignature='4',duration=20,inference_steps=steps,guidance_scale=guidance,seed=31415,thinking=thinking,use_cot_caption=False,use_cot_metas=False,use_cot_language=False)
   config=GenerationConfig(batch_size=1,audio_format='wav',use_random_seed=False,seeds=[31415])
   result=generate_music(dit,lm,params=params,config=config,save_dir=str(out/'raw'))
   if not result.success or not result.audios:raise RuntimeError(str(result.error or result.status_message))
   dest=out/(case+'.wav');shutil.copyfile(result.audios[0]['path'],dest);audio,rate=sf.read(dest,dtype='float32',always_2d=True)
   row={'case':case,'profile':name,'request':caption,'steps':steps,'guidance':guidance,'planner':lm_name,'thinking':thinking,'seed':31415,'seconds':round(time.perf_counter()-started,2),'peak_cuda_gb':round(torch.cuda.max_memory_allocated()/1024**3,2),'duration':len(audio)/rate,'sample_rate':rate,'channels':audio.shape[1],'rms':float(np.sqrt(np.mean(audio**2))),'nonfinite':int(np.sum(~np.isfinite(audio))),'listening_verdict':'UNRATED'}
  except Exception as exc:row={'case':case,'profile':name,'error':str(exc)}
  records.append(row);(out/'results.json').write_text(json.dumps({'model':'acestep-v15-sft','gpu':torch.cuda.get_device_name(0),'results':records},indent=2),encoding='utf-8');print(json.dumps(row),flush=True)
print(json.dumps({'stage':'complete','succeeded':sum('error' not in x for x in records),'total':len(records)}),flush=True)
