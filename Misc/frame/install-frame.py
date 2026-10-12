#!/usr/bin/env python3
"""Install the experimental Frame engine separately; never modify owned originals."""
import argparse
import importlib.util
import json
import platform
import shutil
import zipfile
from pathlib import Path, PurePosixPath

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mod-archive',required=True,type=Path)
parser.add_argument('--quake-data',required=True,type=Path)
parser.add_argument('--install-dir',type=Path,default=Path.home()/'Games/QuakeVR-Unleashed-native-release/game')
parser.add_argument('--check',action='store_true',help='Validate inputs without installing')
parser.add_argument('--register',action='store_true',help='Register a VR shortcut using the running Steam client')
parser.add_argument('--shortcut-name',default='Quake VR: Unleashed (Steam Frame)')
args=parser.parse_args()
kit=Path(__file__).resolve().parent
destination=args.install_dir.expanduser().resolve()
if platform.system()!='Linux' or platform.machine() not in ('aarch64','arm64'):
    raise SystemExit('This binary package requires Linux ARM64 on Steam Frame')
if destination.exists():raise SystemExit('Use a fresh destination; existing games/saves are never overwritten')
if not (kit/'ironwail').is_file():raise SystemExit('Native ironwail binary is missing from this package')
paks={}
for p in args.quake_data.iterdir():
    if p.is_file() and p.name.lower() in ('pak0.pak','pak1.pak'):paks[p.name.lower()]=p
if set(paks)!= {'pak0.pak','pak1.pak'}:raise SystemExit('Owned id1 must contain PAK0.PAK and PAK1.PAK')
for p in paks.values():
    with p.open('rb') as f:
        if f.read(4)!=b'PACK':raise SystemExit(f'Invalid Quake PAK: {p.name}')
with zipfile.ZipFile(args.mod_archive) as archive:
    names=set(archive.namelist())
    if 'quakevr/progs.dat' not in names or 'ironwail.pak' not in names:
        raise SystemExit('Expected official Unleashed v1.0.0 QuakeVR.zip layout')
    if 'manifest.json' not in names:
        raise SystemExit('Official release manifest is missing')
    manifest=json.loads(archive.read('manifest.json'))
    if manifest.get('product')!='Quake VR' or not manifest.get('version','').startswith('1.0.0 '):
        raise SystemExit('This engine package requires the official v1.0.0 mod assets')
    selected=[entry for entry in archive.infolist() if entry.filename.startswith('quakevr/') or entry.filename=='ironwail.pak']
    for entry in selected:
        path=PurePosixPath(entry.filename)
        if path.is_absolute() or '..' in path.parts or '\\' in entry.filename:
            raise SystemExit('Archive contains an unsafe path')
        if ((entry.external_attr>>16)&0o170000)==0o120000:
            raise SystemExit('Archive contains a symbolic link')
    print(json.dumps({'destination':str(destination),'mod_entries':len(selected),'checked':True}))
    if args.check:raise SystemExit(0)
    destination.mkdir(parents=True)
    for entry in selected:
        target=destination.joinpath(*PurePosixPath(entry.filename).parts)
        if entry.is_dir():target.mkdir(parents=True,exist_ok=True)
        else:
            target.parent.mkdir(parents=True,exist_ok=True)
            with archive.open(entry) as source,target.open('wb') as out:shutil.copyfileobj(source,out)
(destination/'id1').mkdir()
for name,p in paks.items():shutil.copy2(p,destination/'id1'/name)
for name in ('ironwail','run-native.sh'):shutil.copy2(kit/name,destination/name)
for name in ('ironwail','run-native.sh'):(destination/name).chmod(0o755)
if args.register:
    spec=importlib.util.spec_from_file_location('shortcut',kit/'steam-shortcut.py')
    shortcut=importlib.util.module_from_spec(spec);spec.loader.exec_module(shortcut)
    idfile=destination/'steam-appid.txt'
    shortcut.ensure(args.shortcut_name,str(destination/'run-native.sh'),str(destination),'',str(idfile))
    appid=int(idfile.read_text())
    shortcut.evaluate(f'SteamClient.Apps.SetShortcutIsVR({appid}, true)')
    print('Registered VR shortcut:',appid)
print('Installed:',destination)
