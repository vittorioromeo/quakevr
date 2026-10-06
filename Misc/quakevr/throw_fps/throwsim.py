# Offline replica (ROUND21.md, "Throws at any frame rate") of the throw estimate (vr_throw.cpp releasePeak) on throw_plays.py's motions, sampled at any frame
# rate: old (sample-based) vs new (time-based) estimators. Tracking space: x right, y up, -z forward.
import math, sys, random
import vec as np
sys.path.insert(0, __import__("os").path.join(__import__("os").path.dirname(__import__("os").path.abspath(__file__)), ".."))
import throw_plays as tp

def quat_x(deg):
    a = math.radians(deg) / 2
    return np.array([math.cos(a), math.sin(a), 0, 0])
def qmul(a, b):
    w1, x1, y1, z1 = a; w2, x2, y2, z2 = b
    return np.array([w1*w2-x1*x2-y1*y2-z1*z2, w1*x2+x1*w2+y1*z2-z1*y2, w1*y2-x1*z2+y1*w2+z1*x2, w1*z2+x1*y2-y1*x2+z1*w2])
def qrot(q, v):
    w, x, y, z = q; u = np.array([x, y, z]); v = np.array(v)
    return v + np.cross(u, np.cross(u, v) + v*w)*2
def qinv(q): return np.array([q[0], -q[1], -q[2], -q[3]])
def quat_axis(axis, deg):
    a = math.radians(deg)/2; ax = np.array(axis); ax = ax / np.linalg.norm(ax)
    return np.array([math.cos(a)] + [math.sin(a)*c for c in ax])

GUN = 70.0
ROUND = [4, 2]
OFFSET = np.array([-0.007, -0.00182941, 0.1019482])   # legacy grip in raw (main hand)
THROWFRAME = qmul(quat_axis([0,1,0], 4.0), quat_axis([1,0,0], -39.5))

class Motion:
    """mock play of one throw's keys: piecewise linear positions, pitch (slerp about x == linear pitch)."""
    def __init__(self, keys):
        self.t = [round(k[0], ROUND[0]) for k in keys]; self.p = [np.array([round(c, ROUND[0]) for c in k[1:4]]) for k in keys]; self.a = [round(k[4] + GUN, ROUND[1]) for k in keys]
    def at(self, t):
        i = int(np.searchsorted(self.t, t) - 1); i = max(0, min(i, len(self.t) - 2))
        t0, t1 = self.t[i], self.t[i+1]; s = min(1, max(0, (t - t0) / (t1 - t0)))
        pos = self.p[i] + (self.p[i+1] - self.p[i]) * s; pitch = self.a[i] + (self.a[i+1] - self.a[i]) * s
        inside = t0 <= t <= t1
        vel = (self.p[i+1] - self.p[i]) / (t1 - t0) if inside else np.zeros(3)
        w = math.radians(self.a[i+1] - self.a[i]) / (t1 - t0) if inside else 0.0
        return pos, pitch, vel, np.array([w, 0, 0])

def samples_for(motion, times):
    out = []; last = None
    for t in times:
        pos, pitch, vel, ang = motion.at(t)
        q = quat_x(pitch)
        grip = vel + np.cross(ang, qrot(q, OFFSET))
        fwd = qrot(qmul(q, THROWFRAME), np.array([0, 0, -1.0]))
        spin = np.zeros(3); spin_t = t
        if last is not None:
            d = qmul(q, qinv(last[1]))
            if d[0] < 0: d = -d
            v = np.array(d[1:]); s = np.linalg.norm(v); dt = t - last[0]
            if s > 1e-9: spin = v * (2*math.atan2(s, d[0]) / (s*dt))
            spin_t = t - dt/2
        last = (t, q)
        out.append(dict(time=t, pos=pos, vel=grip, ang=spin, ang_t=spin_t, fwd=fwd))
    return out

# ---------------- old estimator (vr_throw.cpp as of f693dc76) ----------------
WINDOW, LOOK, SPAN, LEVER, DIRLB, ANGTH, ANGF, PEAKFIT = 0.12, 0.01, 0.017, 0.1, 0.04, 6.0, 0.7, 0.03
def old_fit(h, pt, best):
    xs = [s['time'] - pt for s in h if abs(s['time'] - pt) <= PEAKFIT]; ys = [np.linalg.norm(s['vel']) for s in h if abs(s['time'] - pt) <= PEAKFIT]
    if len(xs) < 3: return 0
    c2, c1, c0 = np.polyfit(xs, ys, 2)
    if c2 >= 0: return 0
    x = min(max(-c1/(2*c2), -PEAKFIT), PEAKFIT); top = c0 + c1*x + c2*x*x
    return min(max(top, 0.8*best), 1.2*best)
