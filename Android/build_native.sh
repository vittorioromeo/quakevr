#!/usr/bin/env bash
# build_native.sh -- the engine for the Quest (arm64-v8a): libquakevr.so and libSDL2.so, with the NDK's toolchain, the
# SDK's CMake and Ninja (docs/vr-port/QUEST_PLAN.md, C3; Android/README.md). Gradle's externalNativeBuild builds the
# same CMake project; this script is the build without Gradle (the offline APK: Android/package_apk.sh).
#
#   Android/build_native.sh [build folder (default <repo>/build-android)] [Release|Debug|RelWithDebInfo (default Release)]
#
# Needs ANDROID_HOME, ANDROID_NDK_HOME and QVR_SDL2_DIR (SDL2's source, 2.30.x) in the environment
# (F:/QuakeVRDev/env.sh sets the first two). QVR_ANDROID_KEEP_GOING=1: every failing file, not the first (ninja -k 0).
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/.." && pwd)"
: "${ANDROID_HOME:?ANDROID_HOME: the Android SDK}"
: "${ANDROID_NDK_HOME:?ANDROID_NDK_HOME: the NDK (27.x)}"
: "${QVR_SDL2_DIR:?QVR_SDL2_DIR: the SDL2 source folder}"
win() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }
cmakeDir="$ANDROID_HOME/cmake/3.22.1/bin"
exe=""
[ -f "$cmakeDir/cmake.exe" ] && exe=".exe"
out="${1:-$repo/build-android}"
config="${2:-Release}"
"$cmakeDir/cmake$exe" -S "$(win "$repo")" -B "$(win "$out")" -G Ninja \
	-DCMAKE_MAKE_PROGRAM="$(win "$cmakeDir/ninja$exe")" \
	-DCMAKE_TOOLCHAIN_FILE="$(win "$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake")" \
	-DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_static \
	-DCMAKE_BUILD_TYPE="$config" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DQVR_SDL2_DIR="$(win "$QVR_SDL2_DIR")" \
	-DQVR_QC=OFF # (progs.dat: the PC build's, staged with the game folder; no FTEQCC build for the device)
keep=()
[ "${QVR_ANDROID_KEEP_GOING:-0}" = "1" ] && keep=(-- -k 0)
"$cmakeDir/cmake$exe" --build "$(win "$out")" --target ironwail SDL2 ${keep[@]+"${keep[@]}"}
echo "built: $out/libquakevr.so, $out/sdl2/libSDL2.so"
