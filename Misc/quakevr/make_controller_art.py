#!/usr/bin/env python3
"""make_controller_art.py -- the Controller Bindings screen's pictures (vr_bindsetup.cpp, ROUND26.md "ctrlbind").

Draws every controller the screen knows as a flat vector-style picture (our own drawing: no product photos, no logos),
each button's anchor point recorded where it is drawn, and writes:
  quakevr/gfx/vr/controllers/<art>[_l|_r].png   512 wide (VR hands 512 x 512: the face seen from above, the side under it;
                                                gamepads 512 x 384, the front)
  quakevr/gfx/vr/controllers/<art>[_l|_r]_s.png the same at half size (the reminder panels)
  quakevr/gfx/vr/glyphs/<name>.png              64 x 64 button glyphs: Kenney's Input Prompts (CC0) where the pack has
                                                the button (--kenney <the pack's zip>), else drawn here in its style
  Quake/vr/vr_bindsetup_art.inc                 the anchors, button names and glyphs (C++ table; do not edit)

Usage: python Misc/quakevr/make_controller_art.py [--kenney <kenney_input-prompts_1.zip>]
Without --kenney the glyphs already in quakevr/gfx/vr/glyphs are kept; the drawn ones are written again.
"""

import argparse
import io
import math
import os
import zipfile

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT_PICS = os.path.join(ROOT, 'quakevr', 'gfx', 'vr', 'controllers')
OUT_GLYPHS = os.path.join(ROOT, 'quakevr', 'gfx', 'vr', 'glyphs')
OUT_INC = os.path.join(ROOT, 'Quake', 'vr', 'vr_bindsetup_art.inc')

SS = 4  # supersampling
BODY = (46, 50, 58, 255)
BODY_DARK = (30, 33, 39, 255)
RIM = (150, 158, 172, 255)
BUTTON = (88, 95, 108, 255)
BUTTON_RIM = (190, 196, 206, 255)
TRIGGER = (70, 76, 88, 255)
TEXT = (235, 238, 242, 255)
ACCENT = (205, 120, 60, 255)  # a grip's or trigger's face (the parts the fingers rest on)

FONT_PATHS = ['C:/Windows/Fonts/segoeuib.ttf', 'C:/Windows/Fonts/arialbd.ttf',
              '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf']


