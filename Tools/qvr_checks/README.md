# qvr_checks: the build's source checks

One C++20 tool (no dependencies) for the checks the kit's `build.sh`, the Visual Studio build, `QC/build.sh`,
`QC/build.bat` and the release script run. It replaced the Python scripts (`check_statics.py`,
`check_test_numbers.py`, `check_builtins.py`, `gen_vr_cvars_qc.py`, `check_canonical.py`, `check_release_qc.py`,
`check_qc_precedence.py`, `check_progs_format.py`) with the same rules, verdicts and messages; each file's header
comment says what it checks.

Build: `MSBuild Tools/qvr_checks/qvr_checks.vcxproj -p:Configuration=Release -p:Platform=x64` (clang-cl, not in the
solution): `build/qvr_checks/qvr_checks.exe`. `Windows/VisualStudio/ironwail.vcxproj` builds it before its QC step
(`qvr_checks.targets`, only when a source is newer than the exe); the kit's build.sh the same way.

```
qvr_checks [--tree DIR] [--fteqcc PATH] <check> [arguments]      (--tree: the repository, default the current folder)
  statics [files or folders]     no function-local static/thread_local in Quake/vr that is not constexpr
  test_numbers [--list]          the numbered tests' dispatchers against QC/test_numbers.txt
  builtins                       the QC's `= #0` builtins against the engine's tables
  cvars_qc [--check]             writes QC/vr_cvars_gen.qc from the QC's cvarh_ uses (--check: stale or unknown fails)
  canonical [--list FAM | --tighten]   no new copies of the canonical helpers (Misc/quakevr/canonical_allowlist.txt)
  release_qc                     the release flavour's QC (-DQVR_NO_TESTS) compiles clean, tests guarded
  qc_precedence                  no QC expression that reads differently under C priorities (two compiles at once)
  progs_format [progs.dat]       progs.dat is FTE's 32-bit format
  fgd [args]                     Misc/trenchbroom/fgdgen.py (still Python), as a process
  all [--qchash H] [--times]     every check at once, a thread each; prints each failing check's last lines and a
                                 "... FAILED" line, in a fixed order; exit 1 when one fails (the FGD check only warns)
```

`all --qchash H` skips the precedence check when `scratch/.qc_precedence_pass` holds H (the kit's `qc_hash.py`) and
writes it on a pass. Work files: `build/qcrelease/`, `build/qcprec/` (the precedence check's two progs.dat and
progs.lno, ~16 MB each, are emptied after each run).

Not Windows: the tool uses Win32 for processes and file mapping. `progs_format.sh` is the progs-format check in
plain sh (the flatpak, `QC/build.sh` elsewhere).

Adding a check: a function `int checkX(const Ctx&, const Args&, Out&)` in its own file (common.hpp declares it),
a row in main.cpp's `checks` and `allEntries`, the file in qvr_checks.vcxproj.
