# compare.py <exact|noisy|jitter|k90|step> [top share, e.g. 0.02: only the new estimate] -- the throw estimate before and
# after 2026-10-06 (ROUND21.md, "Throws at any frame rate") over 72-240 fps and two frame phases: the spread of speed,
# elevation and spin of throw_plays.py's four throws.
import sys, functools; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import throwsim as ts
NEW = functools.partial(ts.new_estimate2, vdeg=1, adeg=0, centroid=float(sys.argv[2]) if len(sys.argv) > 2 else 0.0)
mode = sys.argv[1]
if mode == 'noisy':      # the files as throw_plays.py writes them now (%.4f positions, %.2f angles)
    ts.ROUND[:] = [4, 2]; kw = dict(release_interp='exact')
elif mode == 'k90':
    ts.ROUND[:] = [7, 5]; kw = dict(release_interp='exact', keyrate=90)
elif mode == 'jitter':
    ts.ROUND[:] = [7, 5]; kw = dict(release_interp='exact', jitter=0.3)
elif mode == 'exact':
    ts.ROUND[:] = [7, 5]; kw = dict(release_interp='exact')
elif mode == 'step':     # the grip a step 1 -> 0: its crossing of the floor interpolated between the frames
    ts.ROUND[:] = [7, 5]; kw = dict(release_interp=True)
for name, est in ((('OLD', ts.old_estimate),) if len(sys.argv) <= 2 else ()) + (('NEW', NEW),):
    print(name, mode); ts.table(est, phases=(0, 0.5), **kw)
