#!/usr/bin/env python3
# transcribe_notes.py -- turns Quake VR's voice notes (vr_notes: off hand at the mouth, hold Y) into
# text, and collects them into one file to read or hand over as feedback.
#
# Each note is quakevr/notes/<map>_<date>_<time>.wav with a .txt of its context beside it. This
# transcribes every note that has no transcript yet (<note>.transcript.txt) with Whisper, locally
# (faster-whisper; https://github.com/SYSTRAN/faster-whisper, MIT; the model is downloaded once
# from Hugging Face into your user cache), and writes quakevr/notes/NOTES.md: the notes in order,
# each with its context, transcript and the screenshot taken as it started (screenshots/, same
# name).
#
# Usage:
#   python Misc/quakevr/transcribe_notes.py [--notes quakevr/notes] [--model large-v3-turbo]
#       [--device cpu|cuda|auto] [--again]
#
# Setup (once): python -m pip install --user faster-whisper

import argparse
import glob
import os
import sys
import time

# Words Whisper would otherwise mishear: the game's and the port's terms.
VOCABULARY = ("Quake VR playtest notes. Quake, Ironwail, cvar, hitbox, headshot, parry, bash, headbutt, "
              "holster, force grab, gib, grunt, knight, ogre, shambler, fiend, scrag, zombie, shotgun, "
              "super shotgun, nailgun, rocket launcher, grenade launcher, thunderbolt, Mjolnir, axe, sword, "
              "ammo box, health box, bloom, normal maps, relit, e1m1, vrfiringrange, Quest, Virtual Desktop.")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description="Transcribe Quake VR voice notes")
    parser.add_argument("--notes", default=os.path.normpath(os.path.join(here, "..", "..", "quakevr", "notes")))
    parser.add_argument("--model", default="large-v3-turbo")
    parser.add_argument("--device", default="auto", help="cpu, cuda or auto")
    parser.add_argument("--again", action="store_true", help="transcribe notes that already have a transcript")
    args = parser.parse_args()

    notes = sorted(glob.glob(os.path.join(args.notes, "*.wav")))
    if not notes:
        print("no notes in %s" % args.notes)
        return 0

    todo = [n for n in notes if args.again or not os.path.isfile(n[:-4] + ".transcript.txt")]
    if todo:
        from faster_whisper import WhisperModel

        compute = "int8" if args.device == "cpu" else "default"
        print("loading %s (the first time downloads it) ..." % args.model, flush=True)
        model = WhisperModel(args.model, device=args.device, compute_type=compute)
        for n in todo:
            start = time.time()
            segments, info = model.transcribe(n, language="en", initial_prompt=VOCABULARY, vad_filter=True,
                                              beam_size=5)
            text = " ".join(s.text.strip() for s in segments).strip()
            with open(n[:-4] + ".transcript.txt", "w", encoding="utf-8") as f:
                f.write(text + "\n")
            print("%s (%.1f s of audio, %.1f s): %s" % (os.path.basename(n), info.duration, time.time() - start,
                                                        text or "(nothing heard)"), flush=True)

    # NOTES.md: every note, oldest first.
    shots = os.path.join(os.path.dirname(os.path.normpath(args.notes)), "screenshots")
    lines = ["# Voice notes", ""]
    for n in notes:
        base = os.path.basename(n)[:-4]
        context = ""
        if os.path.isfile(n[:-4] + ".txt"):
            with open(n[:-4] + ".txt", encoding="utf-8", errors="replace") as f:
                context = f.read().strip()
        transcript = ""
        if os.path.isfile(n[:-4] + ".transcript.txt"):
            with open(n[:-4] + ".transcript.txt", encoding="utf-8") as f:
                transcript = f.read().strip()
        shot = next((p for p in sorted(glob.glob(os.path.join(shots, base + ".*")))), None)
        lines.append("## " + base)
        lines.append("")
        lines.append("> " + (transcript or "(nothing heard)"))
        lines.append("")
        if context:
            lines.extend("    " + l for l in context.splitlines())
            lines.append("")
        if shot:
            lines.append("Screenshot: `%s`" % os.path.relpath(shot, args.notes).replace(os.sep, "/"))
            lines.append("")
    out = os.path.join(args.notes, "NOTES.md")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("%d notes -> %s" % (len(notes), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
