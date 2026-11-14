# Quake VR on Android (the Quest)

Status and plan: `docs/vr-port/QUEST_PLAN.md` (C3: this build; C4: OpenXR on the headset). With Khronos' loader
(`QVR_OPENXR_ANDROID_DIR`) the APK is an immersive OpenXR app (C4a: the ES binding, the loader's Android init, the
Quest's extensions; not booted yet); `QVR_ANDROID_FLAT=1` packages the flat-panel APK (no VR category: a 2D window on
the headset, for the first boot's files, logcat and crash checks).

## What is here

- `android.cmake`: the Android settings of the top-level `CMakeLists.txt` (`ANDROID`): the engine as
  `libquakevr.so`, SDL2 built from its source as `libSDL2.so`, GLES 3.2 + EGL, logcat, `-ffp-contract=off` on every
  target (the PC build contracts nothing either), no mimalloc, no codecs or curl yet; OpenXR when
  `QVR_OPENXR_ANDROID_DIR` names Khronos' loader AAR, unpacked (`jni/arm64-v8a/libopenxr_loader.so`, copied beside
  `libquakevr.so`; the engine's own headers, `Windows/OpenXR/include`).
- `app/`: the Gradle module: `QuakeVRActivity` (SDL2's `SDLActivity`, loading `libSDL2.so` and `libquakevr.so`; an
  `args` string extra is the command line), the manifest (GLES 3.2, the Quest models; no permission: the game's
  files live in the app's external files folder).
- `build_native.sh`: the two libraries with the NDK's toolchain, the SDK's CMake 3.22.1 and Ninja (no Gradle).
- `package_apk.sh`: the APK from those libraries with the SDK's build tools and the JDK alone (javac, d8, aapt2,
  zipalign, apksigner; a debug key made once): no Android Gradle Plugin download.

## Build

The tools (QUEST_PLAN.md section 7): JDK 17, the Android SDK (platform 34, build-tools 34.0.0, CMake 3.22.1), NDK
27.2, SDL2 2.30.x source; optionally Steam Audio's release (its `lib/android-armv8/libphonon.so`).

    source F:/QuakeVRDev/env.sh                 # JAVA_HOME, ANDROID_HOME, ANDROID_NDK_HOME, PATH
    export QVR_SDL2_DIR=F:/QuakeVRDev/SDL2-2.30.9
    export QVR_STEAMAUDIO_DIR=F:/QuakeVRDev/steamaudio/steamaudio   # optional
    export QVR_OPENXR_ANDROID_DIR=F:/QuakeVRDev/openxr/aar         # OpenXR (read at the build folder's first configure)
    Android/build_native.sh                     # build-android/libquakevr.so, build-android/sdl2/libSDL2.so
    Android/package_apk.sh                      # build-android/apk/QuakeVR.apk (+ libopenxr_loader.so)
    QVR_ANDROID_FLAT=1 Android/package_apk.sh build-android build-android/apk-flat   # the flat-panel APK

Gradle (the same APK; its first run downloads the Android Gradle Plugin and its dependencies from Google's Maven):
`gradle -p Android assembleRelease -PqvrSdl2Dir=F:/QuakeVRDev/SDL2-2.30.9`.

The QuakeC (`quakevr/progs.dat`) and the game's data are the PC's (build them on Windows).

## Install and run (C4: needs the headset in developer mode)

    adb install -r build-android/apk/QuakeVR.apk
    adb push <your Quake>/id1 /sdcard/Android/data/com.vittorioromeo.quakevr/files/id1
    adb push quakevr /sdcard/Android/data/com.vittorioromeo.quakevr/files/quakevr
    adb shell am start -n com.vittorioromeo.quakevr/.QuakeVRActivity --es args "+map e1m1"
    adb logcat -s QuakeVR SDL                   # the console; a crash's report too

A crash writes `quakevr/crash/<date>_<time>.txt` in that folder and logcat (vr_crash.cpp; the addresses resolve with
`llvm-addr2line -f -C -e build-android/libquakevr.so 0x<offset>`, the unstripped library of the same build).
