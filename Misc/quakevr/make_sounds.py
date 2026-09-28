#!/usr/bin/env python3
# make_sounds.py -- generates Quake VR's own synthesised sounds (quakevr/sound/vr/):
#   headshot.wav  a headshot's confirmation (vr_headshot_sound): meaty -- a crack, a helmet dink, a low
#                 punch and a wet crunch of bone; short, so that it cuts through gunfire
#   shell_tink1..3.wav  a spent shotgun shell landing (vr_shells.cpp): a brass ring, a plastic tock and
#                 a softer second bounce
#   shell_plip1..3.wav  a spent shell dropping into water (vr_shells.cpp): the recorded plips (plip1, plip3,
#                 plip4, below) pitched up, for a smaller thing going in, their tails shortened
#   shove.wav, bash.wav, bash_parry.wav, parry.wav  melee contacts other than blows (vr_bash_sound): a
#                 shove (a whoosh into a thud), a weapon bash (a dull clang on a thud), a parry-bash (a
#                 scrape and ring over the bash) and a parry (a bright ring of steel)
#   counter_open.wav, counter.wav  counter-attacks (vr_counter_sound; QC vr_melee.qc): the window opening after a
#                 parry (a blade drawn off into a high ring) and a counter landing (a crack, a deep boom, a sting of
#                 steel), over the attack's own sound
#   dummy_windup.wav  the training dummy winding up a blow (vr_dummy_attacks; QC vr_dummy.qc): two clacks and a
#                 rising, growling swell, the tell before its blow
#   pommel1..3.wav  a pommel, a hilt or a gun's butt striking (QC vr_melee.qc VR_Melee_HitSound): a blunt knock,
#                 short and dry, apart from the blades' cuts and the punches; three, a little apart in pitch
#
# The water sounds in the same folder (splash_small*, splash_big*, splash_out*, plip*, slosh*, stroke*) are not
# made here: they are recordings (docs/vr-port/CREDITS.md), kept in the repository as they are.
#
# Usage: python Misc/quakevr/make_sounds.py [output sound folder [names of the headshot/melee sounds...]]

import math
import os
import random
import struct
import sys

RATE = 22050


def write_wav(path, samples):
    data = b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(data)) + data)


class OnePole:
    """A one-pole low-pass at `cutoff` Hz (high-pass: the input minus it)."""

    def __init__(self, cutoff):
        self.a = 1.0 - math.exp(-2 * math.pi * cutoff / RATE)
        self.y = 0.0

    def __call__(self, x):
        self.y += self.a * (x - self.y)
        return self.y


def headshot():
    """Meaty, in layers: a sharp crack, a short helmet dink (inharmonic partials, dropping a
    little in pitch), a low punch sweeping down, a mid thwack, and a wet crunch of bone (grains of
    band-passed noise) tailing off; soft-clipped together for weight."""
    rng = random.Random(7)
    n = int(RATE * 0.42)
    crack_hp = OnePole(2500)
    thwack_lp, thwack_hp = OnePole(900), OnePole(250)
    crunch_lp, crunch_hp = OnePole(2200), OnePole(500)
    wet_lp = OnePole(700)
    grain = 0.0
    phase_punch = 0.0
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)

        # The crack: 3 ms of bright noise.
        crack = (noise - crack_hp(noise)) * math.exp(-t / 0.003)

        # The dink: a struck helmet, partials that die fast (the high ones faster).
        drop = 1.0 - 0.06 * min(1.0, t / 0.08)
        dink = 0.0
        for f, amp, decay in ((1870, 0.55, 0.050), (2960, 0.45, 0.040), (4630, 0.30, 0.028), (6310, 0.18, 0.018)):
            dink += amp * math.sin(2 * math.pi * f * drop * t) * math.exp(-t / decay)
        dink *= min(1.0, t / 0.0008)

        # The punch: a sine sweeping 130 -> 50 Hz.
        phase_punch += 2 * math.pi * (50 + 80 * math.exp(-t / 0.03)) / RATE
        punch = math.sin(phase_punch) * math.exp(-t / 0.09) * min(1.0, t / 0.002)

        # The thwack: mid-band noise, 60 ms.
        thwack = thwack_hp(thwack_lp(noise)) * math.exp(-t / 0.035)

        # The crunch: sparse grains of band-passed noise, thinning out.
        if rng.random() < 0.035 * math.exp(-t / 0.08):
            grain = rng.uniform(0.5, 1.0) * (1 if rng.random() < 0.5 else -1)
        grain *= 0.93
        crunch_in = grain + noise * 0.15
        crunch = (crunch_lp(crunch_in) - crunch_hp(crunch_lp.y)) * math.exp(-t / 0.11) * min(1.0, t / 0.006)

        # The wet tail: low noise, swelling slightly after the hit.
        wet = wet_lp(noise) * (t / 0.02) * math.exp(-t / 0.05) * 0.6 if t < 0.3 else 0.0

        s = crack * 0.9 + dink * 0.5 + punch * 1.1 + thwack * 1.6 + crunch * 1.4 + wet * 1.2
        out.append(math.tanh(s * 1.6))
    peak = max(abs(s) for s in out)
    fade = int(RATE * 0.02)
    return [s * 0.97 / peak * (min(1.0, (n - i) / fade)) for i, s in enumerate(out)]