def font(size):
    for p in FONT_PATHS:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def catmull(points, steps=12):
    """A closed smooth outline through `points` (Catmull-Rom)."""
    out = []
    n = len(points)
    for i in range(n):
        p0, p1, p2, p3 = points[(i - 1) % n], points[i], points[(i + 1) % n], points[(i + 2) % n]
        for s in range(steps):
            t = s / steps
            t2, t3 = t * t, t * t * t
            x = 0.5 * ((2 * p1[0]) + (-p0[0] + p2[0]) * t + (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2 +
                       (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3)
            y = 0.5 * ((2 * p1[1]) + (-p0[1] + p2[1]) * t + (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2 +
                       (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3)
            out.append((x, y))
    return out


class Canvas:
    """A picture drawn in 512-wide units (supersampled), mirrored for a left hand, with the anchors it records."""

    def __init__(self, w, h, mirror=False):
        self.w, self.h, self.mirror = w, h, mirror
        self.img = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)
        self.anchors = {}

    def x(self, x):
        return (self.w - x) if self.mirror else x

    def pt(self, p):
        return (self.x(p[0]) * SS, p[1] * SS)

    def blob(self, pts, fill=BODY, rim=RIM, width=3, smooth=True):
        poly = catmull(pts) if smooth else pts
        self.d.polygon([self.pt(p) for p in poly], fill=fill, outline=rim, width=width * SS)

    def ellipse(self, cx, cy, rx, ry, fill=BUTTON, rim=BUTTON_RIM, width=2):
        cx = self.x(cx)
        self.d.ellipse([(cx - rx) * SS, (cy - ry) * SS, (cx + rx) * SS, (cy + ry) * SS], fill=fill, outline=rim,
                       width=int(width * SS))

    def ring(self, cx, cy, rx, ry, thick, color=BODY, rim=RIM):
        self.ellipse(cx, cy, rx + thick / 2, ry + thick / 2, fill=None, rim=rim, width=thick + 4)
        self.ellipse(cx, cy, rx + thick / 2, ry + thick / 2, fill=None, rim=color, width=thick)

    def rrect(self, x0, y0, x1, y1, r, fill=BUTTON, rim=BUTTON_RIM, width=2):
        a, b = sorted((self.x(x0), self.x(x1)))
        self.d.rounded_rectangle([a * SS, y0 * SS, b * SS, y1 * SS], radius=r * SS, fill=fill, outline=rim,
                                 width=int(width * SS))

    def text(self, cx, cy, s, size, color=TEXT):
        f = font(size * SS)
        self.d.text((self.x(cx) * SS, cy * SS), s, fill=color, font=f, anchor='mm')

    def line(self, pts, color=RIM, width=3):
        self.d.line([self.pt(p) for p in pts], fill=color, width=width * SS, joint='curve')

    def button(self, key, cx, cy, r, label='', fill=BUTTON, size=None, color=TEXT):
        self.ellipse(cx, cy, r, r, fill=fill)
        if label:
            self.text(cx, cy - r * 0.05, label, size or int(r * 1.15), color)
        self.anchor(key, cx, cy)

    def stick(self, key, cx, cy, r):
        self.ellipse(cx, cy, r * 1.3, r * 1.3, fill=BODY_DARK, rim=RIM)
        self.ellipse(cx, cy, r, r, fill=BUTTON)
        self.ellipse(cx, cy, r * 0.62, r * 0.62, fill=(76, 82, 94, 255), rim=(120, 128, 140, 255), width=1.5)
        self.anchor(key, cx, cy)

    def dpad(self, keys, cx, cy, arm, w):
        # keys: up, down, left, right (None: not anchored)
        self.rrect(cx - w / 2, cy - arm, cx + w / 2, cy + arm, 4)
        self.rrect(cx - arm, cy - w / 2, cx + arm, cy + w / 2, 4)
        self.d.rectangle([(self.x(cx) - w / 2 + 2) * SS, (cy - w / 2 + 2) * SS, (self.x(cx) + w / 2 - 2) * SS,
                          (cy + w / 2 - 2) * SS], fill=BUTTON)
        for k, (dx, dy) in zip(keys, ((0, -1), (0, 1), (-1, 0), (1, 0))):
            ax, ay = cx + dx * arm * 0.62, cy + dy * arm * 0.62
            if k:
                self.anchor(k, ax, ay)
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
            tx, ty = cx + dx * arm * 0.62, cy + dy * arm * 0.62
            s = 6
            pts = [(tx + dx * s, ty + dy * s), (tx - dx * s * 0.4 + dy * s, ty - dy * s * 0.4 + dx * s),
                   (tx - dx * s * 0.4 - dy * s, ty - dy * s * 0.4 - dx * s)]
            self.d.polygon([self.pt(p) for p in pts], fill=TEXT)

    def anchor(self, key, x, y):
        self.anchors[key] = (self.x(x) / self.w, y / self.h)

    def save(self, path):
        big = self.img.resize((self.w, self.h), Image.LANCZOS)
        big.save(path, optimize=True)
        big.resize((self.w // 2, self.h // 2), Image.LANCZOS).save(path.replace('.png', '_s.png'), optimize=True)


# ---------------------------------------------------------------------------------------------------------------------
# VR controllers: drawn as the right hand (mirrored for the left). Face view in y 0..300, side view in y 300..512, the
# controller pointing left in the side view.
# ---------------------------------------------------------------------------------------------------------------------

def side_touch(c, ring=None, slim=False):
    if ring == 'top':
        c.ring(235, 338, 95, 26, 16)
    head = [(165, 330), (250, 312), (330, 322), (345, 350), (300, 368), (200, 372), (160, 356)]
    handle = [(262, 352), (322, 352), (352, 430), (350, 492), (318, 506), (290, 470), (268, 410)]
    c.blob(handle)
    c.blob(head)
    if ring == 'front':
        c.ring(205, 360, 60, 40, 14)
    # trigger: a paddle under the head's front
    c.blob([(188, 366), (232, 368), (236, 392), (222, 420), (200, 418), (186, 392)], fill=TRIGGER, rim=ACCENT, width=3)
    c.anchor('trigger', 210, 392)
    # grip: on the handle's front
    c.blob([(272, 404), (292, 400), (306, 450), (300, 482), (286, 478), (276, 440)], fill=TRIGGER, rim=ACCENT, width=3)
    c.anchor('grip', 288, 442)
    c.text(256, 500 if not slim else 500, '', 10)


def face_touch(c, hand, kind):
    """kind: quest2 (ring above), quest3 (ringless), questpro (ringless, cameras), cv1 (ring round the bottom),
    frame (Steam Frame), pico (ring above, Pico's)."""
    right = hand == 'r'
    if kind in ('quest2', 'pico'):
        c.ring(300, 96, 112, 64, 22)
    if kind == 'cv1':
        c.ring(262, 178, 120, 88, 18)
    c.blob([(232, 178), (296, 178), (304, 240), (288, 292), (250, 296), (226, 246)])  # the handle seen from above
    if kind == 'questpro':
        head = [(150, 120), (190, 64), (262, 48), (330, 70), (352, 130), (318, 196), (240, 212), (168, 184)]
    elif kind == 'frame':
        head = [(160, 118), (200, 68), (262, 56), (322, 78), (342, 132), (306, 192), (236, 204), (172, 176)]
    else:
        head = [(146, 124), (186, 62), (262, 44), (338, 66), (362, 128), (324, 198), (240, 216), (164, 188)]
    c.blob(head)
    if kind == 'questpro':
        for x, y in ((170, 92), (350, 104), (300, 196)):
            c.ellipse(x, y, 8, 8, fill=BODY_DARK, rim=(110, 116, 128, 255), width=1.5)
    c.stick('stick', 214, 124, 30)
    if kind == 'frame' and not right:
        c.dpad(('secondary', 'primary', None, None), 300, 136, 34, 22)
    else:
        c.button('primary', 300, 160, 20, 'A' if right else 'X')
        c.button('secondary', 326, 106, 20, 'B' if right else 'Y')
    if right:
        c.ellipse(258, 196, 9, 9, fill=BODY_DARK)  # the system button (the runtime's)
        if kind == 'frame':
            c.button('menu', 230, 186, 9, '')
    else:
        c.button('menu', 258, 196, 10, '')
        c.text(258, 196, '\u2261', 14)
    side_touch(c, 'top' if kind in ('quest2', 'pico') else 'front' if kind == 'cv1' else None)


def face_index(c, hand):
    right = hand == 'r'
    c.blob([(214, 40), (262, 26), (300, 46), (304, 160), (292, 292), (240, 296), (222, 170)])
    c.line([(306, 60), (348, 120), (352, 220), (300, 290)], color=RIM, width=6)  # the strap
    c.stick('stick', 252, 82, 22)
    c.ellipse(250, 158, 20, 34, fill=BODY_DARK, rim=RIM)  # the trackpad
    c.button('primary', 286, 140, 11, 'A', size=13)
    if right:
        c.button('secondary', 286, 108, 11, 'B', size=13)
    else:
        c.button('menu', 286, 108, 11, 'B', size=13)
    c.ellipse(256, 222, 7, 7, fill=BODY_DARK)
    handle = [(170, 322), (300, 316), (330, 340), (360, 470), (330, 506), (300, 470), (250, 350), (175, 346)]
    c.blob(handle)
    c.line([(320, 330), (420, 380), (392, 500), (340, 504)], color=RIM, width=6)
    c.blob([(176, 344), (214, 346), (218, 372), (200, 396), (180, 386)], fill=TRIGGER, rim=ACCENT, width=3)
    c.anchor('trigger', 196, 368)
    c.blob([(262, 364), (290, 360), (330, 460), (318, 486), (300, 470)], fill=TRIGGER, rim=ACCENT, width=3)
    c.anchor('grip', 298, 426)


def face_vive(c, hand):
    right = hand == 'r'
    c.ring(256, 92, 86, 70, 20)
    c.blob([(214, 150), (298, 150), (292, 296), (220, 296)])
    c.ellipse(256, 196, 36, 36, fill=BODY_DARK, rim=RIM)
    c.anchor('primary', 256, 196)
    c.anchor('stick', 256, 196)
    if right:
        c.button('secondary', 256, 146, 9, '')
    else:
        c.button('menu', 256, 146, 9, '')
    c.text(256, 146, '\u2261', 12)
    c.ellipse(256, 254, 7, 7, fill=BODY_DARK)
    c.ring(205, 340, 70, 30, 14)
    c.blob([(150, 344), (330, 330), (346, 352), (338, 500), (300, 506), (290, 380), (160, 368)])
    c.blob([(180, 366), (220, 368), (222, 396), (204, 418), (186, 402)], fill=TRIGGER, rim=ACCENT, width=3)
    c.anchor('trigger', 202, 390)
    c.rrect(306, 410, 330, 456, 6, fill=TRIGGER, rim=ACCENT, width=3)
    c.anchor('grip', 318, 433)


def face_wmr(c, hand):
    right = hand == 'r'
    c.ring(256, 84, 112, 62, 18)
    for i in range(9):
        a = math.pi * (0.1 + 0.8 * i / 8)
        c.ellipse(256 + 112 * math.cos(a + math.pi), 84 - 62 * math.sin(a), 3, 3, fill=(120, 126, 138, 255), width=1)
    c.blob([(196, 140), (316, 140), (322, 210), (292, 294), (226, 294), (190, 210)])
    c.stick('stick', 226, 178, 22)
    c.ellipse(288, 186, 26, 26, fill=BODY_DARK, rim=RIM)
    c.anchor('primary', 288, 186)
    if right:
        c.button('secondary', 242, 236, 8, '')
    else:
        c.button('menu', 242, 236, 8, '')
    c.text(242, 236, '\u2261', 11)
    c.ellipse(276, 240, 7, 7, fill=BODY_DARK)
    side_touch(c, 'top')


def face_simple(c, hand):
    c.blob([(206, 60), (306, 60), (300, 290), (212, 290)])
    c.button('menu', 256, 120, 12, '')
    c.text(256, 120, '\u2261', 14)
    c.blob([(170, 330), (330, 330), (340, 360), (320, 500), (290, 500), (280, 370), (175, 366)])
    c.blob([(186, 364), (224, 366), (226, 392), (206, 414), (188, 398)], fill=TRIGGER, rim=ACCENT, width=3)
    c.anchor('trigger', 204, 388)


# ---------------------------------------------------------------------------------------------------------------------
# Gamepads (512 x 384, the front)
# ---------------------------------------------------------------------------------------------------------------------

def pad_body(c, kind):
    lt, rt = ('LT', 'RT') if kind.startswith('xbox') else ('ZL', 'ZR') if kind == 'switch' else ('L2', 'R2')
    lb, rb = ('LB', 'RB') if kind.startswith('xbox') else ('L', 'R') if kind == 'switch' else ('L1', 'R1')
    if kind == 'generic':
        lt, rt, lb, rb = 'L2', 'R2', 'L1', 'R1'
    for side in (1, -1):
        def X(x):
            return x if side == 1 else 512 - x
        c.rrect(X(122), 34, X(198), 76, 16, fill=TRIGGER, rim=ACCENT, width=3)
        c.text(X(160), 52, lt if side == 1 else rt, 16)
        c.anchor('lt' if side == 1 else 'rt', X(160), 52)
        c.rrect(X(108), 70, X(214), 98, 12, fill=BUTTON, rim=BUTTON_RIM)
        c.text(X(161), 83, lb if side == 1 else rb, 14)
        c.anchor('lb' if side == 1 else 'rb', X(161), 84)
    round_ = kind in ('xbox360', 'generic')
    left = [(150, 98), (226, 104), (232, 270), (176, 340), (112, 354), (72, 316), (70, 220), (92, 128)]
    if round_:
        left = [(150, 96), (226, 102), (232, 262), (180, 330), (118, 350), (74, 318), (66, 224), (88, 130)]
    if kind == 'ps':
        left = [(150, 96), (226, 102), (232, 250), (170, 300), (124, 350), (84, 340), (66, 230), (90, 124)]
    right = [(512 - x, y) for x, y in reversed(left)]
    c.blob(left)
    c.blob(right)
    c.blob([(150, 96), (362, 96), (380, 160), (360, 262), (256, 274), (152, 262), (132, 160)], rim=BODY)
    c.line([(150, 96), (362, 96)], color=RIM, width=3)


def pad_xbox(c, kind):
    pad_body(c, kind)
    c.stick('ls', 166, 150, 28)
    c.dpad(('dup', 'ddown', 'dleft', 'dright'), 210, 226, 34, 22)
    c.stick('rs', 300, 226, 28)
    cols = {'a': (90, 170, 80, 255), 'b': (200, 70, 60, 255), 'x': (60, 110, 200, 255), 'y': (210, 180, 50, 255)}
    for k, (x, y) in {'a': (346, 182), 'b': (378, 150), 'x': (314, 150), 'y': (346, 118)}.items():
        c.button(k, x, y, 16, k.upper(), fill=cols[k], size=17)
    if kind == 'xbox360':
        c.button('back', 218, 150, 9, '\u25c0', size=9)
        c.button('start', 294, 150, 9, '\u25b6', size=9)
    else:
        c.button('back', 222, 150, 9, '\u29c9', size=10)
        c.button('start', 290, 150, 9, '\u2261', size=12)
    c.ellipse(256, 116, 15, 15, fill=BODY_DARK, rim=RIM)


def pad_switch(c):
    pad_body(c, 'switch')
    c.stick('ls', 166, 150, 28)
    c.dpad(('dup', 'ddown', 'dleft', 'dright'), 210, 226, 34, 22)
    c.stick('rs', 300, 226, 28)
    # Ironwail names the keys by Nintendo's labels: A on the right, B at the bottom, X at the top, Y on the left
    for k, (x, y), lab in (('a', (378, 150), 'A'), ('b', (346, 182), 'B'), ('x', (346, 118), 'X'), ('y', (314, 150), 'Y')):
        c.button(k, x, y, 16, lab, size=17)
    c.button('back', 224, 124, 8, '\u2212', size=12)
    c.button('start', 288, 124, 8, '+', size=12)
    c.ellipse(286, 168, 8, 8, fill=BODY_DARK)
    c.rrect(220, 162, 232, 174, 2, fill=BODY_DARK, rim=RIM, width=1.5)


def pad_ps(c, generic=False):
    pad_body(c, 'generic' if generic else 'ps')
    c.dpad(('dup', 'ddown', 'dleft', 'dright'), 158, 154, 34, 22)
    c.stick('ls', 204, 236, 26)
    c.stick('rs', 308, 236, 26)
    pos = {'a': (354, 186), 'b': (386, 154), 'x': (322, 154), 'y': (354, 122)}
    if generic:
        for k, (x, y) in pos.items():
            c.button(k, x, y, 16, {'a': '1', 'b': '2', 'x': '3', 'y': '4'}[k], size=16)
        c.button('back', 228, 160, 8, '', size=10)
        c.button('start', 284, 160, 8, '', size=10)
        return
    c.rrect(208, 100, 304, 168, 12, fill=BODY_DARK, rim=RIM)
    for k, (x, y) in pos.items():
        c.button(k, x, y, 16, '')
        cx = c.x(x) * SS
        cy = y * SS
        s = 7 * SS
        col = (230, 232, 236, 255)
        if k == 'a':  # cross
            c.d.line([(cx - s, cy - s), (cx + s, cy + s)], fill=col, width=2 * SS)
            c.d.line([(cx - s, cy + s), (cx + s, cy - s)], fill=col, width=2 * SS)
        elif k == 'b':  # circle
            c.d.ellipse([cx - s, cy - s, cx + s, cy + s], outline=col, width=2 * SS)
        elif k == 'x':  # square
            c.d.rectangle([cx - s * 0.8, cy - s * 0.8, cx + s * 0.8, cy + s * 0.8], outline=col, width=2 * SS)
        else:  # triangle
            c.d.polygon([(cx, cy - s), (cx + s, cy + s * 0.7), (cx - s, cy + s * 0.7)], outline=col, width=2 * SS)
    c.button('back', 194, 108, 7, '')
    c.button('start', 318, 108, 7, '')


# ---------------------------------------------------------------------------------------------------------------------
# The arts: each one's pictures, and its buttons' names and glyphs
# ---------------------------------------------------------------------------------------------------------------------

QUEST_GLYPHS = {
    'l': {'trigger': 'quest_trigger_left', 'grip': 'quest_grip_left', 'primary': 'quest_button_x',
          'secondary': 'quest_button_y', 'stick': 'quest_stick_l_press', 'menu': 'quest_button_menu'},
    'r': {'trigger': 'quest_trigger_right', 'grip': 'quest_grip_right', 'primary': 'quest_button_a',
          'secondary': 'quest_button_b', 'stick': 'quest_stick_r_press', 'stickup': 'quest_stick_r_up',
          'stickdown': 'quest_stick_r_down'},
}
TOUCH_NAMES = {'l': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'X', 'secondary': 'Y', 'stick': 'Stick click',
                     'menu': 'Menu'},
               'r': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'A', 'secondary': 'B', 'stick': 'Stick click',
                     'stickup': 'Stick up', 'stickdown': 'Stick down'}}

VR_ARTS = {
    'Quest2': (lambda c, h: face_touch(c, h, 'quest2'), TOUCH_NAMES, QUEST_GLYPHS),
    'Quest3': (lambda c, h: face_touch(c, h, 'quest3'), TOUCH_NAMES, QUEST_GLYPHS),
    'QuestPro': (lambda c, h: face_touch(c, h, 'questpro'), TOUCH_NAMES, QUEST_GLYPHS),
    'RiftCV1': (lambda c, h: face_touch(c, h, 'cv1'), TOUCH_NAMES, QUEST_GLYPHS),
    'Pico': (lambda c, h: face_touch(c, h, 'pico'), TOUCH_NAMES, QUEST_GLYPHS),
    'Index': (face_index,
              {'l': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'A', 'stick': 'Stick click', 'menu': 'B (menu)'},
               'r': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'A', 'secondary': 'B', 'stick': 'Stick click',
                     'stickup': 'Stick up', 'stickdown': 'Stick down'}},
              {'l': {'trigger': 'index_lt', 'primary': 'index_button_a', 'stick': 'index_stick_l_press',
                     'menu': 'index_button_b'},
               'r': {'trigger': 'index_rt', 'primary': 'index_button_a', 'secondary': 'index_button_b',
                     'stick': 'index_stick_r_press', 'stickup': 'index_stick_r_up', 'stickdown': 'index_stick_r_down'}}),
    'Frame': (lambda c, h: face_touch(c, h, 'frame'),
              {'l': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'D-pad down', 'secondary': 'D-pad up',
                     'stick': 'Stick click', 'menu': 'View'},
               'r': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'A', 'secondary': 'B', 'stick': 'Stick click',
                     'stickup': 'Stick up', 'stickdown': 'Stick down', 'menu': 'Menu'}},
              {'l': {'trigger': 'frame_button_l2', 'grip': 'frame_lg', 'primary': 'frame_dpad_down',
                     'secondary': 'frame_dpad_up', 'stick': 'frame_stick_l_press', 'menu': 'frame_button_view'},
               'r': {'trigger': 'frame_button_r2', 'grip': 'frame_rg', 'primary': 'frame_button_a',
                     'secondary': 'frame_button_b', 'stick': 'frame_stick_r_press', 'stickup': 'frame_stick_r_up',
                     'stickdown': 'frame_stick_r_down', 'menu': 'frame_button_options'}}),
    'Vive': (face_vive,
             {'l': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'Pad click', 'menu': 'Menu'},
              'r': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'Pad click', 'secondary': 'Menu button',
                    'stickup': 'Pad up', 'stickdown': 'Pad down'}},
             {}),
    'Wmr': (face_wmr,
            {'l': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'Pad click', 'stick': 'Stick click', 'menu': 'Menu'},
             'r': {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'Pad click', 'secondary': 'Menu button',
                   'stick': 'Stick click', 'stickup': 'Stick up', 'stickdown': 'Stick down'}},
            {}),
    'SimpleVr': (face_simple, {'l': {'trigger': 'Trigger', 'menu': 'Menu'}, 'r': {'trigger': 'Trigger'}}, {}),
}

