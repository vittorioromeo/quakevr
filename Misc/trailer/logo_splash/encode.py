# encode.py -- from composite.py's RGBA PNG sequence: the ProRes 4444 movie with its alpha, a preview MP4 over a dark
# checker, a contact sheet, the alpha checks (frames over white and over a bright picture, to see fringes), and a
# smaller copy of the sequence (scaled with premultiplied alpha, so its edges stay clean).
#
#   python encode.py --png <dir> [--prores out.mov] [--preview out.mp4] [--sheet out.png]
#                    [--alphatest out.png --bg <bright picture>] [--downscale <dir> --to 1920] [--fps 60]
#
# PyAV (pip "av") with FFmpeg's prores_ks (profile 4444, yuva444p10le) and libx264.

import argparse
import glob
import os
import sys
import time

import av
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402
from composite import to_straight_u8  # noqa: E402


BITS_PER_MB = 900


def frames_of(d):
    fs = sorted(glob.glob(os.path.join(d, "*.png")))
    if not fs:
        raise SystemExit("no PNGs in %s" % d)
    return fs


def checker(w, h, cell, a=(34, 34, 38), b=(52, 52, 58)):
    yy, xx = np.mgrid[0:h, 0:w]
    m = ((xx // cell + yy // cell) % 2).astype(bool)
    out = np.empty((h, w, 3), np.float32)
    out[...] = np.array(a, np.float32)
    out[m] = np.array(b, np.float32)
    return out


def over_bg(rgba_u8, bg):
    a = rgba_u8[..., 3:4].astype(np.float32) / 255
    return np.clip(rgba_u8[..., :3].astype(np.float32) * a + bg * (1 - a) + 0.5, 0, 255).astype(np.uint8)


def prores(files, out, fps):
    t0 = time.time()
    first = Image.open(files[0])
    w, h = first.size
    with av.open(out, "w") as c:
        s = c.add_stream("prores_ks", rate=fps)
        s.width, s.height = w, h
        s.pix_fmt = "yuva444p10le"
        # A normal ProRes 4444 rate (about 600-800 Mbit/s at 1080p60; left to itself prores_ks spends about 2 Gbit/s
        # on these frames, more than players keep up with).
        s.options = {"profile": "4444", "vendor": "apl0", "alpha_bits": "16", "bits_per_mb": str(BITS_PER_MB)}
        s.codec_context.color_primaries = 1
        s.codec_context.color_trc = 1
        s.codec_context.colorspace = 1
        for f in files:
            arr = np.asarray(Image.open(f).convert("RGBA"))
            vf = av.VideoFrame.from_ndarray(arr, format="rgba")
            vf = vf.reformat(format="yuva444p10le", dst_colorspace="ITU709", dst_color_range="MPEG")
            for p in s.encode(vf):
                c.mux(p)
        for p in s.encode():
            c.mux(p)
    print("%s: %d frames, %.0f MB, %.0f s" % (out, len(files), os.path.getsize(out) / 1e6, time.time() - t0))


def read_wav(path):
    """A 16- or 24-bit PCM WAV as float32 (channels, samples) and its rate."""
    import wave
    with wave.open(path) as w:
        n, ch, sw, rate = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n)
    if sw == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        i = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = (np.where(i >= 1 << 23, i - (1 << 24), i) / 8388608.0).astype(np.float32)
    else:
        x = np.frombuffer(raw, "<i2").astype(np.float32) / 32768
    return x.reshape(-1, ch).T.copy(), rate


def preview(files, out, fps, width=1920, audio=None):
    t0 = time.time()
    h = width * 9 // 16
    bg = checker(width, h, 32)
    with av.open(out, "w") as c:
        s = c.add_stream("libx264", rate=fps)
        s.width, s.height = width, h
        s.pix_fmt = "yuv420p"
        s.options = {"crf": "17", "preset": "medium"}
        if audio:
            pcm, rate = read_wav(audio)
            sa = c.add_stream("aac", rate=rate)
            sa.layout = "stereo"
            sa.bit_rate = 256000
        for f in files:
            im = Image.open(f).convert("RGBA")
            if im.width != width:
                im = im.resize((width, h), Image.LANCZOS)
            rgb = over_bg(np.asarray(im), bg)
            for p in s.encode(av.VideoFrame.from_ndarray(rgb, format="rgb24")):
                c.mux(p)
        for p in s.encode():
            c.mux(p)
        if audio:
            for i in range(0, pcm.shape[1], 1024):
                af = av.AudioFrame.from_ndarray(np.ascontiguousarray(pcm[:, i:i + 1024]), format="fltp", layout="stereo")
                af.sample_rate = rate
                af.pts = i
                for p in sa.encode(af):
                    c.mux(p)
            for p in sa.encode():
                c.mux(p)
    print("%s: %.0f MB, %.0f s" % (out, os.path.getsize(out) / 1e6, time.time() - t0))


def sheet(files, out, cols=6, rows=5, cell=640):
    ch = cell * 9 // 16
    n = cols * rows
    pick = [files[round(i * (len(files) - 1) / (n - 1))] for i in range(n)]
    W = Image.new("RGB", (cols * cell, rows * (ch + 22)), (20, 20, 22))
    bg = checker(cell, ch, 16)
    d = ImageDraw.Draw(W)
    for i, f in enumerate(pick):
        im = Image.open(f).convert("RGBA").resize((cell, ch), Image.LANCZOS)
        x, y = (i % cols) * cell, (i // cols) * (ch + 22)
        W.paste(Image.fromarray(over_bg(np.asarray(im), bg)), (x, y + 22))
        num = int(os.path.splitext(os.path.basename(f))[0].split("_")[-1])
        d.text((x + 6, y + 5), "frame %d  (%.2f s)" % (num, num / C.FPS), fill=(220, 220, 220))
    W.save(out)
    print(out)


def alphatest(files, out, bg_path, picks=(80, 250, 330, 470)):
    """Frames over white, over a bright picture and over black, side by side (straight alpha: no dark fringes)."""
    rows = []
    cell = 960
    ch = cell * 9 // 16
    bright = None
    if bg_path and os.path.exists(bg_path):
        bright = np.asarray(Image.open(bg_path).convert("RGB").resize((cell, ch), Image.LANCZOS), np.float32)
    else:
        yy, xx = np.mgrid[0:ch, 0:cell].astype(np.float32)
        bright = np.stack([200 + 55 * xx / cell, 190 + 40 * yy / ch, 150 + 60 * (1 - xx / cell)], -1)
    for k in picks:
        cand = [f for f in files if f.endswith("_%04d.png" % k)]
        if not cand:
            continue
        im = np.asarray(Image.open(cand[0]).convert("RGBA").resize((cell, ch), Image.LANCZOS))
        row = [over_bg(im, np.full((ch, cell, 3), 255, np.float32)), over_bg(im, bright),
               over_bg(im, np.zeros((ch, cell, 3), np.float32))]
        rows.append(np.concatenate(row, 1))
    Image.fromarray(np.concatenate(rows, 0)).save(out)
    print(out)


def downscale(files, out_dir, width):
    os.makedirs(out_dir, exist_ok=True)
    h = width * 9 // 16
    for f in files:
        a = np.asarray(Image.open(f).convert("RGBA"), np.float32) / 255
        a[..., :3] *= a[..., 3:4]
        ch = [np.asarray(Image.fromarray(a[..., k]).resize((width, h), Image.LANCZOS)) for k in range(4)]
        p = np.clip(np.stack(ch, -1), 0, 1)
        p[..., :3] = np.minimum(p[..., :3], p[..., 3:4])
        u8 = to_straight_u8(p, len(f))       # straight alpha, colour bled into the transparent pixels, dithered
        Image.fromarray(u8, "RGBA").save(os.path.join(out_dir, os.path.basename(f)), compress_level=2)
    print("%s: %d frames at %d" % (out_dir, len(files), width))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", required=True)
    ap.add_argument("--prores")
    ap.add_argument("--preview")
    ap.add_argument("--sheet")
    ap.add_argument("--alphatest")
    ap.add_argument("--bg")
    ap.add_argument("--range", help="only frames a-b")
    ap.add_argument("--audio", help="a WAV muxed into the preview (AAC)")
    ap.add_argument("--downscale")
    ap.add_argument("--to", type=int, default=1920)
    ap.add_argument("--fps", type=int, default=C.FPS)
    a = ap.parse_args()
    files = frames_of(a.png)
    if a.range:
        lo, _, hi = a.range.partition("-")
        num = lambda f: int(os.path.splitext(os.path.basename(f))[0].split("_")[-1])  # noqa: E731
        files = [f for f in files if int(lo) <= num(f) <= int(hi or lo)]
    if a.downscale:
        downscale(files, a.downscale, a.to)
    if a.sheet:
        sheet(files, a.sheet)
    if a.alphatest:
        alphatest(files, a.alphatest, a.bg)
    if a.preview:
        preview(files, a.preview, a.fps, audio=a.audio)
    if a.prores:
        prores(files, a.prores, a.fps)


if __name__ == "__main__":
    main()