def shell_tink(pitch, seed):
    """A spent shotgun shell dropping on stone: the brass head's bright ring (inharmonic partials,
    the high ones dying first), a hollow plastic tock of the hull and a tiny click, with a second,
    softer bounce 60-90 ms later. Quiet and short: vr_shells.cpp plays it at the shell."""
    rng = random.Random(seed)
    n = int(RATE * 0.26)
    out = [0.0] * n
    second = rng.uniform(0.06, 0.09)
    for at, level in ((0.0, 1.0), (second, rng.uniform(0.3, 0.45))):
        click_hp = OnePole(3000)
        tock_lp, tock_hp = OnePole(1600), OnePole(500)
        for i in range(int(RATE * 0.16)):
            j = int(at * RATE) + i
            if j >= n:
                break
            t = i / RATE
            noise = rng.uniform(-1, 1)
            click = (noise - click_hp(noise)) * math.exp(-t / 0.0007)
            ring = 0.0
            for f, amp, decay in ((3350, 0.5, 0.045), (5230, 0.35, 0.03), (7480, 0.22, 0.018), (9910, 0.12, 0.01)):
                ring += amp * math.sin(2 * math.pi * f * pitch * t + f) * math.exp(-t / decay)
            ring *= min(1.0, t / 0.0004)
            tock = tock_hp(tock_lp(noise)) * math.exp(-t / 0.008) + \
                math.sin(2 * math.pi * 1150 * pitch * t) * math.exp(-t / 0.012) * 0.5
            out[j] += level * (click * 0.5 + ring * 0.55 + tock * 0.9)
    peak = max(abs(s) for s in out)
    fade = int(RATE * 0.02)
    return [s * 0.9 / peak * min(1.0, (n - i) / fade) for i, s in enumerate(out)]


# ---- Shared ---------------------------------------------------------------------------------


class VarLowPass:
    """A one-pole low-pass whose cutoff may change every sample."""

    def __init__(self):
        self.y = 0.0

    def __call__(self, x, cutoff):
        a = 1.0 - math.exp(-2 * math.pi * max(cutoff, 20.0) / RATE)
        self.y += a * (x - self.y)
        return self.y


def finish(out, peak_to=0.9, fade=0.02):
    """Normalized to `peak_to`, faded out over the last `fade` seconds."""
    peak = max(abs(s) for s in out) or 1.0
    n = len(out)
    f = int(RATE * fade)
    return [s * peak_to / peak * min(1.0, (n - i) / f) for i, s in enumerate(out)]