PAD_KEYS = ['a', 'b', 'x', 'y', 'lb', 'rb', 'lt', 'rt', 'ls', 'rs', 'dup', 'ddown', 'dleft', 'dright', 'start', 'back']
XBOX_GLYPHS = {'a': 'xbox_button_color_a', 'b': 'xbox_button_color_b', 'x': 'xbox_button_color_x',
               'y': 'xbox_button_color_y', 'lb': 'xbox_lb', 'rb': 'xbox_rb', 'lt': 'xbox_lt', 'rt': 'xbox_rt',
               'ls': 'xbox_ls', 'rs': 'xbox_rs', 'dup': 'xbox_dpad_up', 'ddown': 'xbox_dpad_down',
               'dleft': 'xbox_dpad_left', 'dright': 'xbox_dpad_right'}
DPAD_NAMES = {'dup': 'D-pad up', 'ddown': 'D-pad down', 'dleft': 'D-pad left', 'dright': 'D-pad right'}
PAD_ARTS = {
    'Xbox360': (lambda c: pad_xbox(c, 'xbox360'),
                dict(a='A', b='B', x='X', y='Y', lb='LB', rb='RB', lt='LT', rt='RT', ls='LS click', rs='RS click',
                     start='Start', back='Back', **DPAD_NAMES),
                dict(XBOX_GLYPHS, start='xbox_button_start', back='xbox_button_back')),
    'XboxOne': (lambda c: pad_xbox(c, 'xboxone'),
                dict(a='A', b='B', x='X', y='Y', lb='LB', rb='RB', lt='LT', rt='RT', ls='LS click', rs='RS click',
                     start='Menu', back='View', **DPAD_NAMES),
                dict(XBOX_GLYPHS, start='xbox_button_menu', back='xbox_button_view')),
    'PlayStation': (pad_ps,
                    dict(a='Cross', b='Circle', x='Square', y='Triangle', lb='L1', rb='R1', lt='L2', rt='R2',
                         ls='L3', rs='R3', start='Options', back='Share', **DPAD_NAMES),
                    {}),
    'SwitchPro': (pad_switch,
                  dict(a='A', b='B', x='X', y='Y', lb='L', rb='R', lt='ZL', rt='ZR', ls='LS click', rs='RS click',
                       start='+', back='\u2212', **DPAD_NAMES),
                  {'a': 'switch_button_a', 'b': 'switch_button_b', 'x': 'switch_button_x', 'y': 'switch_button_y',
                   'lb': 'switch_button_l', 'rb': 'switch_button_r', 'lt': 'switch_button_zl',
                   'rt': 'switch_button_zr', 'ls': 'switch_stick_l_press', 'rs': 'switch_stick_r_press',
                   'dup': 'switch_dpad_up', 'ddown': 'switch_dpad_down', 'dleft': 'switch_dpad_left',
                   'dright': 'switch_dpad_right', 'start': 'switch_button_plus', 'back': 'switch_button_minus'}),
    'GenericPad': (lambda c: pad_ps(c, generic=True),
                   dict(a='1 (A)', b='2 (B)', x='3 (X)', y='4 (Y)', lb='L1', rb='R1', lt='L2', rt='R2', ls='L3',
                        rs='R3', start='Start', back='Select', **DPAD_NAMES),
                   {}),
}

