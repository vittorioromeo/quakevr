"""Generate skin masks without changing authored diffuse textures. Excludes nails, clothing and static blood."""
from pathlib import Path
import numpy as np
from PIL import Image
import sys,struct
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import make_hand_rig as hand
PROGS=HERE.parent.parent/'quakevr/progs'
models,shifts,rig,mesh,islands=hand.build(str(PROGS))
layout,size=hand.layout_islands(islands)
pos,norm,part=hand.rasterize(mesh,layout,size)
skin=part>=0
nails=np.zeros_like(skin)
for i,name in enumerate(hand.FINGERS):
 m=part==i+1
 seg,along,side,_=hand.finger_coords(rig,name,pos[m],norm[m])
 pts=rig.fingers[name]['points']
 length=np.linalg.norm(pts[3]-pts[2]); a=(along-.40*length)/(.62*length)
 nails[m]=(seg==2)&(a>0)&(a<1.05)&(side>.5)
def grow(m):
 p=np.pad(m,1); return np.logical_or.reduce([p[y:y+m.shape[0],x:x+m.shape[1]] for y in range(3) for x in range(3)])
skin=grow(grow(skin))&~grow(nails)
def lmp(path):
 data=path.read_bytes(); w,h=struct.unpack('<ii',data[:8]); return np.frombuffer(data[8:],np.uint8).reshape(h,w)
clean=lmp(PROGS/'hand_rig_00_00.lmp')
for n in range(4):
 same=lmp(PROGS/f'hand_rig_{n:02d}_00.lmp')==clean
 mask=skin&~grow(~same)
 Image.fromarray((mask*255).astype(np.uint8)).convert('RGB').save(PROGS/f'hand_rig_{n:02d}_00_skinmask.tga',compression='tga_rle')
 print(f'hand {n}: {mask.sum()} skin texels; nails excluded {nails.sum()}')
for n in range(16):
 img=np.array(Image.open(PROGS/f'vrbody_{n:02d}_00.tga').convert('RGB'))
 base=np.array(Image.open(PROGS/f'vrbody_{n//4*4:02d}_00.tga').convert('RGB'))
 h,w=img.shape[:2]; yy,xx=np.mgrid[:h,:w]
 mask=(xx<w*.5)&(yy<h*.4)&~grow(np.any(img!=base,axis=2))
 Image.fromarray((mask*255).astype(np.uint8)).convert('RGB').save(PROGS/f'vrbody_{n:02d}_00_skinmask.tga',compression='tga_rle')
 assert not mask[yy>=h*.4].any()
 print(f'body {n}: {mask.sum()} skin texels, clothing/static blood excluded')