# ---- Melee feedback (QC vr_juice.qc VR_Bash, combat.qc VR_Parry; docs/vr-port/ROUND18.md) -------------
# Each kind of contact has its own sound, apart from the blows' (fist and weapon hits keep Quake's):
# a shove is flesh and air (a whoosh into a heavy thud), a weapon bash is metal driven into a body (a
# dull clang on a thud), a parry is steel on steel (a bright ring), and a parry-bash (a bash right
# after a parry) is both: the blade's scrape and ring over the bash's clang and thud.


def partials(t, pitch, table):
    """A struck metal body: inharmonic sine partials (f, amp, decay), the high ones dying first."""
    return sum(a * math.sin(2 * math.pi * f * pitch * t + f) * math.exp(-t / d) for f, a, d in table)


def thud(t, phase, f0=45.0, f1=70.0, tau=0.13):
    """A heavy low body hit: a sine sweeping from f0 + f1 Hz down to f0 (the phase is carried)."""
    phase[0] += 2 * math.pi * (f0 + f1 * math.exp(-t / 0.035)) / RATE
    return math.sin(phase[0]) * math.exp(-t / tau) * min(1.0, t / 0.002)


def shove():
    """Both hands (or an open palm) shoving a body: a short whoosh of air rising into the push, a
    heavy low thud, and the slap of cloth and flesh (mid noise). No metal."""
    rng = random.Random(83)
    n = int(RATE * 0.55)
    hit = 0.075  # the whoosh leads into the contact
    air = VarLowPass()
    air_hp = OnePole(250)
    slap_lp, slap_hp = OnePole(2600), OnePole(400)
    body_lp = OnePole(260)
    phase = [0.0]
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        swell = (t / hit) ** 2 if t < hit else math.exp(-(t - hit) / 0.06)
        w = air(noise, 400 + 1600 * swell)
        w -= air_hp(w)
        whoosh = w * swell * 2.2
        s = whoosh
        if t >= hit:
            u = t - hit
            slap = (slap_lp(noise) - slap_hp(slap_lp.y)) * math.exp(-u / 0.025) * min(1.0, u / 0.001)
            body = body_lp(noise) * math.exp(-u / 0.09) * 2.5
            s += thud(u, phase, 42, 75, 0.14) * 1.3 + slap * 1.1 + body
        out.append(math.tanh(s * 1.5))
    return finish(out, 0.92)


def bash_weapon():
    """A weapon's guard driven into a body: a short whoosh, a dull clang (low, heavy metal partials
    that die fast: a gun's body or a blade's flat, not a ring) on a heavy thud."""
    rng = random.Random(89)
    n = int(RATE * 0.55)
    hit = 0.05
    air = VarLowPass()
    air_hp = OnePole(300)
    click_hp = OnePole(2500)
    body_lp = OnePole(300)
    phase = [0.0]
    table = ((310, 0.55, 0.12), (740, 0.5, 0.09), (1290, 0.4, 0.06), (1980, 0.3, 0.04), (2870, 0.2, 0.025))
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        swell = (t / hit) ** 2 if t < hit else math.exp(-(t - hit) / 0.04)
        w = air(noise, 500 + 1500 * swell)
        w -= air_hp(w)
        s = w * swell * 1.4
        if t >= hit:
            u = t - hit
            click = (noise - click_hp(noise)) * math.exp(-u / 0.002)
            clang = partials(u, 1.0, table) * min(1.0, u / 0.0005)
            s += click * 0.7 + clang * 0.9 + thud(u, phase, 48, 80, 0.12) * 1.2 + body_lp(noise) * math.exp(-u / 0.07) * 1.8
        out.append(math.tanh(s * 1.4))
    return finish(out, 0.92)


