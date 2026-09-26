#!/usr/bin/env python3
# make_sounds.py -- generates Quake VR's own synthesised sounds (quakevr/sound/vr/):
#   headshot.wav  a headshot's confirmation (vr_headshot_sound): meaty -- a crack, a helmet dink, a low
#                 punch and a wet crunch of bone; short, so that it cuts through gunfire
#   shell_tink1..3.wav  a spent shotgun shell landing (vr_shells.cpp): a brass ring, a plastic tock and
#                 a softer second bounce
#   splash_small.wav, splash_big.wav  things (and hands) going into water, by how hard (vr_physics.cpp)
#   plip.wav      a shot into water
#   slosh1/2.wav  wading
#   stroke1/2.wav swimming strokes
#   shove.wav, bash.wav, bash_parry.wav, parry.wav  melee contacts other than blows (vr_bash_sound): a
#                 shove (a whoosh into a thud), a weapon bash (a dull clang on a thud), a parry-bash (a
#                 scrape and ring over the bash) and a parry (a bright ring of steel)
#
# Usage: python Misc/quakevr/make_sounds.py [output sound folder [names of the water/headshot sounds...]]

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


# ---- Water (vr_physics.cpp: splashes, shots into water, wading, swimming strokes) -----------------
# Water is noise (the splash's crash and hiss) and bubbles: a bubble rings as a sine that rises in
# pitch as it dies (Minnaert resonance, the bubble shrinking near the surface), small ones high and
# short, big ones low and longer; drops falling back are tiny, high bubbles.


class Bubble:
    """A bubble's ring: `f` Hz rising by `rise` (a fraction) as it decays over `tau` seconds."""

    def __init__(self, start, f, tau, amp, rise=0.35):
        self.start, self.f, self.tau, self.amp, self.rise = start, f, tau, amp, rise
        self.phase = 0.0

    def __call__(self, t):
        t -= self.start
        if t < 0 or t > self.tau * 6:
            return 0.0
        f = self.f * (1 + self.rise * (1 - math.exp(-t / (self.tau * 0.7))))
        self.phase += 2 * math.pi * f / RATE
        return self.amp * math.sin(self.phase) * math.exp(-t / self.tau) * min(1.0, t / 0.0015)


def bubbles(rng, count, t0, t1, f0, f1, tau0, tau1, amp0, amp1, rise=0.35):
    """`count` bubbles starting between t0 and t1 s, later ones quieter."""
    out = []
    for _ in range(count):
        start = rng.uniform(t0, t1)
        late = (start - t0) / max(t1 - t0, 1e-6)
        out.append(Bubble(start, rng.uniform(f0, f1), rng.uniform(tau0, tau1),
                          rng.uniform(amp0, amp1) * (1 - 0.6 * late) * (1 if rng.random() < 0.5 else -1), rise))
    return out


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


def splash_small(seed=11):
    """A hand slapping the water, a thrown thing going in: a wet slap (bright noise, 40 ms), a short
    body of lower noise, a handful of bubbles ringing up, and drops falling back."""
    rng = random.Random(seed)
    n = int(RATE * 0.5)
    slap_lp, slap_hp = OnePole(5500), OnePole(500)
    body_lp = OnePole(1800)
    rings = bubbles(rng, 9, 0.004, 0.16, 450, 1600, 0.015, 0.045, 0.25, 0.6)
    rings += bubbles(rng, 7, 0.14, 0.42, 1700, 3400, 0.006, 0.016, 0.1, 0.25, rise=0.2)  # drops falling back
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        slap = (slap_lp(noise) - slap_hp(slap_lp.y)) * min(1.0, t / 0.002) * math.exp(-t / 0.04)
        body = body_lp(noise) * (t / 0.02) * math.exp(-t / 0.07) * 0.9
        out.append(slap * 1.2 + body + sum(b(t) for b in rings))
    return finish([math.tanh(s * 1.3) for s in out])


