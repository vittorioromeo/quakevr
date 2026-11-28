#!/bin/sh
# build_fteqcc.sh <fteqw qclib dir> <out exe> -- builds Tools/fteqcc/fteqcc64.exe (README.md) from FTEQCC's
# engine/qclib (github.com/vittorioromeo/fteqw, qcc-perf-parseerror-null 6cfe93f4) plus Quake VR's C++ backend:
# copies the source to <out exe>'s folder/qclib, applies cpp.patch (the -cppout hook in qccmain.c) and adds qcc_cpp.c.
# Windows: VS 2026's (or 2022's) clang-cl. Linux/macOS: the same files build with cc (Makefile's qcc target plus
# qcc_cpp.c).
set -e
SRC="$1"; OUT="$2"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(dirname "$OUT")/qclib"
mkdir -p "$WORK"
cp -r "$SRC"/. "$WORK"/
cp "$HERE/qcc_cpp.c" "$WORK/"
# the hook: applied unless it is there already (a second build in the same work folder)
grep -q "QCC_CppGen_Maybe" "$WORK/qccmain.c" || patch -s -d "$WORK" -p3 < "$HERE/cpp.patch"
CLANG=/f/VS2026/VC/Tools/Llvm/x64/bin/clang-cl.exe
[ -x "$CLANG" ] || CLANG="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-cl.exe"
cd "$WORK"
MSYS2_ARG_CONV_EXCL="*" "$CLANG" /O2 /w -Wno-incompatible-pointer-types -Wno-int-conversion /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /DNO_ZLIB /DSVNREVISION=git-6729-f937b9d88 /DSVNDATE=2026-06-04 \
	qcctui.c comprout.c hash.c qcc_cmdlib.c qcc_pr_comp.c qcc_pr_lex.c qccmain.c qcd_main.c decomp.c packager.c qcc_cpp.c \
	/Fe:"$OUT" /link /OPT:REF /OPT:ICF > build_fteqcc.log 2>&1 || { tail -20 build_fteqcc.log; exit 1; }
echo "built $OUT"
