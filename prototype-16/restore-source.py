from pathlib import Path
import hashlib,json,zipfile
root=Path(__file__).resolve().parent
manifest=json.loads((root/'source-manifest.json').read_text())
archive=root/manifest['archive']
if archive.stat().st_size!=manifest['bytes'] or hashlib.sha256(archive.read_bytes()).hexdigest()!=manifest['sha256']: raise RuntimeError('Archive checksum failed')
with zipfile.ZipFile(archive) as z:
 if z.testzip() is not None: raise RuntimeError('ZIP integrity failed')
print('Verified source archive:',archive)
print('Extract it, then follow BUILD.md. Runtime assets are listed separately in external-assets.json.')