def old_estimate(h, rel):
    lo, hi = rel - WINDOW, rel + LOOK
    cand = [s for s in h if lo <= s['time'] <= hi] or [h[-1]]
    peak = max(cand, key=lambda s: np.linalg.norm(s['vel'])); best = np.linalg.norm(peak['vel'])
    vel = np.mean([s['vel'] for s in h if abs(s['time']-peak['time']) <= SPAN], axis=0)
    ang = np.mean([s['ang'] for s in h if abs(s['time']-peak['time']) <= 2*SPAN], axis=0)
    f = old_fit(h, peak['time'], best)
    if f > 0: vel = vel/np.linalg.norm(vel) * max(np.linalg.norm(vel), f)
    d = np.sum_([s['vel'] for s in h if peak['time'] - DIRLB <= s['time'] <= peak['time']], axis=0)
    vel = d/np.linalg.norm(d)*np.linalg.norm(vel)
    if np.linalg.norm(ang) > ANGTH: vel = vel + np.cross(ang, peak['fwd']*LEVER)*ANGF
    return vel, ang, peak['time']

# ---------------- driver ----------------
def throws(keyrate=1000.0):
    tp.RATE = keyrate
    return [("overhand", tp.arc_throw("overhand", 150, 40, 95, 35, -45, 0.30)), ("lob", tp.arc_throw("lob", -120, -30, -60, -20, 30, 0.45)),
            ("flat", tp.line_throw("flat", 5, 5.0, 0.30)), ("overhand0", tp.arc_throw("overhand0", 150, 40, 95, 0, 0, 0.30))]

def frame_times(fps, t_from, t_to, phase=0.0, jitter=0.0, seed=1):
    rnd = random.Random(seed); t = t_from + phase / fps; out = []
    while t < t_to:
        out.append(t); t += (1.0/fps) * (1 + (rnd.uniform(-jitter, jitter) if jitter else 0))
    return out

def describe(vel, ang):
    sp = np.linalg.norm(vel); el = math.degrees(math.atan2(vel[1], -vel[2]))
    return sp, el, np.linalg.norm(ang)

def run(estimator, fps, info=None, phase=0.0, jitter=0.0, keyrate=1000.0, server_delay=True, seed=1, release_interp=False):
    res = {}; t0 = 1.0
    allt = frame_times(fps, 0.0, 7.0, phase, jitter, seed)
    for name, (keys, rel) in throws(keyrate):
        m = Motion(keys)
        times = [t - t0 for t in allt if t0 - 0.5 <= t <= t0 + keys[-1][0] + 0.2]
        t0 += keys[-1][0] + 1.0
        h = samples_for(m, times)
        k = next(i for i, t in enumerate(times) if t >= rel)       # the frame the grip reads let go
        relt = times[k]
        if release_interp == 'exact':   # an analog grip's ramp: its crossing interpolated between the frames, exactly
            relt = rel
        elif release_interp:   # the crossing of the floor (0.35) between the frames, the grip a step 1 -> 0 at `rel`
            relt = times[k-1] + (times[k] - times[k-1]) * 0.65
        kk = k
        if server_delay:     # the move is built at a server frame: every ceil((1/72)/dt) host frames
            n = max(1, math.ceil(round((1/72) * fps, 6)))
            while kk % n: kk += 1
        e = estimator(h[:kk+1], relt)
        res[name] = describe(*e[:2])
        if info is not None: info[name] = ((relt - e[2]) * 1000, (times[kk] - relt) * 1000)
    return res

# ---------------- new estimator: time-based (the samples as a piecewise-linear signal) ----------------
class Signal:
    """piecewise-linear in time between the samples (key: 'vel' at 'time', or 'ang' at 'ang_t'); flat beyond the ends."""
    def __init__(self, h, key, tkey):
        self.t = [s[tkey] for s in h]; self.v = [s[key] for s in h]
        self.F = [0.0]   # prefix integrals at the sample times (vectors)
        F = np.zeros(3)
        for i in range(1, len(h)):
            F = F + (self.v[i] + self.v[i-1]) * (0.5 * (self.t[i] - self.t[i-1])); self.F.append(F)
        self.F[0] = np.zeros(3)
    def at(self, t):
        T = self.t
        if t <= T[0]: return self.v[0]
        if t >= T[-1]: return self.v[-1]
        i = np.searchsorted(T, t) - 1
        s = (t - T[i]) / (T[i+1] - T[i]); return self.v[i] + (self.v[i+1] - self.v[i]) * s
    def integral(self, t):   # from T[0] to t (flat extension beyond the ends)
        T = self.t
        if t <= T[0]: return self.v[0] * (t - T[0])
        if t >= T[-1]: return self.F[-1] + self.v[-1] * (t - T[-1])
        i = np.searchsorted(T, t) - 1
        return self.F[i] + (self.v[i] + self.at(t)) * (0.5 * (t - T[i]))
    def mean(self, a, b):
        if b - a < 1e-9: return self.at(a)
        return (self.integral(b) - self.integral(a)) / (b - a)