def parry():
    """A blow caught on the blade: a sharp strike, a bright ring of steel (high inharmonic partials,
    long), and a short scrape of edge on edge."""
    rng = random.Random(97)
    n = int(RATE * 0.8)
    click_hp = OnePole(3500)
    scrape = VarLowPass()
    scrape_hp = OnePole(1800)
    table = ((1480, 0.45, 0.30), (2310, 0.55, 0.26), (3390, 0.45, 0.18), (4870, 0.3, 0.12), (6620, 0.2, 0.07),
             (8150, 0.1, 0.04))
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        click = (noise - click_hp(noise)) * math.exp(-t / 0.0025)
        ring = partials(t, 1.0 - 0.01 * min(1.0, t / 0.3), table) * min(1.0, t / 0.0004)
        sc = scrape(noise, 2500 + 4000 * min(1.0, t / 0.08))
        sc -= scrape_hp(sc)
        sc *= math.exp(-t / 0.05) * (0.6 + 0.4 * math.sin(2 * math.pi * 55 * t))
        out.append(click * 0.9 + ring * 0.75 + sc * 1.2)
    return finish([math.tanh(s * 1.2) for s in out], 0.9)


def bash_parry():
    """A parry-bash (a bash right after a parry): the blade's scrape rising and its bright ring, over
    the bash's clang and a heavier thud."""
    rng = random.Random(101)
    n = int(RATE * 0.8)
    hit = 0.06
    shing = VarLowPass()
    shing_hp = OnePole(2000)
    click_hp = OnePole(3000)
    body_lp = OnePole(280)
    phase = [0.0]
    ring_table = ((1720, 0.4, 0.28), (2650, 0.5, 0.22), (3980, 0.35, 0.15), (5710, 0.2, 0.09))
    clang_table = ((330, 0.5, 0.13), (790, 0.45, 0.1), (1370, 0.35, 0.07), (2090, 0.25, 0.045))
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        # The scrape: band noise sweeping up into the hit.
        sw = min(1.0, t / hit)
        sh = shing(noise, 2200 + 5000 * sw)
        sh -= shing_hp(sh)
        s = sh * (sw ** 2 if t < hit else math.exp(-(t - hit) / 0.03)) * 1.3
        if t >= hit:
            u = t - hit
            click = (noise - click_hp(noise)) * math.exp(-u / 0.002)
            ring = partials(u, 1.0, ring_table) * min(1.0, u / 0.0004)
            clang = partials(u, 1.0, clang_table) * min(1.0, u / 0.0005)
            s += click * 0.8 + ring * 0.6 + clang * 0.7 + thud(u, phase, 44, 85, 0.15) * 1.3 + \
                body_lp(noise) * math.exp(-u / 0.08) * 2.0
        out.append(math.tanh(s * 1.4))
    return finish(out, 0.92)


def counter_open():
    """A counter's window opening (a parry just caught a blow; QC vr_melee.qc VR_Counter_Open): after the parry's
    strike (a short silence: it follows the parry's ring, it doesn't mask it), the blade drawn off the enemy's -- a
    scrape rising fast -- into a clean, high ring gliding up a little: steel ready to strike back. Lighter than the
    parry, and nothing low (no hit)."""
    rng = random.Random(137)
    lead = 0.07  # after the parry's strike
    rise = 0.11  # the scrape, rising into the ring
    n = int(RATE * 0.6)
    scrape = VarLowPass()
    scrape_hp = OnePole(2200)
    table = ((2630, 0.5, 0.22), (3940, 0.45, 0.17), (5310, 0.3, 0.11), (7020, 0.15, 0.06))
    out = []
    for i in range(n):
        t = i / RATE - lead
        if t < 0:
            out.append(0.0)
            continue
        noise = rng.uniform(-1, 1)
        sw = min(1.0, t / rise)
        sc = scrape(noise, 2500 + 6000 * sw)
        sc -= scrape_hp(sc)
        s = sc * (sw ** 1.5 if t < rise else math.exp(-(t - rise) / 0.025)) * 1.1
        if t >= rise:
            u = t - rise
            ring = partials(u, 1.0 + 0.025 * min(1.0, u / 0.2), table) * min(1.0, u / 0.003)
            s += ring * 0.8
        out.append(s)
    return finish([math.tanh(s * 1.2) for s in out], 0.8)


