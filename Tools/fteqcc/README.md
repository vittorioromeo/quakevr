# fteqcc64.exe (Windows, x64)

The QuakeC compiler the Windows builds use by default (`Windows/VisualStudio/ironwail.vcxproj`'s `QvrQcCompiler`
and `QC/build.sh` on Windows). It is FTEQCC with a few speed patches: on Quake VR's QC (~270k lines) a compile takes
~2 s instead of ~8 s, and the `-Fwasm` listing the precedence check reads ~3 s instead of ~25 s.

- **Source:** https://github.com/vittorioromeo/fteqw, branch `qcc-perf-parseerror-null`, commit `c1c874d`
  (five speed commits on top of upstream `fte-team/fteqw` `f937b9d88`, then one fix; see each commit's message).
- **The fix (`c1c874d`):** stock FTEQCC crashes (no message after the real error, exit 0xC0000005 / 0xC0000409) when
  a compile error is followed by any `#include`, or on a "X is not a member of Y" error: `QCC_PR_ParseError(0, NULL)`,
  the silent abort, passed its NULL format to `vsnprintf`. It now ends with the error and "compilation errors".
  `test_fteqcc.py` here compiles those tiny cases (the kit's build.sh runs it whenever the compiler changes).
- **Output is byte-identical** to stock FTEQCC `f937b9d88` (git-6729): `progs.dat`, the `-Fwasm` listing and the
  compile log, checked on our QC. The binary is stamped with the upstream revision (`git-6729-f937b9d88`, 2026-06-04)
  on purpose: FTEQCC writes its version string into `progs.dat`, so a different stamp would change the output.
- **Built with** VS 2022 clang-cl from `engine/qclib`:
  `clang-cl /O2 /w /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /DNO_ZLIB /DSVNREVISION=git-6729-f937b9d88 /DSVNDATE=2026-06-04
  qcctui.c comprout.c hash.c qcc_cmdlib.c qcc_pr_comp.c qcc_pr_lex.c qccmain.c qcd_main.c decomp.c packager.c
  /Fe:fteqcc64.exe /link /OPT:REF /OPT:ICF`
- SHA-256: `923ac7474c35fa8552ad3ac4ddab98662e76f9acdb0567ce916feaf9acaa5a48`

FTEQCC is GPL-2.0-or-later (the fteqw repository's license); its source is at the link above.
To use another compiler: `/p:QvrQcCompiler=<path>` for the Visual Studio build, `FTEQCC=<path>` for `QC/build.sh`.
