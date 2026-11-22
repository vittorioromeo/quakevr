#!/bin/bash
# questogg_test.sh <agent> -- Ogg Vorbis music on the Quest's decoder (stb_vorbis, Quake/vr/external/stb_vorbis):
# the Quest's Android build has no libvorbisfile, so its Ogg music is decoded by stb_vorbis (snd_vorbis.c), which the
# PC build also has and uses in the fake-Android mode (-fakeandroid). Two runs, each starting Quake's start map (its CD
# track plays through bgmusic.c as always) and decoding 3 s of track 2 and of the map's track (vr_music_decode): the PC
# (libvorbisfile), then -fakeandroid (stb_vorbis). Checks: the -fakeandroid run decodes with stb_vorbis, the PC run
# with libvorbisfile; both find the same Ogg file with the same rate, channels and frames, peaks within 2%, the loop's
# rewind reads the start again; the map's track starts playing in both with no Ogg or stream error. The sound runs on
# SDL's dummy driver (nothing heard; the codecs start with the sound). Needs an Ogg soundtrack (the rerelease's, found
# in place). Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
export SDL_AUDIODRIVER=dummy
S="map start;wait10;vr_music_decode 2 3000;vr_music_decode;echo questogg: done"
# (the console wraps long lines at a space, keeping it: the continuation lines are let through and joined back)
F="^VR music|^\([a-z0-9]+, [a-z_]+\) |^\(from |questogg: done|not a valid Ogg|Stream (read|seek) error|Couldn't handle music|exit="
unwrap() { tr -d '\r\n' | sed 's/VR music/\nVR music/g; s/questogg: done/\nquestogg: done/g; s/exit=/\nexit=/g'; echo; }
pc=$(bash $KIT/run.sh $AGENT -Sound -ExtraArgs "-nomapindex -noaddons" -Script "$S" -QuitOn "questogg: done" \
    -Timeout 240 -Filter "$F" 2>&1 | unwrap)
fa=$(bash $KIT/run.sh $AGENT -Sound -ExtraArgs "-nomapindex -noaddons -fakeandroid" -Script "$S" -QuitOn "questogg: done" \
    -Timeout 240 -Filter "$F" 2>&1 | unwrap)
echo "$pc" | grep -E "^VR music" | sed 's/^/pc: /' | cut -c1-200
echo "$fa" | grep -E "^VR music" | sed 's/^/fakeandroid: /' | cut -c1-200
d2pc=$(grep -m1 "^VR music decode: track 2 " <<<"$pc")
d2fa=$(grep -m1 "^VR music decode: track 2 " <<<"$fa")
# "... -> <file> (ogg, <decoder>) R Hz, C ch: F frames, peak P, hash H": the file, the format, the numbers
field() { sed -n "s/.* -> \(.*\) (\([a-z0-9]*\), \([a-z_]*\)) \([0-9]*\) Hz, \([0-9]*\) ch: \([0-9]*\) frames, peak \([0-9]*\), .*/\\$2/p" <<<"$1"; }
check $([ "$(field "$d2pc" 2)" = "ogg" ] && echo 1 || echo 0) "PC: track 2 is an Ogg file (${d2pc:-no decode line})"
check $([ "$(field "$d2pc" 3)" = "libvorbisfile" ] && echo 1 || echo 0) "PC: decoded by libvorbisfile"
check $([ "$(field "$d2fa" 3)" = "stb_vorbis" ] && echo 1 || echo 0) "-fakeandroid: decoded by stb_vorbis, the Quest's decoder (${d2fa:-no decode line})"
check $([ -n "$(field "$d2fa" 1)" ] && [ "$(field "$d2fa" 1)" = "$(field "$d2pc" 1)" ] && echo 1 || echo 0) "both: the same file"
check $([ -n "$(field "$d2fa" 4)" ] && [ "$(field "$d2fa" 4)/$(field "$d2fa" 5)" = "$(field "$d2pc" 4)/$(field "$d2pc" 5)" ] && echo 1 || echo 0) "both: the same rate and channels ($(field "$d2fa" 4) Hz, $(field "$d2fa" 5) ch)"
fpc=$(field "$d2pc" 6); ffa=$(field "$d2fa" 6)
check $([ -n "$ffa" ] && [ "$ffa" -gt 0 ] && [ "$ffa" = "$fpc" ] && echo 1 || echo 0) "both: 3 s decoded, the same frames ($ffa, PC $fpc)"
ppc=$(field "$d2pc" 7); pfa=$(field "$d2fa" 7)
check $([ -n "$pfa" ] && [ "${pfa:-0}" -gt 0 ] && [ $(( (pfa - ppc) * (pfa - ppc) * 2500 )) -le $(( ppc * ppc )) ] && echo 1 || echo 0) "both: the same audio, peaks within 2% (stb $pfa, PC $ppc)"
check $(grep -q "rewind ok" <<<"$d2fa" && grep -q "rewind ok" <<<"$d2pc" && echo 1 || echo 0) "both: the loop's rewind reads the start again"
check $(grep -q "^VR music decode: track [0-9]* -> .*(ogg, stb_vorbis)"<<<"$(grep -v "track 2 " <<<"$fa")" && echo 1 || echo 0) "-fakeandroid: the map's own track decoded by stb_vorbis"
check $(grep -q "^VR music: track " <<<"$fa" && echo 1 || echo 0) "-fakeandroid: the map's track started playing"
check $(grep -qE "not a valid Ogg|Stream (read|seek) error|Couldn't handle music" <<<"$pc$fa" && echo 0 || echo 1) "no Ogg or stream error"
check $(grep -q "exit=quit-on" <<<"$pc" && grep -q "exit=quit-on" <<<"$fa" && echo 1 || echo 0) "both runs ended on their last line"
verdict
exit $fail
