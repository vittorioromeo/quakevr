#!/usr/bin/env bash
# package_apk.sh -- the APK without Gradle (docs/vr-port/QUEST_PLAN.md, C3; Android/README.md): SDL2's Java activity
# and ours compiled (javac, d8), the manifest linked (aapt2), the libraries from Android/build_native.sh added
# (stripped), aligned (zipalign) and signed (apksigner) with a debug key. The same contents as Gradle's
# assembleRelease, from the SDK's build tools and the JDK alone (no Android Gradle Plugin download).
#
#   Android/package_apk.sh [native build folder (default <repo>/build-android)] [output folder (default <it>/apk)]
#
# Needs JAVA_HOME (JDK 17), ANDROID_HOME (build-tools 34.0.0, platforms;android-34), ANDROID_NDK_HOME (llvm-strip) and
# QVR_SDL2_DIR. Optional: QVR_STEAMAUDIO_DIR (Steam Audio's unpacked release: lib/android-armv8/libphonon.so goes in),
# QVR_ANDROID_KEYSTORE (a keystore whose key "androiddebugkey" has the password "android"; default: made once in the
# output folder). Writes <output>/QuakeVR.apk.
#
# QVR_ANDROID_RELEASE=1: the release APK (docs/vr-port/ANDROID_RELEASE.md) from build_native.sh's release build (default
# folder <repo>/build-android-release): the manifest not debuggable, the version code from VERSION, signed with the
# release key, read from the environment and never from the repository:
#   QVR_ANDROID_RELEASE_KEYSTORE   the keystore file (outside the repository)
#   QVR_ANDROID_RELEASE_STOREPASS  its password; QVR_ANDROID_RELEASE_KEYPASS the key's (default: the store's)
#   QVR_ANDROID_RELEASE_ALIAS      the key's alias (default quakevr)
# or from QVR_ANDROID_RELEASE_ENV, a file of those NAME=value lines outside the repository (default
# ~/.quakevr/android-release.env when it exists). The passwords reach apksigner through the environment (env:NAME), never
# a command line. Missing ones stop the script. QVR_ANDROID_SIGN=debug signs a release build with the debug key instead
# (a test APK for a headset that has the debug-key build: never one to publish).
# QVR_ANDROID_VERSION_CODE overrides the version code (default: 1 for a development APK; for a release, from VERSION:
# 1.2.0 is 1020099, 1.2.0-beta2 1020002, so every release updates the one before).
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/.." && pwd)"
: "${JAVA_HOME:?JAVA_HOME: a JDK 17}"
: "${ANDROID_HOME:?ANDROID_HOME: the Android SDK}"
: "${ANDROID_NDK_HOME:?ANDROID_NDK_HOME: the NDK}"
: "${QVR_SDL2_DIR:?QVR_SDL2_DIR: the SDL2 source folder}"
win() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }
exe=""
[ -f "$JAVA_HOME/bin/java.exe" ] && exe=".exe"
java="$JAVA_HOME/bin/java$exe"
javac="$JAVA_HOME/bin/javac$exe"
jar="$JAVA_HOME/bin/jar$exe"
keytool="$JAVA_HOME/bin/keytool$exe"
bt="$ANDROID_HOME/build-tools/34.0.0"
androidJar="$ANDROID_HOME/platforms/android-34/android.jar"
strip="$(ls -d "$ANDROID_NDK_HOME"/toolchains/llvm/prebuilt/*/bin | head -1)/llvm-strip$exe"
release="${QVR_ANDROID_RELEASE:-0}"
sign="release"
[ "$release" = "1" ] || sign="debug"
[ "${QVR_ANDROID_SIGN:-}" = "debug" ] && sign="debug"
if [ "$release" = "1" ]; then native="${1:-$repo/build-android-release}"; else native="${1:-$repo/build-android}"; fi
out="${2:-$native/apk}"

