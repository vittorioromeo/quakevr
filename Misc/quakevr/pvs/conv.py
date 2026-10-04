# Prove which leaf a PVS bit names: for a face in leaf A, step 2 units through the face into the
# neighbouring leaf B. qbsp must mark A in B's PVS. Test bit A (0-based) vs bit A-1 (1-based).
import sys
sys.argv = ["bspvis", "start.bsp", "none"]
import bspvis

b = bspvis.BSP("start.bsp")
N = b.numleafs
vis = {}
def visset(l):
    if l not in vis:
        vis[l] = set(b.bits(b.decompress(l)))
    return vis[l]

pairs = []
for A in range(1, N):
    if b.leafs[A][1] < 0:
        continue
    lo, hi = b.leafbox(A)
    C = [(lo[i] + hi[i]) * 0.5 for i in range(3)]
    for f in b.leaffaces(A)[:4]:
        pl = b.planes[b.faces[f][0]]
        nx, ny, nz, dist = pl[0], pl[1], pl[2], pl[3]
        if b.faces[f][1]:
            nx, ny, nz, dist = -nx, -ny, -nz, -dist
        t = nx * C[0] + ny * C[1] + nz * C[2] - dist
        if t < 0:                      # make sure the leaf centre is on the + side of the face
            nx, ny, nz, dist, t = -nx, -ny, -nz, -dist, -t
        if t <= 0:
            continue
        s = min(2.0, t * 0.5)
        p = (C[0] - (t + s) * nx, C[1] - (t + s) * ny, C[2] - (t + s) * nz)
        B = b.pointleaf(p)
        if B is None or B <= 0 or B == A:
            continue
        pairs.append((A, B))

ok0 = ok1 = 0
for A, B in pairs:
    s = visset(B)
    if A in s:
        ok0 += 1
    if A - 1 in s:
        ok1 += 1
print("adjacent leaf pairs tested:", len(pairs))
print("bit = leaf index      (0-based): neighbour marked", ok0)
print("bit = leaf index - 1  (1-based): neighbour marked", ok1)