GRID = 0.001
def new_estimate(h, rel, span=SPAN, window=WINDOW, lookback=DIRLB, peakfit=PEAKFIT, centroid=0.0):
    to = rel; frm = rel - window
    lo = h[0]['time']
    V = Signal(h, 'vel', 'time'); W = Signal(h, 'ang', 'ang_t')
    def box(t, half):
        a, b = max(t - half, lo), min(t + half, to)
        return V.mean(a, b)
    n = max(1, int(round((to - frm) / GRID)))
    ts_ = [frm + (to - frm) * i / n for i in range(n + 1)]
    S = [np.linalg.norm(box(t, span)) for t in ts_]
    j = max(range(len(S)), key=lambda i: S[i])
    tp_ = ts_[j]
    if 0 < j < n:   # parabola through the three grid points
        a, b, c = S[j-1], S[j], S[j+1]; den = a - 2*b + c
        if den < 0: tp_ += GRID * 0.5 * (a - c) / den * (to - frm) / n / GRID
    if centroid > 0:   # the middle of where the smoothed speed is within `centroid` of its top
        top = max(S); wsum = 0; tsum = 0
        i = j
        while i >= 0 and S[i] >= top * (1 - centroid): i -= 1
        k = j
        while k <= n and S[k] >= top * (1 - centroid): k += 1
        for m in range(i + 1, k):
            w = S[m] - top * (1 - centroid); wsum += w; tsum += w * ts_[m]
        if wsum > 0: tp_ = tsum / wsum
    vel = box(tp_, span)
    # the speed at the peak: a quadratic fitted (least squares, each sample weighted by the time it stands for) to the
    # speeds within peakfit of it
    xs, ys, ws = [], [], []
    T = [s['time'] for s in h]
    for i, s in enumerate(h):
        x = s['time'] - tp_
        if abs(x) > peakfit or s['time'] > to + 1e-9: continue
        a = T[i-1] if i > 0 else T[i]; b = T[i+1] if i + 1 < len(T) else T[i]
        a = max(a, tp_ - peakfit, lo); b = min(b, tp_ + peakfit, to)
        w = max(0.5 * (b - a), 1e-6)
        xs.append(x); ys.append(np.linalg.norm(s['vel'])); ws.append(w)
    sp = np.linalg.norm(vel)
    if len(xs) >= 3:
        c2, c1, c0 = np.polyfit(xs, ys, 2, w=ws)
        if c2 < 0:
            x = min(max(-c1/(2*c2), -peakfit), peakfit); top = c0 + c1*x + c2*x*x
            top = min(max(top, 0.8 * sp), 1.2 * sp)
            if sp > 1e-4: vel = vel / sp * max(sp, top)
    d = V.mean(max(tp_ - lookback, lo), tp_) if lookback > 0 else vel
    if np.linalg.norm(d) > 1e-4: vel = d / np.linalg.norm(d) * np.linalg.norm(vel)
    ang = W.mean(max(tp_ - 2*span, lo), min(tp_ + 2*span, to))
    # forward at the peak
    i = max(0, np.searchsorted(T, tp_) - 1); fwd = h[i]['fwd']
    if np.linalg.norm(ang) > ANGTH: vel = vel + np.cross(ang, fwd*LEVER)*ANGF
    return vel, ang, tp_

def table(est, fpss=(72, 90, 120, 144, 240), keyrate=1000.0, phases=(0, 0.25, 0.5, 0.75), jitter=0.0, **kw):
    agg = {}
    for fps in fpss:
        for ph in phases:
            r = run(est, fps, phase=ph, keyrate=keyrate, jitter=jitter, seed=int(ph*100)+fps, **kw)
            for k, v in r.items(): agg.setdefault(k, []).append((fps, ph) + v)
    for k, rows in agg.items():
        sp = [r[2] for r in rows]; el = [r[3] for r in rows]; an = [r[4] for r in rows]
        print('%-9s speed %.2f..%.2f (%.1f%%)  elev %+.1f..%+.1f (%.1f deg)  spin %.1f..%.1f (%.1f%%)' % (k, min(sp), max(sp),
              100*(max(sp)-min(sp))/max(sp), min(el), max(el), max(el)-min(el), min(an), max(an), 100*(max(an)-min(an))/max(max(an),1e-6)))