# The version: VERSION's name; the code 1 for a development APK (they replace one another), from VERSION for a release.
versionName="$(head -1 "$repo/VERSION" | tr -d '\r\n ')"
versionCode=1
if [ "$release" = "1" ]; then
	if [[ "$versionName" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)(-[a-z]+([0-9]+))?$ ]]; then
		pre=99
		[ -n "${BASH_REMATCH[4]}" ] && pre=$((10#${BASH_REMATCH[5]}))
		[ "$pre" -lt 99 ] || { echo "VERSION $versionName: a prerelease number under 99, please" >&2; exit 1; }
		versionCode=$(( ((10#${BASH_REMATCH[1]} * 100 + 10#${BASH_REMATCH[2]}) * 100 + 10#${BASH_REMATCH[3]}) * 100 + pre ))
	else
		echo "VERSION '$versionName' is not <major>.<minor>.<patch>[-<word><n>]: no version code for the release APK" >&2
		exit 1
	fi
fi
versionCode="${QVR_ANDROID_VERSION_CODE:-$versionCode}"

# The release key (before anything is built: a missing one stops here). Never from the repository.
if [ "$sign" = "release" ]; then
	envFile="${QVR_ANDROID_RELEASE_ENV:-$HOME/.quakevr/android-release.env}"
	if [ -f "$envFile" ]; then
		case "$(cd "$(dirname "$envFile")" && pwd)/" in "$repo"/*)
			echo "QVR_ANDROID_RELEASE_ENV ($envFile) is inside the repository: keep it outside (ANDROID_RELEASE.md)" >&2; exit 1;;
		esac
		while IFS='=' read -r name value; do
			case "$name" in
				QVR_ANDROID_RELEASE_KEYSTORE|QVR_ANDROID_RELEASE_STOREPASS|QVR_ANDROID_RELEASE_KEYPASS|QVR_ANDROID_RELEASE_ALIAS)
					[ -n "${!name:-}" ] || export "$name=${value%$'\r'}";;
			esac
		done < "$envFile"
	fi
	missing=""
	[ -n "${QVR_ANDROID_RELEASE_KEYSTORE:-}" ] || missing="$missing QVR_ANDROID_RELEASE_KEYSTORE"
	[ -n "${QVR_ANDROID_RELEASE_STOREPASS:-}" ] || missing="$missing QVR_ANDROID_RELEASE_STOREPASS"
	if [ -n "$missing" ]; then
		echo "the release key is not set:$missing (environment variables, or NAME=value lines in $envFile;" \
			"docs/vr-port/ANDROID_RELEASE.md). QVR_ANDROID_SIGN=debug signs a test APK with the debug key." >&2
		exit 1
	fi
	[ -f "$QVR_ANDROID_RELEASE_KEYSTORE" ] || { echo "QVR_ANDROID_RELEASE_KEYSTORE: no file $QVR_ANDROID_RELEASE_KEYSTORE" >&2; exit 1; }
	case "$(cd "$(dirname "$QVR_ANDROID_RELEASE_KEYSTORE")" && pwd)/" in "$repo"/*)
		echo "QVR_ANDROID_RELEASE_KEYSTORE is inside the repository: keep the release key outside it (ANDROID_RELEASE.md)" >&2; exit 1;;
	esac
	export QVR_ANDROID_RELEASE_KEYPASS="${QVR_ANDROID_RELEASE_KEYPASS:-$QVR_ANDROID_RELEASE_STOREPASS}"
	export QVR_ANDROID_RELEASE_STOREPASS
fi
[ -f "$native/libquakevr.so" ] || { echo "no $native/libquakevr.so: run Android/build_native.sh first" >&2; exit 1; }

mkdir -p "$out"
native="$(cd "$native" && pwd)"
out="$(cd "$out" && pwd)" # absolute: the jar step below runs in the stage folder
stage="$out/stage"
classes="$out/classes"
mkdir -p "$stage/lib/arm64-v8a" "$classes"

# The Java: SDL2's activity and ours, against the platform's android.jar (Java 8 class files: what d8 takes everywhere).
find "$QVR_SDL2_DIR/android-project/app/src/main/java" "$here/app/src/main/java" -name '*.java' > "$out/sources.txt"
win_sources="$out/sources_win.txt"
: > "$win_sources"
while read -r f; do win "$f" >> "$win_sources"; done < "$out/sources.txt"
# (core-lambda-stubs.jar: java.lang.invoke's lambda factory, which android.jar lacks; d8 desugars the lambdas)
sep=":"
[ -n "$exe" ] && sep=";"
"$javac" -nowarn -Xlint:-options -encoding UTF-8 -source 8 -target 8 \
	-bootclasspath "$(win "$androidJar")$sep$(win "$bt/core-lambda-stubs.jar")" \
	-d "$(win "$classes")" "@$(win "$win_sources")"
find "$classes" -name '*.class' > "$out/classes.txt"
win_classes="$out/classes_win.txt"
: > "$win_classes"
while read -r f; do win "$f" >> "$win_classes"; done < "$out/classes.txt"
"$java" -cp "$(win "$bt/lib/d8.jar")" com.android.tools.r8.D8 --release --min-api 29 --lib "$(win "$androidJar")" \
	--output "$(win "$stage")" "@$(win "$win_classes")"

# The libraries, stripped (the unstripped ones stay in the build folder: a crash report's addresses resolve there).
"$strip" --strip-unneeded -o "$(win "$stage/lib/arm64-v8a/libquakevr.so")" "$(win "$native/libquakevr.so")"
"$strip" --strip-unneeded -o "$(win "$stage/lib/arm64-v8a/libSDL2.so")" "$(win "$native/sdl2/libSDL2.so")"
# Khronos' OpenXR loader (C4a: the build copies it beside libquakevr.so when it has OpenXR), as Khronos ships it.
if [ -f "$native/libopenxr_loader.so" ]; then
	cp "$native/libopenxr_loader.so" "$stage/lib/arm64-v8a/libopenxr_loader.so"
fi
if [ -n "${QVR_STEAMAUDIO_DIR:-}" ] && [ -f "$QVR_STEAMAUDIO_DIR/lib/android-armv8/libphonon.so" ]; then
	cp "$QVR_STEAMAUDIO_DIR/lib/android-armv8/libphonon.so" "$stage/lib/arm64-v8a/libphonon.so"
fi

# The manifest (its package named here: Gradle takes it from the module's namespace) and the package.
# QVR_ANDROID_FLAT=1: the flat-panel APK (the manifest's vr-only lines left out: no VR category or head tracking).
# A release: not debuggable (the manifest's development line turned off: no run-as, no debugger, ART's and JNI's
# checks off).
vrOnly='p'
[ "${QVR_ANDROID_FLAT:-0}" = "1" ] && vrOnly='/vr-only/d;p'
edits=(-e 's|<manifest |<manifest package="com.vittorioromeo.quakevr" |')
[ "$release" = "1" ] && edits+=(-e 's|android:debuggable="true"|android:debuggable="false"|')
sed -n -e "$vrOnly" "$here/app/src/main/AndroidManifest.xml" | sed "${edits[@]}" > "$out/AndroidManifest.xml"
if [ "$release" = "1" ] && ! grep -q 'android:debuggable="false"' "$out/AndroidManifest.xml"; then
	echo "the release manifest is still debuggable (AndroidManifest.xml's android:debuggable line changed?)" >&2; exit 1
fi
"$bt/aapt2$exe" link -o "$(win "$out/unaligned.apk")" -I "$(win "$androidJar")" \
	--manifest "$(win "$out/AndroidManifest.xml")" \
	--min-sdk-version 29 --target-sdk-version 32 --version-code "$versionCode" --version-name "$versionName" \
	--replace-version
(cd "$stage" && "$jar" -u -M -f "$(win "$out/unaligned.apk")" classes.dex lib)
"$bt/zipalign$exe" -f -p 4 "$(win "$out/unaligned.apk")" "$(win "$out/aligned.apk")"

# One debug key for every APK of a build folder (the flat and the VR APK replace each other on the device with
# `adb install -r`: a different key would need an uninstall, which deletes the app's files folder, the game data).
if [ "$sign" = "release" ]; then
	"$java" -jar "$(win "$bt/lib/apksigner.jar")" sign --ks "$(win "$QVR_ANDROID_RELEASE_KEYSTORE")" \
		--ks-pass env:QVR_ANDROID_RELEASE_STOREPASS --key-pass env:QVR_ANDROID_RELEASE_KEYPASS \
		--ks-key-alias "${QVR_ANDROID_RELEASE_ALIAS:-quakevr}" --out "$(win "$out/QuakeVR.apk")" "$(win "$out/aligned.apk")"
else
	keystore="${QVR_ANDROID_KEYSTORE:-$native/debug.keystore}"
	[ -f "$keystore" ] || [ ! -f "$native/apk/debug.keystore" ] || cp "$native/apk/debug.keystore" "$keystore"
	if [ ! -f "$keystore" ]; then
		"$keytool" -genkeypair -keystore "$(win "$keystore")" -storepass android -alias androiddebugkey -keypass android \
			-keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null
	fi
	"$java" -jar "$(win "$bt/lib/apksigner.jar")" sign --ks "$(win "$keystore")" --ks-pass pass:android \
		--key-pass pass:android --ks-key-alias androiddebugkey --out "$(win "$out/QuakeVR.apk")" "$(win "$out/aligned.apk")"
fi
"$java" -jar "$(win "$bt/lib/apksigner.jar")" verify "$(win "$out/QuakeVR.apk")"
kindText="development"
[ "$release" = "1" ] && kindText="release"
[ "$release" = "1" ] && [ "$sign" = "debug" ] && kindText="release build, DEBUG KEY (a test APK: never publish it)"
echo "packaged: $out/QuakeVR.apk ($kindText, $versionName, version code $versionCode)"
