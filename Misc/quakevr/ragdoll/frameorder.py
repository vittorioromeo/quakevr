# frameorder.py -- whether a pack's monster model has Quake VR's model's frame order (a seed table copied, or rigged
# from the same death frames): each frame's height (z extent over the stand frame's), both models side by side, 20 a row.
# Death animations end low; a model in another order shows its low runs elsewhere. Needs numpy.
#   RIG_PAK=<...>/mg3/pak0.pak python frameorder.py ogre ogre_rocket    (Quake VR's quakevr/progs/ogre.mdl, the pack's
#                                                                        progs/ogre_rocket.mdl)
import os, sys, struct
import numpy as np
import mdl

here = os.path.dirname(os.path.abspath(__file__))
names_a, A, _ = mdl.load(os.path.join(here, '../../../quakevr/progs/%s.mdl' % sys.argv[1]))
pak = open(os.environ['RIG_PAK'], 'rb').read()
_, off, ln = struct.unpack_from('<4sii', pak, 0)
data = None
for i in range(ln // 64):
    fn, o, l = struct.unpack_from('<56sii', pak, off + i * 64)
    if fn.split(b'\0')[0].decode() == 'progs/%s.mdl' % sys.argv[2]:
        data = pak[o:o + l]
names_b, B, _ = mdl.load(data)


def heights(P):
    z = P[:, :, 2]
    h = z.max(1) - z.min(1)
    return h / h[0]


HA, HB = heights(A), heights(B)
print('frames: %s %d, %s %d' % (sys.argv[1], len(HA), sys.argv[2], len(HB)))
for lo in range(0, max(len(HA), len(HB)), 20):
    print('%3d %-12s' % (lo, sys.argv[1][:12]), ' '.join('%.2f' % x for x in HA[lo:lo + 20]))
    print('%3d %-12s' % (lo, sys.argv[2][:12]), ' '.join('%.2f' % x for x in HB[lo:lo + 20]))
