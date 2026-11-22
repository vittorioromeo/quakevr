# stb_vorbis (vendored: the Quest's Ogg Vorbis music decoder)

Sean Barrett's single-file Ogg Vorbis decoder, public domain or MIT (the choice is the user's: the licence text is at
the end of `stb_vorbis.c`).

- Upstream: https://github.com/nothings/stb (`stb_vorbis.c`), release `v1.22` (the CVE-2019-13217..13223 fixes and
  the later fuzzing fixes included).
- Copied from miniaudio's `extras/stb_vorbis.c` (mackron/miniaudio, commit `80b9fa63ee66`: upstream's v1.22 with a
  MinGW macro-warning fix), SHA-256 `4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636`. No local
  changes.

Why: the PC build decodes Ogg music with libvorbisfile (`Windows/codecs`, prebuilt; pkg-config on Linux), which has no
source in the repository, so the Quest's Android build had no Ogg codec at all (WAV music only). This file is the
Android build's Ogg decoder (`Quake/snd_vorbis.c`, `VORBIS_USE_STB`), the same codec interface (`vorbis_codec`, the
`ogg` extension) and the same music path (`bgmusic.c`) as the PC. The PC builds compile it too (`QVR_STB_VORBIS`) and
use it only in the fake-Android mode (`-fakeandroid`), so the Quest's decoder is tested on the PC (`vr_music_decode`,
`Misc/quakevr/questogg_test.sh`).

Built as its own unit, warnings off: `Quake/vr/vr.cmake` (`qvr_stb_vorbis`, every CMake build, the Quest's too),
`Windows/VisualStudio/quakevr.props`. `snd_vorbis.c` includes it with `STB_VORBIS_HEADER_ONLY` for the declarations.