def splash_big(seed=23):
    """A body or a heavy thing going in: a low thump, a crash of noise that darkens as it falls
    away, many bubbles, and a rain of drops falling back."""
    rng = random.Random(seed)
    n = int(RATE * 1.2)
    crash = VarLowPass()
    crash_hp = OnePole(250)
    thump_lp = OnePole(350)
    rings = bubbles(rng, 26, 0.01, 0.5, 250, 1300, 0.02, 0.07, 0.25, 0.55)
    rings += bubbles(rng, 34, 0.25, 1.05, 1300, 3800, 0.005, 0.018, 0.08, 0.22, rise=0.2)
    phase = 0.0
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        phase += 2 * math.pi * (48 + 80 * math.exp(-t / 0.04)) / RATE
        thump = (math.sin(phase) * 0.9 + thump_lp(noise) * 2.0) * min(1.0, t / 0.003) * math.exp(-t / 0.11)
        cutoff = 1100 + 6500 * math.exp(-t / 0.18)
        c = crash(noise, cutoff)
        c -= crash_hp(c)
        c *= min(1.0, t / 0.006) * (0.55 * math.exp(-t / 0.08) + 0.45 * math.exp(-t / 0.35))
        out.append(thump * 1.1 + c * 1.6 + sum(b(t) for b in rings))
    return finish([math.tanh(s * 1.2) for s in out])


def plip():
    """A bullet into water: a tiny tick, one bright bubble ringing up, and a couple of drops."""
    rng = random.Random(5)
    n = int(RATE * 0.24)
    tick_hp = OnePole(3000)
    rings = [Bubble(0.002, 1250, 0.03, 0.9, rise=0.55)]
    rings += bubbles(rng, 3, 0.07, 0.17, 2200, 3600, 0.005, 0.012, 0.12, 0.25, rise=0.2)
    out = []
    for i in range(n):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        tick = (noise - tick_hp(noise)) * math.exp(-t / 0.0025)
        hiss = (noise - tick_hp.y) * math.exp(-t / 0.02) * 0.15
        out.append(tick * 0.8 + hiss + sum(b(t) for b in rings))
    return finish(out, 0.85)


def slosh(seed):
    """Wading, a leg pushing through: a soft swell of low, darkened noise (opening and closing as
    the water moves), a low gurgle, and a few drops."""
    rng = random.Random(seed)
    n = int(RATE * 0.62)
    lp = VarLowPass()
    hp = OnePole(120)
    rise = rng.uniform(0.1, 0.16)
    rings = bubbles(rng, 6, 0.05, 0.35, 160, 520, 0.03, 0.08, 0.15, 0.35, rise=0.25)
    rings += bubbles(rng, 3, 0.3, 0.55, 1500, 2600, 0.006, 0.014, 0.05, 0.12, rise=0.2)
    wobble = rng.uniform(5, 8)
    out = []
    for i in range(n):
        t = i / RATE
        swell = math.sin(0.5 * math.pi * min(1.0, t / rise)) ** 2 * math.exp(-max(0.0, t - rise) / 0.17)
        cutoff = 300 + 1500 * swell * (0.8 + 0.2 * math.sin(2 * math.pi * wobble * t))
        s = lp(rng.uniform(-1, 1), cutoff)
        s -= hp(s)
        out.append(s * swell * 3.0 + sum(b(t) for b in rings))
    return finish(out, 0.8)


def stroke(seed):
    """A swimming stroke: a whoosh of water pushed (band noise swelling and falling, rippling with
    turbulence) full of bubbles."""
    rng = random.Random(seed)
    n = int(RATE * 0.75)
    lp = VarLowPass()
    hp = OnePole(200)
    rise = rng.uniform(0.12, 0.2)
    rings = bubbles(rng, 16, 0.03, 0.55, 280, 950, 0.015, 0.05, 0.12, 0.35, rise=0.4)
    rate = rng.uniform(16, 24)
    out = []
    for i in range(n):
        t = i / RATE
        swell = math.sin(0.5 * math.pi * min(1.0, t / rise)) ** 2 * math.exp(-max(0.0, t - rise) / 0.2)
        turb = 0.7 + 0.3 * math.sin(2 * math.pi * rate * t + 2 * math.sin(2 * math.pi * 3.1 * t))
        s = lp(rng.uniform(-1, 1), 500 + 1400 * swell)
        s -= hp(s)
        out.append(s * swell * turb * 2.6 + sum(b(t) for b in rings) * (0.4 + 0.6 * swell))
    return finish(out, 0.8)


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


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "sound", "vr")
    os.makedirs(out, exist_ok=True)
    sounds = {
        "headshot.wav": headshot,
        "splash_small.wav": splash_small,
        "splash_big.wav": splash_big,
        "plip.wav": plip,
        "slosh1.wav": lambda: slosh(31),
        "slosh2.wav": lambda: slosh(47),
        "stroke1.wav": lambda: stroke(53),
        "stroke2.wav": lambda: stroke(71),
        "shove.wav": shove,
        "bash.wav": bash_weapon,
        "bash_parry.wav": bash_parry,
        "parry.wav": parry,
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


if __name__ == "__main__":
    main()