def counter_hit():
    """A counter landing (a blow, a bash or a shove in the window after a parry; QC vr_melee.qc VR_Counter_Use),
    over the attack's own sound: a hard crack, a deep boom under it (lower and longer than the bash's thud) and a
    short bright sting of steel on top -- the heaviest hit there is, and the only one with both the boom and the
    ring."""
    rng = random.Random(139)
    n = int(RATE * 0.7)
    crack_hp = OnePole(1500)
    crack_lp = OnePole(6000)
    body_lp = OnePole(220)
    mid_lp, mid_hp = OnePole(1300), OnePole(300)
    phase = [0.0]
    sting = ((1830, 0.45, 0.16), (2760, 0.5, 0.12), (4150, 0.35, 0.08), (5930, 0.2, 0.05))
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        crack = (crack_lp(noise) - crack_hp(crack_lp.y)) * math.exp(-t / 0.006)
        boom = thud(t, phase, 36, 95, 0.22)
        body = body_lp(noise) * math.exp(-t / 0.11)
        smack = (mid_lp(noise) - mid_hp(mid_lp.y)) * math.exp(-t / 0.04)  # the flesh and the weapon's body
        ring = partials(t, 1.0, sting) * min(1.0, t / 0.0005)
        out.append(math.tanh((crack * 1.4 + boom * 0.6 + body * 1.0 + smack * 6.0 + ring * 1.2) * 1.4))
    return finish(out, 0.95)


def dummy_windup():
    """The training dummy winding up a blow (QC vr_dummy.qc, vr_dummy_attacks): the tell, as its wind-up begins. Two
    hard clacks (the rifle taken up: wood and a little metal), then a rising, growling swell -- a low buzz gliding up an
    octave and a half under a rush of air -- that breaks off just before the blow. Nothing like the parry's or the
    counter's rings (no clean high partials): it says "now", and "coming", in 0.45 s."""
    rng = random.Random(151)
    n = int(RATE * 0.45)
    clacks = (0.0, 0.065)
    knock = ((420, 0.6, 0.03), (1130, 0.45, 0.018), (2350, 0.3, 0.01))
    click_hp = OnePole(1800)
    air = VarLowPass()
    air_hp = OnePole(350)
    buzz_lp = VarLowPass()
    phase = 0.0
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        s = 0.0
        for k, c in enumerate(clacks):
            if t >= c:
                u = t - c
                click = (noise - click_hp(noise)) * math.exp(-u / 0.003)
                s += (click * 0.8 + partials(u, 1.0 - 0.12 * k, knock) * min(1.0, u / 0.0005)) * (1.0 - 0.25 * k)
        # The swell: from 0.08 s, rising to the end, then cut (a 15 ms fade).
        rise = max(0.0, min(1.0, (t - 0.08) / 0.34))
        if rise > 0.0:
            f = 70.0 * 2 ** (1.5 * rise)  # 70 Hz up an octave and a half
            phase += 2 * math.pi * f / RATE
            saw = 2.0 * ((phase / (2 * math.pi)) % 1.0) - 1.0
            buzz = buzz_lp(saw, 300 + 1500 * rise) * 0.9
            w = air(noise, 500 + 3500 * rise)
            w -= air_hp(w)
            s += (buzz + w * 1.6) * rise ** 1.6
        out.append(math.tanh(s * 1.6))
    return finish(out, 0.9, 0.015)


def pommel(pitch, seed):
    """A pommel, a hilt or a gun's butt knocked into a body: blunt, short and dry. A hard tick, a wooden knock (low
    inharmonic modes of a dense knob damped in a few tens of ms: no ring, unlike the bash's clang or the parry's
    steel), a crunch of flesh and a short low thump; no whoosh (the shove's and the bash's), tighter than a punch."""
    rng = random.Random(seed)
    n = int(RATE * 0.3)
    click_hp = OnePole(2200)
    crunch_lp, crunch_hp = OnePole(1500), OnePole(180)
    body_lp = OnePole(330)
    phase = [0.0]
    table = ((290, 0.8, 0.03), (640, 0.7, 0.02), (1090, 0.45, 0.012), (1650, 0.25, 0.007))
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        click = (noise - click_hp(noise)) * math.exp(-t / 0.0015)
        knock = partials(t, pitch, table) * min(1.0, t / 0.0004)
        crunch = crunch_lp(noise)
        crunch -= crunch_hp(crunch)
        crunch *= math.exp(-t / 0.02)
        body = body_lp(noise) * math.exp(-t / 0.045)
        s = click * 0.6 + knock * 1.6 + crunch * 1.1 + thud(t, phase, 58 * pitch, 95 * pitch, 0.05) * 0.6 + body * 0.8
        out.append(math.tanh(s * 1.6))
    return finish(out, 0.92)


