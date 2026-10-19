import json
import sys
import torch

info = {'torch': torch.__version__, 'cuda_build': torch.version.cuda,
        'cuda_available': torch.cuda.is_available()}
if not torch.cuda.is_available():
    print(json.dumps(info, indent=2))
    sys.exit('GPU check failed: this environment cannot use CUDA. CPU fallback is not accepted for this test.')
info['gpu'] = torch.cuda.get_device_name(0)
info['vram_gb'] = round(torch.cuda.get_device_properties(0).total_memory / 1024**3, 2)
with torch.inference_mode():
    a = torch.randn((512, 512), device='cuda')
    result = a @ a.T
    torch.cuda.synchronize()
    assert result.is_cuda and torch.isfinite(result).all().item()
info['gpu_computation'] = 'PASS'
print(json.dumps(info, indent=2))