# Drawn glyphs (no Kenney glyph for the button): a light pill with the button's short name, as Kenney's style.
SHORT = {'trigger': 'TRIG', 'grip': 'GRIP', 'primary': 'PAD', 'secondary': 'MENU', 'stick': 'STICK', 'menu': 'MENU',
         'stickup': 'UP', 'stickdown': 'DOWN'}


def drawn_glyph(path, text, shape=None):
    s = 64 * SS
    img = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    light, dark = (238, 238, 238, 255), (40, 40, 44, 255)
    if shape:
        d.ellipse([6 * SS, 6 * SS, 58 * SS, 58 * SS], fill=light, outline=dark, width=3 * SS)
        c, r = 32 * SS, 12 * SS
        if shape == 'cross':
            d.line([(c - r, c - r), (c + r, c + r)], fill=dark, width=5 * SS)
            d.line([(c - r, c + r), (c + r, c - r)], fill=dark, width=5 * SS)
        elif shape == 'circle':
            d.ellipse([c - r, c - r, c + r, c + r], outline=dark, width=5 * SS)
        elif shape == 'square':
            d.rectangle([c - r, c - r, c + r, c + r], outline=dark, width=5 * SS)
        else:
            d.polygon([(c, c - r * 1.1), (c + r * 1.1, c + r * 0.8), (c - r * 1.1, c + r * 0.8)], outline=dark,
                      width=5 * SS)
    else:
        d.rounded_rectangle([3 * SS, 14 * SS, 61 * SS, 50 * SS], radius=12 * SS, fill=light, outline=dark,
                            width=3 * SS)
        size = 22 if len(text) <= 2 else 17 if len(text) <= 4 else 13
        d.text((32 * SS, 32 * SS), text, fill=dark, font=font(size * SS), anchor='mm')
    img.resize((64, 64), Image.LANCZOS).save(path, optimize=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--kenney', help="Kenney's Input Prompts zip (CC0): its Default glyphs are copied")
    args = ap.parse_args()
    os.makedirs(OUT_PICS, exist_ok=True)
    os.makedirs(OUT_GLYPHS, exist_ok=True)

    rows = []  # (art, hand, button, u, v, name, glyph)
    used_kenney = set()
    for art, (draw, names, glyphs) in VR_ARTS.items():
        for hand in ('l', 'r'):
            c = Canvas(512, 512, mirror=hand == 'l')
            draw(c, hand)
            c.save(os.path.join(OUT_PICS, f'{art.lower()}_{hand}.png'))
            for btn, name in names[hand].items():
                key = 'stick' if btn in ('stickup', 'stickdown') else btn
                if key not in c.anchors:
                    raise SystemExit(f'{art} {hand}: no anchor for {btn}')
                u, v = c.anchors[key]
                g = glyphs.get(hand, {}).get(btn, '')
                if g:
                    used_kenney.add(g)
                else:
                    g = f'drawn_{SHORT[btn].lower()}'
                rows.append((art, 'Left' if hand == 'l' else 'Right', btn, u, v, name, g))
    for art, (draw, names, glyphs) in PAD_ARTS.items():
        c = Canvas(512, 384)
        draw(c)
        c.save(os.path.join(OUT_PICS, f'{art.lower()}.png'))
        for btn in PAD_KEYS:
            u, v = c.anchors[btn]
            g = glyphs.get(btn, '')
            if g:
                used_kenney.add(g)
            else:
                g = f'drawn_{art.lower()}_{btn}'
            rows.append((art, 'Pad', btn, u, v, names[btn], g))

    # glyphs: Kenney's, and the drawn ones
    if args.kenney:
        z = zipfile.ZipFile(args.kenney)
        index = {n.split('/')[-1][:-4]: n for n in z.namelist() if '/Default/' in n and n.endswith('.png')}
        for g in sorted(used_kenney):
            if g not in index:
                raise SystemExit(f'no Kenney glyph {g}')
            with open(os.path.join(OUT_GLYPHS, g + '.png'), 'wb') as f:
                f.write(z.read(index[g]))
        with open(os.path.join(OUT_GLYPHS, 'LICENSE.txt'), 'wb') as f:
            f.write(z.read('License.txt'))
    for name in set(SHORT.values()):
        drawn_glyph(os.path.join(OUT_GLYPHS, f'drawn_{name.lower()}.png'), name)
    ps_shapes = {'a': 'cross', 'b': 'circle', 'x': 'square', 'y': 'triangle'}
    for art, (draw, names, glyphs) in PAD_ARTS.items():
        for btn in PAD_KEYS:
            if btn in glyphs:
                continue
            label = names[btn].split(' ')[0]
            if btn.startswith('d'):
                label = {'dup': '\u2191', 'ddown': '\u2193', 'dleft': '\u2190', 'dright': '\u2192'}[btn]
            shape = ps_shapes.get(btn) if art == 'PlayStation' else None
            drawn_glyph(os.path.join(OUT_GLYPHS, f'drawn_{art.lower()}_{btn}.png'), label, shape)

    btn_enum = {'trigger': 'Trigger', 'grip': 'Grip', 'primary': 'Primary', 'secondary': 'Secondary', 'stick': 'Stick',
                'stickup': 'StickUp', 'stickdown': 'StickDown', 'menu': 'Menu', 'a': 'A', 'b': 'B', 'x': 'X', 'y': 'Y',
                'lb': 'LB', 'rb': 'RB', 'lt': 'LT', 'rt': 'RT', 'ls': 'LS', 'rs': 'RS', 'dup': 'DUp',
                'ddown': 'DDown', 'dleft': 'DLeft', 'dright': 'DRight', 'start': 'Start', 'back': 'Back'}
    with open(OUT_INC, 'w', encoding='utf-8', newline='\n') as f:
        f.write('// vr_bindsetup_art.inc -- generated by Misc/quakevr/make_controller_art.py: do not edit.\n')
        f.write('// Each art\'s buttons: the picture (hand), the button, its anchor on the picture (0..1), its name and '
                'its glyph (gfx/vr/glyphs).\n')
        for art, hand, btn, u, v, name, g in rows:
            esc = name.replace('\u2212', '-')
            f.write(f'{{Art::{art}, Pic::{hand}, Btn::{btn_enum[btn]}, {u:.4f}f, {v:.4f}f, "{esc}", "{g}"}},\n')
    print(f'{len(rows)} buttons, {len(VR_ARTS) * 2 + len(PAD_ARTS)} pictures, {len(used_kenney)} Kenney glyphs')


if __name__ == '__main__':
    main()
