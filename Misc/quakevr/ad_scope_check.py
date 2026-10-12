from pathlib import Path
import sys
import re
r=Path(sys.argv[1])
for mode in ['vanilla','ad']:
 t=(r/f'scratch/admenu_{mode}.log').read_text()
 phases=t.split('admenu:')[1:]
 assert len(phases)==3,(mode,len(phases))
 for n,phase in enumerate(phases):
  rows=re.findall(r'admenu-ammo: kind (\d) big (\d) model (\S+) count (\d+) rigid (\d)',phase)
  assert len(rows)==8,(mode,n,len(rows))
  assert len({(k,b) for k,b,m,c,v in rows})==8
  assert all(v=='1' for k,b,m,c,v in rows),(mode,n,rows)
  want_ad=(n==2 or (mode=='ad' and n==1))
  assert all(m.startswith('progs/ammo_')==want_ad for k,b,m,c,v in rows),(mode,n,rows)
  assert all(int(c)==[[20,40],[25,50],[5,10],[6,12]][int(k)][int(b)] for k,b,m,c,v in rows)
 assert 'menu paths: 5 found, 0 missing' in t
 assert 'exit=0' in t
 assert not re.search(r'(?m)^(Host_Error|SV_Error|ENGINE ERROR|Fatal error|CRASH)',t)
 print(mode+': 3 scopes, 24/24 correct models/counts and rigid boxes; calibration 0 missing')
for mode in ['vanilla','ad']:
 t=(r/f'scratch/admenu_{mode}.log').read_text()
 assert 'model progs/v_shot.mdl -> progs/v_shot.mdl' in t
 assert 'model progs/v_shot2.mdl -> progs/v_shot2.mdl' in t
print('Held SG/SSG models preserved in both campaigns')
