# headfit.py -- a monster's head zone (QC weapons.qc PositionalHead: xHeadForward, xHeadUp, xHeadRadius) from its stand
# frame: the vertices above `zmin` (forward of `xmin`, within `ymax` of its middle line), their middle and the radius
# holding 80% of them. Needs numpy.
#   python headfit.py ogre 26                         Quake VR's (quakevr/progs/ogre.mdl)
#   RIG_PAK=<...>/mg3/pak0.pak python headfit.py ogre_rocket 28 2 9    a pack's (its progs/<name>.mdl)
import os, sys, struct
import numpy as np
import mdl

name = sys.argv[1]
zmin = float(sys.argv[2])
xmin = float(sys.argv[3]) if len(sys.argv) > 3 else -1e9
ymax = float(sys.argv[4]) if len(sys.argv) > 4 else 1e9
path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../quakevr/progs/%s.mdl' % name)
if not os.path.exists(path) and os.environ.get('RIG_PAK'):
    pak = open(os.environ['RIG_PAK'], 'rb').read()
    _, off, ln = struct.unpack_from('<4sii', pak, 0)
    for i in range(ln // 64):
        fn, o, l = struct.unpack_from('<56sii', pak, off + i * 64)
        if fn.split(b'\0')[0].decode() == 'progs/%s.mdl' % name:
            path = pak[o:o + l]
names, P, T = mdl.load(path)
V = P[0]
sel = V[(V[:, 2] > zmin) & (V[:, 0] > xmin) & (np.abs(V[:, 1]) < ymax)]
c = sel.mean(0)
r = np.percentile(np.linalg.norm(sel - c, axis=1), 80)
print('%s: %d vertices, middle %.1f %.1f %.1f, radius (80%%) %.1f; the model: top %.1f, front %.1f'
      % (name, len(sel), c[0], c[1], c[2], r, V[:, 2].max(), V[:, 0].max()))
