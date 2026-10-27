"""CUDA adapter for Sound Razor's lead/rhythm inference.

The original overlap/add remains on CPU; each model inference runs on CUDA.
"""
import sys
import torch
from sound_razor.vendor.roformer import mel_band_roformer as module

if not torch.cuda.is_available():
    raise SystemExit('CUDA is unavailable in the advanced environment. Run SETUP-ADVANCED-GPU.cmd.')
print('Guitar inference device: ' + torch.cuda.get_device_name(0), file=sys.stderr, flush=True)
original=module.MelBandRoformer
def gpu_model(*args,**kwargs):
    model=original(*args,**kwargs).to('cuda')
    model.register_forward_pre_hook(lambda mod,inputs: tuple(x.to('cuda') if isinstance(x,torch.Tensor) else x for x in inputs))
    model.register_forward_hook(lambda mod,inputs,output: output.to('cpu') if isinstance(output,torch.Tensor) else output)
    return model
module.MelBandRoformer=gpu_model
from sound_razor.cli import main
raise SystemExit(main())

