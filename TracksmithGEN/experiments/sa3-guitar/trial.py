"""Experimental Stable Audio 3 screening trial. No subjective winner is assigned."""
import html
import importlib.metadata
import json
import platform
import subprocess
import sys
import time
import traceback
import webbrowser
from datetime import datetime, timezone
from pathlib import Path

REVISION = "77e0398c26f7d22d801ec1bb036307e8b231f741"
CASES = [
    ("lead", "Solo distorted electric lead guitar in D minor. Expressive sustained bends, wide vibrato and short melodic runs. A memorable repeating lead melody. Guitar alone, no backing band."),
    ("clean", "One clean electric guitar alone playing a repeating melodic riff in D minor at 100 BPM. Clear picked notes and natural string decay. Dry close recording, no drums, bass, vocals or other instruments."),
    ("palm_muted", "One distorted electric rhythm guitar alone playing a tight palm-muted riff in D minor at 100 BPM. Distinct pick attacks, short chugs and deliberate rests. No drums, bass, vocals or other instruments."),
]
SEEDS = [31415, 27182, 16180]
PAGE_JS = """
const key='gen-sa3-scores:'+document.body.dataset.run;
const fields=[...document.querySelectorAll('select,textarea')];
let saved={};try{saved=JSON.parse(localStorage.getItem(key)||'{}')}catch(e){}
for(const f of fields){
  if(saved[f.id]!==undefined)f.value=saved[f.id];
  f.addEventListener('change',()=>{const s=Object.fromEntries(fields.map(x=>[x.id,x.value]));
    try{localStorage.setItem(key,JSON.stringify(s))}catch(e){}
  });
}
document.getElementById('export').onclick=()=>{
  const scores=Object.fromEntries(fields.map(x=>[x.id,x.value]));
  const a=document.createElement('a');
  const url=URL.createObjectURL(new Blob([JSON.stringify({run:document.body.dataset.run,scores},null,2)],{type:'application/json'}));
  a.href=url;a.download='listening-scores.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
};
"""