QSTEP = 0.00025
def local_fit(sig, t, a, b, deg, scalar=False):
    """least squares polynomial (degree deg) in (u = time - t) to the signal over [a, b], continuous (quadrature): its value at t."""
    n = max(2, int(math.ceil((b - a) / QSTEP)))
    S = [0.0] * 5; T = [np.zeros(3) if not scalar else 0.0 for _ in range(3)]
    for i in range(n + 1):
        u = a + (b - a) * i / n; w = (b - a) / n * (0.5 if i in (0, n) else 1.0)
        y = sig.at(u); y = np.linalg.norm(y) if scalar else y; x = u - t
        p = 1.0
        for k in range(5):
            S[k] += w * p
            if k < 3: T[k] = T[k] + y * (w * p)
            p *= x
    if deg == 0 or b - a < 1e-6: return T[0] * (1.0 / S[0])
    if deg == 1:
        det = S[0]*S[2] - S[1]*S[1]
        return (T[0] * S[2] - T[1] * S[1]) * (1.0 / det)
    M = [[S[0], S[1], S[2]], [S[1], S[2], S[3]], [S[2], S[3], S[4]]]
    def det3(m): return m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1]) - m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0]) + m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0])
    d = det3(M)
    # value at u = t (x = 0): the constant coefficient, by Cramer's rule with column 0 replaced
    if scalar:
        mc = [[T[r], M[r][1], M[r][2]] for r in range(3)]; return det3(mc) / d
    out = []
    for c in range(3):
        mc = [[T[r][c], M[r][1], M[r][2]] for r in range(3)]; out.append(det3(mc) / d)
    return np.array(out)

def new_estimate2(h, rel, span=SPAN, window=WINDOW, lookback=DIRLB, peakfit=PEAKFIT, deg=1, speedfit=2, grid=0.001, vdeg=1, adeg=1, centroid=0.0):
    to = rel; frm = rel - window; lo = h[0]['time']
    V = Signal(h, 'vel', 'time'); W = Signal(h, 'ang', 'ang_t')
    sm = lambda t: local_fit(V, t, max(t - span, lo), min(t + span, to), deg)
    n = max(1, int(round((to - frm) / grid)))
    ts_ = [frm + (to - frm) * i / n for i in range(n + 1)]
    S = [np.linalg.norm(sm(t)) for t in ts_]
    j = max(range(len(S)), key=lambda i: S[i]); tp_ = ts_[j]
    if 0 < j < n:
        a, b, c = S[j-1], S[j], S[j+1]; den = a - 2*b + c
        if den < 0: tp_ += (ts_[1] - ts_[0]) * 0.5 * (a - c) / den
    if centroid > 0:
        top = S[j]; floor_ = top * (1 - centroid); i = j; k = j
        while i > 0 and S[i-1] >= floor_: i -= 1
        while k < n and S[k+1] >= floor_: k += 1
        wsum = sum(S[m] - floor_ for m in range(i, k+1))
        if wsum > 0: tp_ = sum((S[m] - floor_) * ts_[m] for m in range(i, k+1)) / wsum
    vel = local_fit(V, tp_, max(tp_ - span, lo), min(tp_ + span, to), vdeg); sp = np.linalg.norm(vel)
    if speedfit and tp_ < to - grid * 0.5:
        q = local_fit(V, tp_, max(tp_ - peakfit, lo), min(tp_ + peakfit, to), speedfit, scalar=True)
        if sp > 1e-4 and q > sp: vel = vel / sp * min(q, 1.2 * sp)
    d = V.mean(max(tp_ - lookback, lo), tp_) if lookback > 0 else vel
    if np.linalg.norm(d) > 1e-4: vel = d / np.linalg.norm(d) * np.linalg.norm(vel)
    ang = local_fit(W, tp_, max(tp_ - 2*span, lo), min(tp_ + 2*span, to), adeg)
    F = Signal(h, 'fwd', 'time'); fwd = F.at(tp_); fwd = fwd / np.linalg.norm(fwd)
    if np.linalg.norm(ang) > ANGTH: vel = vel + np.cross(ang, fwd*LEVER)*ANGF
    return vel, ang, tp_
