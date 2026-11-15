# Opus (vendored: the float codec, without its DNN parts)

Xiph.Org's speech and audio codec, BSD-3-Clause (`COPYING`; contributors in `AUTHORS`).

- Upstream: https://opus-codec.org/downloads/ (https://github.com/xiph/opus), release `1.6.1`
- Archive: `opus-1.6.1.tar.gz` (the official release tarball, downloads.xiph.org), SHA-256
  `6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1` (as in xiph's `SHA256SUMS.txt`)
- Vendored: `include/*.h`; the sources the release's own lists name for a float build (`opus_sources.mk`:
  `OPUS_SOURCES`, `OPUS_SOURCES_FLOAT`; `celt_sources.mk`: `CELT_SOURCES`; `silk_sources.mk`: `SILK_SOURCES`,
  `SILK_SOURCES_FLOAT`) and the headers of their folders (`src`, `celt`, `silk`, `silk/float`); `COPYING`, `AUTHORS`.
  Not vendored: `dnn` (the optional deep PLC, DRED and OSCE, off by default), the x86/ARM/MIPS SIMD and run-time CPU
  dispatch, the fixed-point build, the demos, tests, docs and build scripts. No local changes: the files are upstream's.

Used by `Quake/vr/vr_voice.cpp` (voice chat, docs/vr-port/MP_PLAN.md, B10v): 48 kHz mono, 20 ms frames, VoIP mode.
Built as its own static library, always optimised (NDEBUG), Debug builds too, warnings off, with `OPUS_BUILD`,
`USE_ALLOCA`, `HAVE_LRINT`, `HAVE_LRINTF` (and `HAVE_ALLOCA_H` off Windows; no `config.h`):
`Windows/VisualStudio/opus.vcxproj` (in the solution, referenced by `quakevr.props`), `Quake/vr/vr.cmake` (`qvr_opus`,
the Quest's Android build too), `Quake/vr/vr.mk` (the Makefiles). The release package ships `COPYING` as
`licenses/opus.txt` (`Windows/package-quakevr.ps1`).
