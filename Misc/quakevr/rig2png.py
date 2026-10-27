#!/usr/bin/env python3
# rig2png.py -- a tiny PNG rasterizer (no third-party modules): make_rig2.py's previews (the mesh in three orthographic
# views, its vertices coloured by bone, the joints marked) for checking where the joints are.

import struct
import zlib


class Canvas:
    def __init__(self, w, h, bg=(24, 24, 28)):
        self.w, self.h = w, h
        self.px = bytearray(bg * (w * h))

    def put(self, x, y, c):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h:
            i = 3 * (y * self.w + x)
            self.px[i:i + 3] = bytes(c)

    def line(self, x0, y0, x1, y1, c):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for k in range(n + 1):
            t = k / n
            self.put(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, c)

    def dot(self, x, y, c, r=2):
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if dx * dx + dy * dy <= r * r:
                    self.put(x + dx, y + dy, c)

    def tri(self, a, b, c, col):
        # filled, flat (no depth: callers draw back to front)
        xs = [a[0], b[0], c[0]]
        ys = [a[1], b[1], c[1]]
        x0, x1 = int(max(min(xs), 0)), int(min(max(xs), self.w - 1))
        y0, y1 = int(max(min(ys), 0)), int(min(max(ys), self.h - 1))
        d = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])
        if abs(d) < 1e-9:
            return
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                w1 = ((x - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (y - a[1])) / d
                w2 = ((b[0] - a[0]) * (y - a[1]) - (x - a[0]) * (b[1] - a[1])) / d
                if w1 >= 0 and w2 >= 0 and w1 + w2 <= 1:
                    self.put(x, y, col)

    def save(self, path):
        raw = b''.join(b'\0' + bytes(self.px[3 * y * self.w:3 * (y + 1) * self.w]) for y in range(self.h))

        def chunk(t, data):
            return struct.pack('>I', len(data)) + t + data + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)

        with open(path, 'wb') as f:
            f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', self.w, self.h, 8, 2, 0, 0, 0)) +
                    chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))
