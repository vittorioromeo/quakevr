# flatfov_check.py <screenshot dir> "<wids>" [sheet.png] -- judges flatfov_test.sh's frames (pure Python). Per weapon six
# frames in the order taken: model FOV 70, 90, 110, each with the flashlight on, then off. At fov 70 with every part scaled
# alike (vr_flat_model_fov_near 200, _far 201), the frame at model FOV M is the 70 frame shrunk round the screen's middle
# by s = tan(35) / tan(M / 2), lit the same:
#   size:  IoU of the drawn models' pixels (not the clear colour) and the 70 frame's, shrunk; the part of the screen whose
#          source lies off the 70 frame is left out. Pass at 0.85 or more.
#   light: the flashlight's share (lit / unlit - 1) on the model pixels whose true place is off the screen (their source
#          off the 70 frame), in a band beyond that line, against the band inside it: at least half (it was none there:
#          the part went dark below a line). Too few pixels on either side (the gun does not cross the line, or the light
#          barely reaches it): not judged.
# Prints a line a check and `flatfov: N passed, M failed`.
import math, os, struct, sys, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import quakeimage

def write_png(path, w, h, rows): # (eyeshot_sheet.py's)
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    def chunk(k, b): return struct.pack(">I", len(b)) + k + b + struct.pack(">I", zlib.crc32(k + b) & 0xffffffff)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                           chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))

STEP = 2          # every other pixel each way
IOU_MIN = 0.85
BAND = 40         # px (at 960 wide) each side of the line
MIN_PIXELS = 150  # sampled pixels a side for the light check
MIN_SHARE = 0.04  # the inside band's share under this: the light does not reach; not judged

def load(p):
    w, h, rgb = quakeimage.read_rgb(open(p, "rb").read(), "png")
    return w, h, rgb

def main():
    shots = sys.argv[1].strip()
    wids = sys.argv[2].split()
    sheet = sys.argv[3] if len(sys.argv) > 3 else None
    files = sorted((os.path.join(shots, f) for f in os.listdir(shots) if f.lower().endswith(".png")), key=os.path.getmtime)
    if len(files) != 6 * len(wids):
        print("flatfov: %d frames, %d expected: FAIL" % (len(files), 6 * len(wids)))
        print("flatfov: 0 passed, 1 failed")
        return 1
    passed = failed = 0
    tiles = []
    for k, wid in enumerate(wids):
        frames = [load(f) for f in files[6 * k: 6 * k + 6]]
        w, h, a = frames[0]
        bg = a[0:3]
        def masked(img, i):
            return abs(img[3 * i] - bg[0]) + abs(img[3 * i + 1] - bg[1]) + abs(img[3 * i + 2] - bg[2]) > 24
        def lum(img, i):
            return 0.299 * img[3 * i] + 0.587 * img[3 * i + 1] + 0.114 * img[3 * i + 2]
        cx, cy = w / 2.0, h / 2.0
        band = BAND * w / 960.0
        for j, m in ((1, 90), (2, 110)):
            s = math.tan(math.radians(35.0)) / math.tan(math.radians(m / 2.0))
            lit, unlit = frames[2 * j][2], frames[2 * j + 1][2]
            inter = union = 0
            share = {True: [0.0, 0], False: [0.0, 0]}
            for y in range(0, h, STEP):
                for x in range(0, w, STEP):
                    i = y * w + x
                    sx, sy = cx + (x + 0.5 - cx) / s, cy + (y + 0.5 - cy) / s
                    off = not (0 <= sx < w and 0 <= sy < h)
                    b = masked(lit, i)
                    if not off:
                        p = masked(a, int(sy) * w + int(sx))
                        inter += p and b
                        union += p or b
                    if not b or not masked(unlit, i):
                        continue
                    # how far beyond the line (in this frame's pixels): negative inside
                    d = max(abs(x + 0.5 - cx) - s * cx, abs(y + 0.5 - cy) - s * cy)
                    if abs(d) > band:
                        continue
                    u = lum(unlit, i)
                    if u < 12:
                        continue
                    share[d > 0][0] += lum(lit, i) / u - 1.0
                    share[d > 0][1] += 1
            iou = inter / union if union else 0.0
            ok = iou >= IOU_MIN
            print("flatfov: wid %s model fov %d: size IoU %.3f %s" % (wid, m, iou, "PASS" if ok else "FAIL"))
            passed += ok
            failed += not ok
            sin = share[False][0] / share[False][1] if share[False][1] else 0.0
            sout = share[True][0] / share[True][1] if share[True][1] else 0.0
            if share[False][1] < MIN_PIXELS or share[True][1] < MIN_PIXELS or sin < MIN_SHARE:
                print("flatfov: wid %s model fov %d: light inside %.3f (%d px), past the line %.3f (%d px): not judged" %
                      (wid, m, sin, share[False][1], sout, share[True][1]))
            else:
                ok = sout >= 0.5 * sin
                print("flatfov: wid %s model fov %d: light inside %.3f (%d px), past the line %.3f (%d px) %s" %
                      (wid, m, sin, share[False][1], sout, share[True][1], "PASS" if ok else "FAIL"))
                passed += ok
                failed += not ok
        if sheet:
            for f in (0, 2, 4):
                tw, th, img = frames[f]
                tiles.append([bytearray(img[(y * tw + x) * 3:(y * tw + x) * 3 + 3][c] for x in range(0, tw, 4) for c in range(3))
                              for y in range(0, th, 4)])
    if sheet and tiles:
        cols = 6
        tw, th = len(tiles[0][0]) // 3, len(tiles[0])
        rowsn = (len(tiles) + cols - 1) // cols
        out = [bytearray(cols * tw * 3) for _ in range(rowsn * th)]
        for t, tile in enumerate(tiles):
            ox, oy = (t % cols) * tw * 3, (t // cols) * th
            for y, line in enumerate(tile):
                out[oy + y][ox:ox + tw * 3] = line
        write_png(sheet, cols * tw, rowsn * th, out)
        print("flatfov: sheet %s (each weapon at 70, 90, 110; two weapons a row)" % sheet)
    print("flatfov: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
