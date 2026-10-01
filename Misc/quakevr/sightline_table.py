# sightline_table.py -- the sight lines of the guns without painted sights (Quake/vr/vr_sightalign_table.inc).
# For each gun: a line on the model's middle (y 0), parallel to its barrel (the model's x), from above the fist (x of
# the middle of the fist round the grip, as drawn: vr_sight_check) to the muzzle (the muzzle point's x), just over the
# highest part of the mesh (frame 0) within half a unit of the middle between them.
# Run from the repository's root: python Misc/quakevr/sightline_table.py > Quake/vr/vr_sightalign_table.inc
import os, struct, sys
P=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..','quakevr','progs')+os.sep
def load(path):
    d=open(path,'rb').read()
    (ident,ver)=struct.unpack_from('<4si',d,0)
    scale=struct.unpack_from('<3f',d,8); tr=struct.unpack_from('<3f',d,20)
    rad=struct.unpack_from('<f',d,32)[0]; eye=struct.unpack_from('<3f',d,36)
    nskins,sw,sh,nv,nt,nf,st,flags,size=struct.unpack_from('<iiiiiiiif',d,48)
    o=84
    skins=[]
    for i in range(nskins):
        g=struct.unpack_from('<i',d,o)[0]; o+=4
        if g==0: skins.append(d[o:o+sw*sh]); o+=sw*sh
        else:
            n=struct.unpack_from('<i',d,o)[0]; o+=4+4*n
            for k in range(n): skins.append(d[o:o+sw*sh]); o+=sw*sh
    stv=[struct.unpack_from('<iii',d,o+12*i) for i in range(nv)]; o+=12*nv
    tris=[struct.unpack_from('<iiii',d,o+16*i) for i in range(nt)]; o+=16*nt
    frames=[]
    for f in range(nf):
        t=struct.unpack_from('<i',d,o)[0]; o+=4
        if t==0:
            o+=8+16
            v=[struct.unpack_from('<BBBB',d,o+4*i) for i in range(nv)]; o+=4*nv
            frames.append([(v[i][0]*scale[0]+tr[0],v[i][1]*scale[1]+tr[1],v[i][2]*scale[2]+tr[2]) for i in range(nv)])
        else:
            n=struct.unpack_from('<i',d,o)[0]; o+=4+8+4*n
            for k in range(n):
                o+=8+16
                v=[struct.unpack_from('<BBBB',d,o+4*i) for i in range(nv)]; o+=4*nv
                frames.append([(v[i][0]*scale[0]+tr[0],v[i][1]*scale[1]+tr[1],v[i][2]*scale[2]+tr[2]) for i in range(nv)])
    return dict(sw=sw,sh=sh,skins=skins,stv=stv,tris=tris,frames=frames,scale=scale,tr=tr)


# model: (fist x, muzzle x) from vr_sight_check (the weapon's own or its base's placement)
# The grapple's line ends before its claws (they open out above the body); the laser cannon's runs along its barrel,
# which hangs below the handle and the frame (from x 50, past them).
guns={'v_grpple':(8.09,20.0),'v_nail':(-2.05,20.21),'v_lava':(-2.05,20.21),'v_nail2':(2.46,40.08),'v_lava2':(2.46,40.08),
      'v_rock':(-1.32,30.80),'v_multi':(-1.32,30.80),'v_prox':(-1.58,30.79),'v_rock2':(5.70,56.11),'v_multi2':(5.70,56.11),
      'v_laserg':(50.0,105.17)}
# Guns with modelled sights (make_enemyguns.py prints them): the notch's bottom and the post's top, as they are.
# The grunts' burst gun's level; the enforcers' rifle's zeroed (its line meets the shots' 10 m out: ENF_ZERO_METRES).
sights={'v_gruntgun':((-3.5,14.35),(22.4,14.45)),'v_enfrifle':((6.30,10.660),(23.00,10.523))}
def top(m,x0,x1,band=0.5):
    V=m['frames'][0]; best=-1e9
    for ys in [i*band/4 for i in range(-4,5)]:
        for (ff,a,b,c) in m['tris']:
            P3=[V[a],V[b],V[c]]
            # the triangle cut by the plane y=ys: a segment; its highest point within x0..x1
            pts=[]
            for i in range(3):
                p,q=P3[i],P3[(i+1)%3]
                if (p[1]-ys)*(q[1]-ys)<=0 and p[1]!=q[1]:
                    t=(ys-p[1])/(q[1]-p[1]); pts.append((p[0]+t*(q[0]-p[0]),p[2]+t*(q[2]-p[2])))
                elif p[1]==ys: pts.append((p[0],p[2]))
            if len(pts)<2: continue
            (ax,az),(bx,bz)=pts[0],pts[1]
            for (x,z) in [(ax,az),(bx,bz)]:
                if x0<=x<=x1: best=max(best,z)
            # clipped ends
            if ax!=bx:
                for xe in (x0,x1):
                    t=(xe-ax)/(bx-ax)
                    if 0<=t<=1: best=max(best,az+t*(bz-az))
    return best
HEAD='''// vr_sightalign_table.inc -- the sight lines of the guns without painted sights (vr_sightalign.cpp): model, rear, front
// (model units, as the frames' vertices). A line on the model's middle, parallel to its barrel (its x), from above the
// middle of the fist to the muzzle, 0.05 over the highest part of the mesh between (Misc/quakevr/sightline_table.py); the
// grapple's ends before its claws, the laser cannon's runs along its barrel (below the handle and frame).'''
out=[]
for g,(xr,xf) in guns.items():
    m=load(P+g+'.mdl'); z=top(m,xr,xf)+0.05
    out.append('{"progs/%s.mdl", {%.2ff, 0.f, %.2ff}, {%.2ff, 0.f, %.2ff}},'%(g,xr,z,xf,z))
    print(g, 'top %.2f'%z, file=sys.stderr)
for g,((xr,zr),(xf,zf)) in sights.items():
    out.append('{"progs/%s.mdl", {%.2ff, 0.f, %.3ff}, {%.2ff, 0.f, %.3ff}},'%(g,xr,zr,xf,zf))
HEAD+='''
// The grunts' burst gun and the enforcers' rifle: their modelled sights (make_enemyguns.py), the rifle's zeroed at 10 m.'''
print(HEAD)
print('\n'.join(out))
