# The Quest's release APK and its signing key

How the Quest APK is built for a release, what differs from the development APK, and the release key's one-time setup.
Build tools and the development APK: Android/README.md. The release script: RELEASING.md ("Prereleases", `-Apk build`).

## Development APK and release APK

Betas 1 and 2 shipped the development APK. What that meant (agent/apkrelease, 2026-11-23):

| | Development APK (`build_native.sh`, `package_apk.sh`) | Release APK (`QVR_ANDROID_RELEASE=1`) |
|---|---|---|
| Native optimisation | `-O3 -DNDEBUG` (the NDK's Release), vr_menu.cpp `-Os` | the same |
| `QVR_RELEASE` (as the PC's release) | off: live tunables (each a load, not a folded constant), every sound channel read checked by a function call (`S_MixCheckAt`, q_sound.h), "-dev" in the version | on |
| Link-time optimisation | none | ThinLTO (engine, its libraries, SDL2; not vr_menu.cpp) |
| `android:debuggable` | true: ART's debuggable mode and CheckJNI on every JNI call, `run-as`, a debugger can attach | false |
| Stripped in the APK | yes (`llvm-strip --strip-unneeded`; the unstripped library stays in the build folder) | the same |
| QuakeC | not in the APK: the game folder's `progs.dat`. Setup installs the package's (the release flavour, `-DQVR_NO_TESTS`); a developer's `adb push` of a checkout has the tests' | the same |
| Tracy, `-g` | no Tracy (compiled out without `QVR_TRACY`); `-g` in the objects, stripped from the APK | the same |
| Signed with | the build folder's debug key | the release key |
| Version | name 1.1.0 and code 1 whatever VERSION said (the manifest's: `aapt2` kept them); now VERSION's name, code 1 | VERSION's name, code from VERSION (1.2.0-beta3 is 1020003, 1.2.0 is 1020099) |

What can cost frame time: `QVR_RELEASE` and ThinLTO (CPU, the main thread: a few percent expected); debuggable (CheckJNI
on SDL's JNI calls, which are few a frame in VR: well under 1%). None of it touches the GPU, and the gib fight on the
Quest 3 is GPU-bound (questgore, questperf: GPU ~16-18 ms, CPU well under), so expect its frame rate unchanged.
Measured: "Measurements" below.

## Building one

    Android/release_apk.sh build      # build-android-release/apk/QuakeVR.apk; the unstripped libquakevr.so beside it
    make_release.ps1 ... -Apk build    # the same inside a release, after its version commit (RELEASING.md)

`release_apk.sh build` is `QVR_ANDROID_RELEASE=1` for `build_native.sh` (CMake `QVR_RELEASE=ON`, `QVR_ANDROID_LTO=ON`,
its own folder `build-android-release`) and `package_apk.sh` (not debuggable, the version code, the release key), then
`check_apk.sh <apk> vr release`: not debuggable, a version code from VERSION, the APK's `libquakevr.so` stripped, a release
engine (no "-dev", no live tuning), the signer not a debug key and, once `Android/release-cert.sha256` exists, exactly the
release certificate. It finds the tools as Android/README.md sets them up (F:/QuakeVRDev, or `QVR_ANDROID_DEV_DIR`).

A release build signed with the debug key, to test on a headset that has a development build (an APK signed with
another key cannot replace it without an uninstall, which deletes the game's files and saves):

    QVR_ANDROID_RELEASE=1 Android/build_native.sh
    QVR_ANDROID_RELEASE=1 QVR_ANDROID_SIGN=debug QVR_ANDROID_KEYSTORE=<the headset's debug.keystore> Android/package_apk.sh
    Android/check_apk.sh build-android-release/apk/QuakeVR.apk vr release-testkey

Never publish that one (`check_apk.sh ... release` fails it: a debug signer).

## The release key: one-time setup (Vittorio)

Nobody but you creates or types its password; no script stores it, the repository never holds the key or the password.

1. **Create the key** (keytool asks for the password twice; PKCS12, so the key's password is the store's):

        mkdir "%USERPROFILE%\.quakevr"
        F:\QuakeVRDev\jdk-17\bin\keytool -genkeypair -v -keystore "%USERPROFILE%\.quakevr\quakevr-release.p12" ^
            -storetype PKCS12 -alias quakevr -keyalg RSA -keysize 4096 -validity 36500 -dname "CN=Quake VR"

2. **Keep it and back it up.** The keystore stays outside the repository (`%USERPROFILE%\.quakevr\`, which
   `package_apk.sh` accepts; it refuses a keystore inside the repository). Put a copy of `quakevr-release.p12` and its
   password in your password manager (as an attachment) and a second copy offline (e.g. an encrypted USB stick). **If
   the key or its password is lost, no release can ever update the installed game again**: every player would have to
   uninstall (losing saves) to move to a new key.

3. **Record its fingerprint** (public, safe to commit; `check_apk.sh` then checks every release APK's signer against it):

        F:\QuakeVRDev\jdk-17\bin\keytool -list -v -keystore "%USERPROFILE%\.quakevr\quakevr-release.p12" -alias quakevr

   Copy the "SHA256:" value into `Android/release-cert.sha256` (one line, colons are fine) and commit it.

4. **Give the scripts the password when releasing**, one of:
   - for the session only (PowerShell, before `make_release.ps1`; it is typed, not stored):

         $env:QVR_ANDROID_RELEASE_KEYSTORE = "$env:USERPROFILE\.quakevr\quakevr-release.p12"
         $env:QVR_ANDROID_RELEASE_STOREPASS = Read-Host "Quake VR release key password"

     (bash: `export QVR_ANDROID_RELEASE_KEYSTORE=...; read -rs QVR_ANDROID_RELEASE_STOREPASS; export QVR_ANDROID_RELEASE_STOREPASS`)
   - or a file `%USERPROFILE%\.quakevr\android-release.env` (or the path in `QVR_ANDROID_RELEASE_ENV`), readable by you
     alone, with lines `QVR_ANDROID_RELEASE_KEYSTORE=C:/Users/<you>/.quakevr/quakevr-release.p12` and
     `QVR_ANDROID_RELEASE_STOREPASS=<password>` (optional: `QVR_ANDROID_RELEASE_KEYPASS`, `QVR_ANDROID_RELEASE_ALIAS`,
     default `quakevr`).

   Without them `package_apk.sh` (and `make_release.ps1 -Apk build`, before it builds anything) stops and says which
   variable is missing. The password reaches `apksigner` through the environment (`--ks-pass env:...`), never a command
   line.

## What the key commits you to

- **Every future release must be signed with this key.** Android refuses an update signed with another key
  (`INSTALL_FAILED_UPDATE_INCOMPATIBLE`; Setup's Quest card then says to uninstall in the headset first).
- **Players of betas 1 and 2** have the development APK (a debug key). Moving to the first release-key APK needs one
  uninstall and reinstall, which deletes the app's folder: the game data (Setup copies it again) **and the saves and
  settings**. Say so in that release's notes, with the backup steps below. After that, updates install in place.
- **Development headsets** (yours, the agents' quest tree) keep development APKs, debug key: a release-key APK there would
  need the same uninstall, and back. Test release builds on them with the debug-key test APK above.

### Backing up the saves before the uninstall (betas 1 and 2: a development APK)

What the game writes in its folder (saves, `autosave/`, its config) is owned by the app, 0660: adb's shell can list it
but not read it, and `run-as` can't enter the external folder either (checked on the Quest 3, 2026-11-23). The beta's
own `vr_android_export` (Debug > Startup) copies a folder's files into the app's internal `export/`, which `run-as`
reads on a debuggable APK. With the headset connected (USB debugging on, as for Setup's Quest install):

    adb shell am start -n com.vittorioromeo.quakevr/.QuakeVRActivity --es args "'+vr_android_export . +vr_android_export autosave +quit'"
    adb exec-out run-as com.vittorioromeo.quakevr tar -cf - files/export > quest-saves.tar

(`.`: the game folder's own files, the saves and config among them, flattened into `export/` with `autosave/`'s.) Then
uninstall in the headset (Library > Unknown Sources > Quake VR > Uninstall), install the new release with Setup, unpack
`quest-saves.tar` on the PC and push the saves back, e.g. for each `.sav`:

    adb push <name>.sav /sdcard/Android/data/com.vittorioromeo.quakevr/files/quakevr/<name>.sav
    adb shell chmod 0666 /sdcard/Android/data/com.vittorioromeo.quakevr/files/quakevr/<name>.sav

(adb's copies need the `chmod` for the game to read and write them, as Android/README.md does for the game data.)
A release APK is not debuggable: this route does not work on it, so back up before the switch.

## Measuring a release APK

`run-as` can't read a non-debuggable app's files, so the perf CSV (`vr_perf_record`) stays on the headset. Each
`vr_perf_mark` now also prints the frames since the previous mark on the console (and so logcat): frames, CPU busy median
and p90, period median, GPU median and p90, frames the runtime reported late. The same lines come from the development APK.

## Measurements
