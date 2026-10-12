from pathlib import Path
import sys,math,re,subprocess,numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Misc/quakevr'))
import make_vrbody as body
# Actual fitted wrist/lip contours: every generated vertex must be distinct; compare predecessor's snapped 12-sided contour.
for muscle in (.9,1.2,1.5):
 for side in ('l','r'):
  _,ref=body.bare_rings(muscle,side)
  for past in (0.,body.HAND_LIP):
   shape=body.hand_ring(side,past,*ref,sides=144)
   pts=[shape(2*math.pi*k/144) for k in range(144)]
   assert len(set(pts))==144,(muscle,side,past)
   radial=[]
   for k in range(144):
    a,b=np.array(pts[k]),np.array(pts[(k+1)%144])
    for t in np.linspace(0,1,9):
     p=a+(b-a)*t; n=np.linalg.norm(p); d=p/n
     skin=body.hand_slice(body.HAND_WRIST[0]+past,-d[0],(-1 if side=='r' else 1)*d[1])*body.HAND_UNIT
     radial.append((n-skin)*1000)
   print(f'geometry build {muscle} {side} past {past}: 144 unique vertices, gap {min(radial):.3f}..{max(radial):.3f} mm')
   assert min(radial)>.12
   assert max(radial)<1.0
# Validate masks against actual authored skins: changed static blood pixels, nails and clothes cannot get tone.
for n in range(16):
 img=np.array(Image.open(ROOT/f'quakevr/progs/vrbody_{n:02d}_00.tga').convert('RGB'))
 clean=np.array(Image.open(ROOT/f'quakevr/progs/vrbody_{n//4*4:02d}_00.tga').convert('RGB'))
 mask=np.array(Image.open(ROOT/f'quakevr/progs/vrbody_{n:02d}_00_skinmask.tga').convert('L'))
 assert not mask[np.any(img!=clean,axis=2)].any()
 h,w=mask.shape;assert not mask[int(math.ceil(h*.4)):].any();assert not mask[:,int(math.ceil(w*.5)):].any()
assert not subprocess.check_output(['git','-C',str(ROOT),'diff','--name-only','--','quakevr/progs/vrbody_*_00.tga','quakevr/progs/hand_rig_*.lmp'],text=True).strip()
print('PASS all 16 authored diffuse paints unchanged; static blood and clothing masks zero')
