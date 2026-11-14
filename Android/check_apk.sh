#!/usr/bin/env bash
# check_apk.sh -- what the APK must carry (docs/vr-port/QUEST_PLAN.md, C4a), without a device: the package, the
# libraries (the engine, SDL2, Khronos' OpenXR loader), the engine's OpenXR imports and the manifest's VR lines (the VR
# category, OpenXR's immersive category, head tracking, focus awareness, the loader's permissions and broker queries;
# none of the VR lines in the flat-panel APK). Ends on "check_apk: N passed, M failed"; exits 1 on a failure.
#
#   Android/check_apk.sh [apk (default <repo>/build-android/apk/QuakeVR.apk)] [vr|flat (default vr)]
#
# Needs ANDROID_HOME (build-tools 34.0.0) and ANDROID_NDK_HOME (llvm-readelf, llvm-nm); the unstripped
# libquakevr.so beside the APK's build folder (<apk>/../../libquakevr.so) for the imports.
set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/.." && pwd)"
: "${ANDROID_HOME:?ANDROID_HOME: the Android SDK}"
: "${ANDROID_NDK_HOME:?ANDROID_NDK_HOME: the NDK}"
win() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }
exe=""
[ -f "$ANDROID_HOME/build-tools/34.0.0/aapt2.exe" ] && exe=".exe"
aapt2="$ANDROID_HOME/build-tools/34.0.0/aapt2$exe"
llvm="$(ls -d "$ANDROID_NDK_HOME"/toolchains/llvm/prebuilt/*/bin | head -1)"
apk="${1:-$repo/build-android/apk/QuakeVR.apk}"
kind="${2:-vr}"
lib="$(dirname "$apk")/../libquakevr.so"
passed=0
failed=0
check() { # check <what> <condition's exit status>
	if [ "$2" = "0" ]; then passed=$((passed + 1)); else failed=$((failed + 1)); echo "check_apk: FAIL $1"; fi
}
[ -f "$apk" ] || { echo "check_apk: no $apk"; echo "check_apk: 0 passed, 1 failed"; exit 1; }
badging="$("$aapt2" dump badging "$(win "$apk")" 2>&1)"
manifest="$("$aapt2" dump xmltree --file AndroidManifest.xml "$(win "$apk")" 2>&1)"
listing="$(unzip -l "$apk" 2>&1)"

echo "$badging" | grep -q "package: name='com.vittorioromeo.quakevr'"; check "the package name" $?
echo "$badging" | grep -q "native-code: 'arm64-v8a'"; check "arm64-v8a native code" $?
echo "$badging" | grep -q "uses-gl-es: '0x30002'"; check "OpenGL ES 3.2" $?
echo "$badging" | grep -q "launchable-activity: name='com.vittorioromeo.quakevr.QuakeVRActivity'"; check "the launchable activity" $?
for so in libquakevr.so libSDL2.so libopenxr_loader.so; do
	echo "$listing" | grep -q "lib/arm64-v8a/$so"; check "lib/arm64-v8a/$so in the APK" $?
done
echo "$manifest" | grep -q "org.khronos.openxr.permission.OPENXR\""; check "the OPENXR permission" $?
echo "$manifest" | grep -q "org.khronos.openxr.runtime_broker"; check "the runtime broker's query" $?
echo "$manifest" | grep -q "org.khronos.openxr.OpenXRRuntimeService"; check "the runtime service's query" $?
vrLines=0
for line in com.oculus.intent.category.VR org.khronos.openxr.intent.category.IMMERSIVE_HMD \
	android.hardware.vr.headtracking com.oculus.vr.focusaware; do
	if echo "$manifest" | grep -q "\"$line\""; then vrLines=$((vrLines + 1)); fi
done
if [ "$kind" = "flat" ]; then
	[ "$vrLines" = "0" ]; check "no VR lines in the flat-panel manifest ($vrLines found)" $?
else
	[ "$vrLines" = "4" ]; check "the 4 VR lines in the manifest ($vrLines found)" $?
fi
if [ -f "$lib" ]; then
	"$llvm/llvm-readelf$exe" -d "$(win "$lib")" | grep -q "NEEDED.*\[libopenxr_loader.so\]"; check "libquakevr.so needs libopenxr_loader.so" $?
	undefined="$("$llvm/llvm-nm$exe" -D --undefined-only "$(win "$lib")")"
	for sym in xrGetInstanceProcAddr xrCreateInstance xrCreateSession xrCreateSwapchain SDL_AndroidGetActivity SDL_AndroidGetJNIEnv; do
		echo "$undefined" | grep -q " $sym$"; check "libquakevr.so imports $sym" $?
	done
else
	check "the unstripped $lib" 1
fi
echo "check_apk: $passed passed, $failed failed"
[ "$failed" = "0" ]
