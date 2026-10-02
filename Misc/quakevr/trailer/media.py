"""Reading recordings and music with PyAV (FFmpeg's libraries in a Python package: `import av`; no ffmpeg on PATH
needed): their length, frame rate and size, their sound as numpy samples, their frames' brightness."""

from fractions import Fraction

import av
import numpy as np


class Info:
    """What a media file is: its length (seconds), its video's frame rate (a Fraction, None without video), size, and
    its sound's rate and channels (0 without sound)."""

    def __init__(self, path):
        self.path = path
        with av.open(path) as c:
            v = c.streams.video[0] if c.streams.video else None
            a = c.streams.audio[0] if c.streams.audio else None
            self.duration = float(c.duration) / av.time_base if c.duration else 0.0
            self.fps = None
            self.width = self.height = 0
            if v is not None:
                rate = v.average_rate or v.base_rate or v.guessed_rate
                self.fps = Fraction(rate.numerator, rate.denominator) if rate else Fraction(60)
                self.width, self.height = v.codec_context.width, v.codec_context.height
                if v.duration and v.time_base:
                    self.duration = max(self.duration, float(v.duration * v.time_base))
            self.audio_rate = a.codec_context.sample_rate if a is not None else 0
            self.audio_channels = a.codec_context.layout.nb_channels if a is not None else 0
            if a is not None and not self.duration and a.duration and a.time_base:
                self.duration = float(a.duration * a.time_base)

    @property
    def has_video(self):
        return self.fps is not None

    @property
    def has_audio(self):
        return self.audio_rate > 0


def audio(path, rate=22050, start=0.0, length=None):
    """The file's sound as mono float32 samples at `rate`, from `start` seconds, `length` seconds of it (None: to the
    end); and the time of its first sample."""
    out = []
    first = None
    with av.open(path) as c:
        if not c.streams.audio:
            return np.zeros(0, np.float32), start
        s = c.streams.audio[0]
        if start > 0:
            c.seek(int(start / s.time_base), stream=s, any_frame=False, backward=True)
        resampler = av.AudioResampler(format="flt", layout="mono", rate=rate)
        need = None if length is None else int(length * rate)
        for frame in c.decode(s):
            t = float(frame.pts * s.time_base) if frame.pts is not None else 0.0
            if length is not None and t > start + length + 0.5:
                break
            for r in resampler.resample(frame):
                if first is None:
                    first = t
                out.append(r.to_ndarray().reshape(-1))
    if not out:
        return np.zeros(0, np.float32), start
    samples = np.concatenate(out).astype(np.float32)
    # From `start` exactly.
    skip = max(0, int(round((start - first) * rate)))
    samples = samples[skip:]
    if need is not None:
        samples = samples[:need]
    return samples, max(start, first)


def brightness(path, start=0.0, length=None, width=64):
    """The video's frames' times and mean brightness (0..255, the luma of a small copy), from `start` seconds."""
    times, values = [], []
    with av.open(path) as c:
        s = c.streams.video[0]
        s.thread_type = "AUTO"
        if start > 0:
            c.seek(int(start / s.time_base), stream=s, any_frame=False, backward=True)
        for frame in c.decode(s):
            if frame.pts is None:
                continue
            t = float(frame.pts * s.time_base)
            if t < start - 1e-6:
                continue
            if length is not None and t > start + length:
                break
            h = max(2, int(round(frame.height * width / frame.width)))
            y = frame.reformat(width=width, height=h, format="gray").to_ndarray()
            times.append(t)
            values.append(float(y.mean()))
    return np.array(times), np.array(values)
