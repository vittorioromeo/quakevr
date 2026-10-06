# minimal numpy stand-in (no installs): 3-vectors as V, a few helpers
import math, bisect
class V(tuple):
    def __new__(cls, it): return tuple.__new__(cls, [float(x) for x in it])
    def __add__(a, b): return V(x + y for x, y in zip(a, b))
    def __radd__(a, b): return a if b == 0 else a + b
    def __sub__(a, b): return V(x - y for x, y in zip(a, b))
    def __mul__(a, k): return V(x * k for x in a)
    __rmul__ = __mul__
    def __truediv__(a, k): return V(x / k for x in a)
    def __neg__(a): return V(-x for x in a)
    def __getitem__(a, i):
        r = tuple.__getitem__(a, i)
        return V(r) if isinstance(i, slice) else r
def array(x): return V(x)
def zeros(n): return V([0.0] * n)
def cross(a, b): return V([a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]])
def dot(a, b): return sum(x*y for x, y in zip(a, b))
class linalg:
    @staticmethod
    def norm(a): return math.sqrt(sum(x*x for x in a))
def sum_(vs, axis=0):
    out = zeros(3)
    for v in vs: out = out + v
    return out
def mean(vs, axis=0): vs = list(vs); return sum_(vs) / len(vs)
def searchsorted(arr, t, side='right'): return bisect.bisect_right(arr, t)
def polyfit(xs, ys, deg, w=None):  # quadratic least squares (weights w)
    w = w or [1.0]*len(xs)
    S = [sum(wi * x**k for x, wi in zip(xs, w)) for k in range(5)]; T = [sum(wi * y * x**k for x, y, wi in zip(xs, ys, w)) for k in range(3)]
    M = [[S[0], S[1], S[2]], [S[1], S[2], S[3]], [S[2], S[3], S[4]]]
    def det(m): return m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1]) - m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0]) + m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0])
    d = det(M); c = []
    for col in range(3):
        mc = [[T[r] if k == col else M[r][k] for k in range(3)] for r in range(3)]; c.append(det(mc) / d)
    return c[2], c[1], c[0]
