#!/usr/bin/env python3
"""Package an already-tested native engine plus complete corresponding source."""
import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary',required=True,type=Path)
parser.add_argument('--output',required=True,type=Path)
parser.add_argument('--version',default='1.0.0-frame.1')
args=parser.parse_args()
source=Path(__file__).resolve().parents[2]
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
sha=subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()
if subprocess.check_output(['git','status','--porcelain'],cwd=source):
    raise SystemExit('Commit the source before packaging so binary/source provenance is reviewable')
binary=args.binary.read_bytes()
if binary[:4]!=b'\x7fELF' or int.from_bytes(binary[18:20],'little')!=183:
    raise SystemExit('Expected an ARM64 ELF engine binary')
package=out/f'QuakeVR-Unleashed-SteamFrame-{args.version}.zip'
files={}
frame=source/'Misc/frame'
for name in ['install-frame.py','run-native.sh','steam-shortcut.py','steam-shortcut-LICENSE.txt',
             'frame-vr-state.py','openvr_capi.h','openvr-LICENSE.txt','RELEASE-NOTES.md']:
    files[name]=(frame/name).read_bytes().replace(b'\r\n',b'\n')
files['ironwail']=binary
files['LICENSE.txt']=(source/'LICENSE.txt').read_bytes()
files['docs/STEAM-FRAME-DEVELOPMENT.md']=(source/'docs/STEAM-FRAME-DEVELOPMENT.md').read_bytes()
files['docs/images/steam-frame-native-gameplay.png']=(source/'docs/images/steam-frame-native-gameplay.png').read_bytes()
for component,name in [('simde','COPYING'),('mimalloc','LICENSE'),('box3d','LICENSE'),
                       ('glm','COPYING.txt'),('fsr1','LICENSE.txt'),('nis','LICENSE.txt')]:
    files[f'licenses/{component}-{name}']=(source/'Quake/vr/external'/component/name).read_bytes()
manifest={'version':args.version,'source_commit':sha,'upstream':'vittorioromeo/quakevr v1.0.0',
          'architecture':'Linux aarch64','status':'experimental; performance issues remain',
          'files':{name:hashlib.sha256(data).hexdigest() for name,data in files.items()}}
files['build-manifest.json']=json.dumps(manifest,indent=2).encode()+b'\n'
with zipfile.ZipFile(package,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
    for name,data in files.items():
        entry=zipfile.ZipInfo(name);entry.create_system=3
        entry.external_attr=(0o100755 if name=='ironwail' or name.endswith(('.sh','.py')) else 0o100644)<<16
        archive.writestr(entry,data,compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
source_archive=out/f'QuakeVR-Unleashed-SteamFrame-{args.version}-source.zip'
subprocess.run(['git','archive','--format=zip','--output='+str(source_archive),sha],cwd=source,check=True)
patch=out/f'QuakeVR-Unleashed-SteamFrame-{args.version}.patch'
with patch.open('wb') as target:
    subprocess.run(['git','format-patch','--stdout','--binary','v1.0.0..'+sha],cwd=source,stdout=target,check=True)
with (out/'SHA256SUMS.txt').open('w',newline='\n') as sums:
    for artifact in [package,source_archive,patch]:
        digest=hashlib.file_digest(artifact.open('rb'),'sha256').hexdigest()
        sums.write(f'{digest}  {artifact.name}\n')
        print(artifact.name,artifact.stat().st_size,digest)
