#!/usr/bin/env python3
# make_gadget_sounds.py -- the wrist gadget's side button's clicks (the gear lights: vr_gearlights.cpp):
#   quakevr/sound/vr/gadget_click_on.wav, gadget_click_off.wav
# A small tactile button: make_flashlight.py's switch click, higher and lighter (a smaller button), the "on" a little
# brighter than the "off".
#
# Usage: python make_gadget_sounds.py [<game dir>]   (default: ../../quakevr)

import os
import sys

import genguard
import make_flashlight as fl


def main(args):
    here = os.path.dirname(os.path.abspath(__file__))
    game = args[0] if args else os.path.join(here, "..", "..", "quakevr")
    sounds = os.path.join(game, "sound", "vr")
    names = (("gadget_click_on", 1.55, 21, 0.8), ("gadget_click_off", 1.35, 22, 0.7))
    guard = genguard.Guard("make_gadget_sounds.py", [os.path.join(sounds, n + ".wav") for n, _, _, _ in names])
    os.makedirs(sounds, exist_ok=True)
    for name, pitch, seed, level in names:
        wav = os.path.join(sounds, name + ".wav")
        if wav in guard.kept:
            continue
        fl.write_wav(wav, [x * level for x in fl.click(pitch, seed)])
        print("%s.wav -> %s" % (name, os.path.normpath(wav)))
    guard.finish()


if __name__ == "__main__":
    main(sys.argv[1:])