def read_wav(path):
    """A 16-bit mono WAV's samples, -1..1."""
    with open(path, "rb") as f:
        data = f.read()
    at = 12
    while at + 8 <= len(data):
        tag, size = data[at:at + 4], struct.unpack("<I", data[at + 4:at + 8])[0]
        if tag == b"data":
            raw = data[at + 8:at + 8 + size]
            return [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
        at += 8 + size + (size & 1)
    raise ValueError("no data in " + path)


def pitched(samples, factor, taps=16):
    """`samples` played `factor` times faster (higher and shorter), band-limited: a Hann-windowed sinc low-pass at
    the new Nyquist, so nothing folds back."""
    cutoff = min(1.0, 1.0 / factor) * 0.95
    out = []
    n = len(samples)
    for j in range(int(n / factor)):
        t = j * factor
        k0 = int(t)
        acc = 0.0
        for k in range(k0 - taps * int(math.ceil(factor)) + 1, k0 + taps * int(math.ceil(factor)) + 1):
            if 0 <= k < n:
                x = (t - k) * cutoff
                w = 0.5 + 0.5 * math.cos(math.pi * (t - k) / (taps * math.ceil(factor)))
                acc += samples[k] * cutoff * (math.sin(math.pi * x) / (math.pi * x) if x else 1.0) * w
        out.append(acc)
    return out


def shell_plip(source, factor, length):
    """A recorded plip pitched up by `factor` (a spent shell is smaller than a stone or a shot), cut to `length`
    seconds with a 40 ms fade."""
    s = pitched(read_wav(source), factor)[:int(RATE * length)]
    fade = int(RATE * 0.04)
    n = len(s)
    return [v * min(1.0, (n - i) / fade) for i, v in enumerate(s)]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "sound", "vr")
    os.makedirs(out, exist_ok=True)
    sounds = {
        "headshot.wav": headshot,
        "shove.wav": shove,
        "bash.wav": bash_weapon,
        "bash_parry.wav": bash_parry,
        "parry.wav": parry,
        "counter_open.wav": counter_open,
        "counter.wav": counter_hit,
        "dummy_windup.wav": dummy_windup,
    }
    only = sys.argv[2:]  # optional: just these
    for name, make in sounds.items():
        if not only or name in only:
            write_wav(os.path.join(out, name), make())
            print(name + " -> " + os.path.normpath(out))
    # Spent shells landing (vr_shells.cpp): three, a little apart in pitch.
    for k, pitch in enumerate((1.0, 0.92, 1.09)):
        name = "shell_tink%d.wav" % (k + 1)
        write_wav(os.path.join(out, name), shell_tink(pitch, 31 + k))
        print(name + " -> " + os.path.normpath(out))
    # Pommel, hilt and butt strikes (QC vr_melee.qc): three, a little apart in pitch, picked at random.
    for k, pitch in enumerate((1.0, 0.89, 1.12)):
        name = "pommel%d.wav" % (k + 1)
        write_wav(os.path.join(out, name), pommel(pitch, 131 + k))
        print(name + " -> " + os.path.normpath(out))
    # And dropping into water (vr_shells.cpp): the recordings' plips higher, from the folder they are in.
    for k, (plip, factor) in enumerate((("plip1.wav", 1.5), ("plip3.wav", 1.4), ("plip4.wav", 1.65))):
        name = "shell_plip%d.wav" % (k + 1)
        write_wav(os.path.join(out, name), shell_plip(os.path.join(out, plip), factor, 0.2))
        print(name + " -> " + os.path.normpath(out))


if __name__ == "__main__":
    main()
