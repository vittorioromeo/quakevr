# RNNoise (vendored: the library with its built-in model)

Jean-Marc Valin's recurrent-network noise suppressor (Xiph.Org, Mozilla, Amazon), BSD-3-Clause (`COPYING`; `AUTHORS`).

- Upstream: https://github.com/xiph/rnnoise, release `v0.2`
- Archive: `rnnoise-0.2.tar.gz` (the GitHub release's tarball), SHA-256
  `90fce4b00b9ff24c08dbfe31b82ffd43bae383d85c5535676d28b0a2b11c0d37`
- Vendored: `include/rnnoise.h`; the library's sources as its `Makefile.am` lists them without the x86 run-time CPU
  dispatch (`denoise.c`, `rnn.c`, `pitch.c`, `kiss_fft.c`, `celt_lpc.c`, `nnet.c`, `nnet_default.c`,
  `parse_lpcnet_weights.c`, `rnnoise_tables.c`, and `rnnoise_data.c`: the model's weights, 4.9 MB of C, part of the
  release: no separate model download) and the headers of `src` and `src/x86` (`vec.h` includes them; SSE2 or NEON
  inlines are chosen at compile time); `COPYING`, `AUTHORS`. Not vendored: `src/x86`'s sources (the SSE4.1/AVX2
  variants behind its run-time dispatch), the examples, training scripts, `dump_features.c`, `write_weights.c`, the build scripts. No
  local changes: the files are upstream's.

Used by `Quake/vr/vr_voice.cpp` (voice chat, docs/vr-port/MP_PLAN.md, B10v): 48 kHz mono, 10 ms frames, before the
Opus encoder. Its function names are its own (`rnn_*`, `rnnoise_*`; its FFT and LPC are prefixed copies of Opus's), so
it links beside Opus. Built as its own static library, always optimised (NDEBUG), Debug builds too, warnings off:
`Windows/VisualStudio/rnnoise.vcxproj` (in the solution, referenced by `quakevr.props`), `Quake/vr/vr.cmake`
(`qvr_rnnoise`, the Quest's Android build too), `Quake/vr/vr.mk` (the Makefiles). The release package ships `COPYING`
as `licenses/rnnoise.txt` (`Windows/package-quakevr.ps1`).
