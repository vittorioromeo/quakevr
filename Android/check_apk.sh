#!/usr/bin/env bash
# check_apk.sh -- what the APK must carry (docs/vr-port/QUEST_PLAN.md, C4a), without a device: the package, the
# libraries (the engine, SDL2, Khronos' OpenXR loader), the engine's OpenXR imports and the manifest's VR lines (the VR
# category, OpenXR's immersive category, head tracking, focus awareness, the loader's permissions and broker queries;
# none of the VR lines in the flat-panel APK). Ends on "check_apk: N passed, M failed"; exits 1 on a failure.
#
#   Android/check_apk.sh [apk (default <repo>/build-android/apk/QuakeVR.apk)] [vr|flat (default vr)] [dev|release|release-testkey]
#
# Needs ANDROID_HOME (build-tools 34.0.0) and ANDROID_NDK_HOME (llvm-readelf, llvm-nm); the unstripped
# libquakevr.so beside the APK's build folder (<apk>/../../libquakevr.so) for the imports.
# release (the release APK, docs/vr-port/ANDROID_RELEASE.md) also checks: not debuggable, a version code from VERSION,
# the APK's libquakevr.so stripped (no .symtab, no debug sections), the engine a release build (QVR_RELEASE: no "-dev"
# version, no live tuning), and the signer: not a debug certificate, and the release certificate's SHA-256 when known
# (QVR_ANDROID_RELEASE_CERT_SHA256, else the file Android/release-cert.sha256). release-testkey: the same but the signer
# (a release build signed with the debug key for a test on the headset: never published).
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
build="${3:-dev}"
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

grep -q "package: name='com.vittorioromeo.quakevr'" <<< "$badging"; check "the package name" $?
grep -q "application: label='Quake VR: UNLEASHED'" <<< "$badging"; check "the app's name (Quake VR: UNLEASHED)" $?
grep -q "native-code: 'arm64-v8a'" <<< "$badging"; check "arm64-v8a native code" $?
grep -q "uses-gl-es: '0x30002'" <<< "$badging"; check "OpenGL ES 3.2" $?
grep -q "launchable-activity: name='com.vittorioromeo.quakevr.QuakeVRActivity'" <<< "$badging"; check "the launchable activity" $?
for so in libquakevr.so libSDL2.so libopenxr_loader.so; do
	grep -q "lib/arm64-v8a/$so" <<< "$listing"; check "lib/arm64-v8a/$so in the APK" $?
done
grep -q "org.khronos.openxr.permission.OPENXR\"" <<< "$manifest"; check "the OPENXR permission" $?
grep -q "android.permission.INTERNET" <<< "$manifest"; check "the INTERNET permission (multiplayer)" $?
grep -q "android.permission.RECORD_AUDIO" <<< "$manifest"; check "the RECORD_AUDIO permission (voice chat)" $?
grep -q "org.khronos.openxr.runtime_broker" <<< "$manifest"; check "the runtime broker's query" $?
grep -q "org.khronos.openxr.OpenXRRuntimeService" <<< "$manifest"; check "the runtime service's query" $?
vrLines=0
for line in com.oculus.intent.category.VR org.khronos.openxr.intent.category.IMMERSIVE_HMD \
	android.hardware.vr.headtracking com.oculus.vr.focusaware com.meta.permission.FOCUSED_BODY_TRACKING \
	com.oculus.permission.BODY_TRACKING com.oculus.software.body_tracking; do
	if grep -q "\"$line\"" <<< "$manifest"; then vrLines=$((vrLines + 1)); fi
done
if [ "$kind" = "flat" ]; then
	[ "$vrLines" = "0" ]; check "no VR lines in the flat-panel manifest ($vrLines found)" $?
else
	[ "$vrLines" = "7" ]; check "the 7 VR lines in the manifest (body tracking's 3 too; $vrLines found)" $?
fi
if [ -f "$lib" ]; then
	"$llvm/llvm-readelf$exe" -d "$(win "$lib")" | grep -q "NEEDED.*\[libopenxr_loader.so\]"; check "libquakevr.so needs libopenxr_loader.so" $?
	undefined="$("$llvm/llvm-nm$exe" -D --undefined-only "$(win "$lib")")"
	for sym in xrGetInstanceProcAddr xrCreateInstance xrCreateSession xrCreateSwapchain SDL_AndroidGetActivity SDL_AndroidGetJNIEnv; do
		grep -q " $sym$" <<< "$undefined"; check "libquakevr.so imports $sym" $?
	done
else
	check "the unstripped $lib" 1
fi
if [ "$build" = "release" ] || [ "$build" = "release-testkey" ]; then
	! grep -q "application-debuggable" <<< "$badging"; check "not debuggable (the release manifest)" $?
	if [ "$build" = "release" ]; then # (a test APK may keep the installed build's code: QVR_ANDROID_VERSION_CODE)
		code="$(sed -n "s/.*versionCode='\([0-9]*\)'.*/\1/p" <<< "$badging" | head -1)"
		[ "${code:-1}" -gt 1 ]; check "a version code from VERSION (got ${code:-none})" $?
	fi
	# The APK's engine library, unpacked beside the APK for the section listing.
	packed="$(dirname "$apk")/check_apk_libquakevr.so"
	unzip -p "$apk" lib/arm64-v8a/libquakevr.so > "$packed" 2>/dev/null
	sections="$("$llvm/llvm-readelf$exe" -S "$(win "$packed")" 2>&1)"
	! grep -q -E " \.symtab | \.debug_" <<< "$sections"; check "the APK's libquakevr.so stripped (no .symtab, no debug sections)" $?
	! grep -a -q -e "-dev build" "$packed"; check "a release engine (no -dev build info: QVR_RELEASE)" $?
	! grep -a -q -e "tune_reset" "$packed"; check "a release engine (no live tuning: QVR_RELEASE)" $?
	if [ "$build" = "release" ]; then
		certs="$("$ANDROID_HOME/build-tools/34.0.0/apksigner$([ -n "$exe" ] && echo .bat)" verify --print-certs "$(win "$apk")" 2>&1)"
		grep -q "Signer #1 certificate DN" <<< "$certs"; check "apksigner reads the signer" $?
		! grep -q "Signer #1 certificate DN: CN=Android Debug" <<< "$certs"; check "the signer is not a debug key" $?
		want="${QVR_ANDROID_RELEASE_CERT_SHA256:-}"
		[ -n "$want" ] || [ ! -f "$here/release-cert.sha256" ] || want="$(head -1 "$here/release-cert.sha256")"
		want="$(tr -d ' :\r\n' <<< "$want" | tr 'A-F' 'a-f')"
		if [ -n "$want" ]; then
			grep -q "Signer #1 certificate SHA-256 digest: $want" <<< "$certs"; check "the signer is the release certificate ($want)" $?
		else
			echo "check_apk: note: the release certificate's SHA-256 is not known (Android/release-cert.sha256): signer checked only as not a debug key"
		fi
	fi
fi
echo "check_apk: $passed passed, $failed failed"
[ "$failed" = "0" ]
