#!/usr/bin/env bash
# release_apk.sh -- the Quest's release APK in one command (docs/vr-port/ANDROID_RELEASE.md), for
# Misc/release/make_release.ps1 -Apk build and by hand:
#
#   Android/release_apk.sh build [build folder (default <repo>/build-android-release)]
#       QVR_ANDROID_RELEASE=1 build_native.sh, package_apk.sh (the release key) and check_apk.sh vr release:
#       <folder>/apk/QuakeVR.apk, the unstripped libquakevr.so beside it in <folder> (a crash report's addresses).
#   Android/release_apk.sh check <apk>
#       check_apk.sh vr release on an APK built elsewhere.
#
# The tools: the environment's ANDROID_HOME, ANDROID_NDK_HOME, JAVA_HOME, QVR_SDL2_DIR, QVR_OPENXR_ANDROID_DIR,
# QVR_STEAMAUDIO_DIR when set; else QVR_ANDROID_DEV_DIR's (default F:/QuakeVRDev, Android/README.md): env.sh,
# SDL2-2.30.9, openxr/aar, steamaudio/steamaudio. The release key: package_apk.sh's QVR_ANDROID_RELEASE_* variables
# (or QVR_ANDROID_RELEASE_ENV's file), never the repository's.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/.." && pwd)"
dev="${QVR_ANDROID_DEV_DIR:-F:/QuakeVRDev}"
if [ -z "${ANDROID_HOME:-}" ] && [ -f "$dev/env.sh" ]; then
	source "$dev/env.sh"
fi
[ -n "${QVR_SDL2_DIR:-}" ] || [ ! -d "$dev/SDL2-2.30.9" ] || export QVR_SDL2_DIR="$dev/SDL2-2.30.9"
[ -n "${QVR_OPENXR_ANDROID_DIR:-}" ] || [ ! -d "$dev/openxr/aar" ] || export QVR_OPENXR_ANDROID_DIR="$dev/openxr/aar"
[ -n "${QVR_STEAMAUDIO_DIR:-}" ] || [ ! -d "$dev/steamaudio/steamaudio" ] || export QVR_STEAMAUDIO_DIR="$dev/steamaudio/steamaudio"
what="${1:-}"
case "$what" in
	build)
		out="${2:-$repo/build-android-release}"
		[ -n "${QVR_OPENXR_ANDROID_DIR:-}" ] || { echo "QVR_OPENXR_ANDROID_DIR: Khronos' loader (the VR APK needs it)" >&2; exit 1; }
		export QVR_ANDROID_RELEASE=1
		unset QVR_ANDROID_SIGN QVR_ANDROID_FLAT QVR_ANDROID_VERSION_CODE
		"$here/build_native.sh" "$out"
		"$here/package_apk.sh" "$out"
		"$here/check_apk.sh" "$out/apk/QuakeVR.apk" vr release
		echo "release APK: $out/apk/QuakeVR.apk"
		;;
	check)
		[ -n "${2:-}" ] || { echo "release_apk.sh check <apk>" >&2; exit 2; }
		"$here/check_apk.sh" "$2" vr release
		;;
	*)
		echo "release_apk.sh build [folder] | check <apk>" >&2
		exit 2
		;;
esac