def write_report(out, report):
    (out / "results.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    blocks = []
    for i, row in enumerate(report["results"]):
        heading = html.escape(row["case"]) + " / seed " + str(row["seed"])
        if "error" in row:
            blocks.append("<article><h2>" + heading + "</h2><pre>" + html.escape(row["error"]) + "</pre></article>")
            continue
        fields = []
        for metric in ("Guitar realism", "Clear notes", "Isolation", "Musical usefulness"):
            ident = "r" + str(i) + "-" + metric.replace(" ", "-")
            options = '<option value="">Unrated</option>' + "".join(
                '<option value="' + str(n) + '">' + str(n) + "</option>" for n in range(1, 6))
            fields.append('<label>' + metric + ' <select id="' + ident + '">' + options + "</select></label>")
        blocks.append("<article><h2>" + heading + "</h2><p>" + html.escape(row["prompt"]) +
            '</p><audio controls preload="none" src="' + row["audition_file"] +
            '"></audio><p><a href="' + row["file"] + '">Original stereo WAV</a> | Render: ' +
            str(row["render_seconds"]) + " s</p>" + "".join(fields) +
            '<p><textarea id="notes-' + str(i) + '" placeholder="Notes: artifacts, unwanted band, timing, useful phrases"></textarea></p></article>')
    page = """<!doctype html><html lang="en"><meta charset="utf-8">
<title>GEN guitar model trial</title><style>
body{max-width:900px;margin:32px auto;padding:20px;background:#13181f;color:#e5edf5;font:16px system-ui}
article{padding:20px;margin:18px 0;background:#202936;border-radius:12px}
audio{width:100%}label{display:inline-block;margin:8px}select,button,textarea{font:inherit;padding:8px}
textarea{width:90%;height:65px}a{color:#7dcfff}pre{white-space:pre-wrap}
</style><body data-run=\"""" + html.escape(out.name, quote=True) + """\">
<h1>GEN: guitar model trial</h1><p>Stable Audio 3 Small Music. Experimental; listen before choosing.
Scores: 1 poor, 5 excellent. Blank means unrated. Audition copies are peak-scaled, not loudness-matched.
Scores may persist in this browser; export before closing.</p>
<button id="export">Export scores</button>""" + "".join(blocks) + "<script>" + PAGE_JS + "</script></body></html>"
    (out / "LISTEN.html").write_text(page, encoding="utf-8")

def main():
    import numpy as np
    import soundfile as sf
    import torch
    from stable_audio_3 import StableAudioModel
    if not torch.cuda.is_available():
        raise RuntimeError("CUDA is unavailable. This trial does not use CPU fallback.")
    out = Path(__file__).resolve().parent / "runs" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S-%fZ")
    out.mkdir(parents=True)
    versions = {}
    for name in ("torch", "torchaudio", "stable-audio-3", "transformers", "huggingface-hub", "numpy", "soundfile"):
        versions[name] = importlib.metadata.version(name)
    frozen = subprocess.check_output([sys.executable, "-m", "pip", "freeze"], text=True)
    (out / "environment.txt").write_text(frozen, encoding="utf-8")
    report = {"model": "small-music", "upstream_code_revision": REVISION,
              "weight_revision": "upstream default, not pinned", "python": platform.python_version(),
              "gpu": torch.cuda.get_device_name(0), "versions": versions, "duration": 20,
              "steps": 8, "cfg_scale": 1.0, "bpm_request": 100, "key_request": "D minor",
              "results": [], "listening_verdict": "UNRATED"}
    write_report(out, report)
    print("Loading Small Music. First use downloads model assets; accept its Hugging Face conditions first.", flush=True)
    started = time.perf_counter()
    try:
        model = StableAudioModel.from_pretrained("small-music", device="cuda", model_half=True)
        torch.cuda.synchronize()
    except Exception:
        (out / "failure.txt").write_text(traceback.format_exc(), encoding="utf-8")
        raise
    report["load_seconds"] = round(time.perf_counter() - started, 2)
    aborted = False
    for case, text in CASES:
        for seed in SEEDS:
            # Match the legacy lead request, with tempo made explicit in text.
            prompt = text + (" Tempo 100 BPM." if case == "lead" else "")
            row = {"case": case, "seed": seed, "prompt": prompt, "listening_verdict": "UNRATED"}
            print("Generating " + case + " / " + str(seed), flush=True)
            try:
                torch.cuda.reset_peak_memory_stats()
                started = time.perf_counter()
                audio = model.generate(prompt=prompt, duration=20, seed=seed, steps=8,
                    cfg_scale=1.0, batch_size=1, sample_size=model.model_config["sample_size"],
                    chunked_decode=True)
                torch.cuda.synchronize()
                row["render_seconds"] = round(time.perf_counter() - started, 2)
                row["peak_cuda_allocated_gb"] = round(torch.cuda.max_memory_allocated() / 1024**3, 3)
                data = audio[0].detach().float().cpu().numpy().T
                del audio
                rate = int(model.model.sample_rate)
                if data.ndim != 2 or data.shape[1] != 2 or not np.isfinite(data).all():
                    raise ValueError("Output is not finite stereo audio.")
                if abs(len(data) / rate - 20) > 0.1:
                    raise ValueError("Unexpected output duration.")
                peak = float(np.max(np.abs(data)))
                rms = float(np.sqrt(np.mean(data.astype(np.float64)**2)))
                if peak < 1e-6 or rms < 1e-7:
                    raise ValueError("Output is effectively silent.")
                row.update(sample_rate=rate, channels=2, duration=len(data)/rate, peak=peak,
                           rms=rms, near_full_scale_fraction=float(np.mean(np.abs(data) >= 0.999)))
                stem = case + "_" + str(seed)
                row["file"] = stem + ".wav"
                row["audition_file"] = stem + "_audition.wav"
                sf.write(out / row["file"], data, rate, subtype="PCM_24")
                sf.write(out / row["audition_file"], data * (0.85 / peak), rate, subtype="PCM_24")
            except Exception as exc:
                row["error"] = str(exc)
                (out / (case + "_" + str(seed) + "_error.txt")).write_text(traceback.format_exc(), encoding="utf-8")
                print("Failed: " + str(exc), flush=True)
                if isinstance(exc, torch.cuda.OutOfMemoryError):
                    aborted = True
            report["results"].append(row)
            write_report(out, report)
            if aborted:
                break
        if aborted:
            break
    report["aborted_on_oom"] = aborted
    write_report(out, report)
    print("Listening page: " + str(out / "LISTEN.html"), flush=True)
    webbrowser.open((out / "LISTEN.html").as_uri())
    good = sum("error" not in r for r in report["results"])
    print(str(good) + "/9 clips succeeded. Sound quality remains for local listening.")
    return 0 if good == 9 else 1

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception:
        traceback.print_exc()
        sys.exit(1)
