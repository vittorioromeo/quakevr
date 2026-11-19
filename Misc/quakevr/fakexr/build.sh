#!/bin/bash
# build.sh <worktree> -- builds the fake OpenXR runtime (fakexr.cpp) into <worktree>/scratch/fakexr and makes the fake
# runtimes the tests of vr_xr_runtime's Auto choice load (Misc/quakevr/xr_runtime_test.sh): one copy of the library per
# runtime, each with a manifest named as the real one's (the game tells runtimes apart by their manifests' names).
#   scratch/fakexr/vd/virtualdesktop-openxr.json  -> fakexr_vd.dll    (Linux: .so)
#   scratch/fakexr/steam/steamxr_win64.json       -> fakexr_steam.dll
#   scratch/fakexr/meta/oculus_openxr_64.json     -> fakexr_meta.dll
#   scratch/fakexr/other/other_openxr.json        -> fakexr_other.dll
# Windows (Git's bash): MSBuild with fakexr.vcxproj. Linux: the C++ compiler (CXX, else c++), with the OpenXR headers
# (pkg-config openxr) and libGL: Misc/quakevr/xr_session_test.sh's runs load it through XR_RUNTIME_JSON.
TREE="${1:?usage: build.sh <worktree>}"
OUT="$TREE/scratch/fakexr"
mkdir -p "$OUT/obj" "$OUT/vd" "$OUT/steam" "$OUT/meta" "$OUT/other"
case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*)
        WOUT=$(cygpath -w "$OUT")
        "/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" \
            "$(cygpath -w "$TREE/Misc/quakevr/fakexr/fakexr.vcxproj")" -p:Configuration=Release -p:Platform=x64 \
            "-p:OutDir=$WOUT\\" "-p:IntDir=$WOUT\\obj\\" -v:q -nologo > "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; echo "FAKEXR BUILD FAILED"; exit 1; }
        grep -E "warning|error" "$OUT/build.log" | head -10
        ext=dll
        libpath() { cygpath -w "$1" | sed 's/\\/\\\\/g'; }
        ;;
    *)
        WOUT="$OUT"
        # shellcheck disable=SC2046
        "${CXX:-c++}" -std=c++17 -O2 -Wall -shared -fPIC -fvisibility=hidden $(pkg-config --cflags openxr 2>/dev/null) \
            -o "$OUT/fakexr.so" "$TREE/Misc/quakevr/fakexr/fakexr.cpp" -lGL -ldl > "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; echo "FAKEXR BUILD FAILED"; exit 1; }
        grep -E "warning|error" "$OUT/build.log" | head -10
        ext=so
        libpath() { echo "$1"; }
        ;;
esac
for kind in vd:virtualdesktop-openxr steam:steamxr_win64 meta:oculus_openxr_64 other:other_openxr; do
    dir="${kind%%:*}"; json="${kind#*:}.json"
    cp -f "$OUT/fakexr.$ext" "$OUT/fakexr_$dir.$ext"
    lib=$(libpath "$OUT/fakexr_$dir.$ext")
    printf '{\n  "file_format_version": "1.0.0",\n  "runtime": {\n    "library_path": "%s",\n    "name": "FakeXR %s"\n  }\n}\n' "$lib" "$dir" > "$OUT/$dir/$json"
done
echo "fakexr built: $WOUT"
