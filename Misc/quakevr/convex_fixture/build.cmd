@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
set CC="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe"
set FLAGS=/nologo /O2 /MD /DNDEBUG /DZA_STATIC /Gy /Gw /clang:-ffp-contract=off /IQuake/vr /IQuake/vr/external /IQuake/vr/external/box3d/include /IQuake/vr/external/zancle/include /Iscratch/gunhullopt %1
%CC% %FLAGS% /clang:-std=c++23 /c scratch/gunhullopt/baseline_fixture.cpp /Ddecompose=baselineDecompose /Dhull=baselineHull /Dbuild=baselineBuild /DcutsSite=baselineCutsSite /DmakeBuild=baselineMakeBuild /Foscratch/gunhullopt/baseline.obj
if errorlevel 1 exit /b 1
%CC% %FLAGS% /clang:-std=c++23 /c scratch/gunhullopt/optimized_fixture.cpp /Foscratch/gunhullopt/optimized.obj
if errorlevel 1 exit /b 1
%CC% %FLAGS% /clang:-std=c++23 /c scratch/gunhullopt/fixture.cpp /Foscratch/gunhullopt/fixture.obj
if errorlevel 1 exit /b 1
for %%s in (hull core math_functions simd distance) do (
    %CC% %FLAGS% /c Quake/vr/external/box3d/src/%%s.c /Foscratch/gunhullopt/%%s.obj
    if errorlevel 1 exit /b 1
)
%CC% /nologo /clang:-fuse-ld=lld scratch/gunhullopt/baseline.obj scratch/gunhullopt/optimized.obj scratch/gunhullopt/fixture.obj scratch/gunhullopt/hull.obj scratch/gunhullopt/core.obj scratch/gunhullopt/math_functions.obj scratch/gunhullopt/simd.obj scratch/gunhullopt/distance.obj Windows/VisualStudio/Build-zancle/bin/x64/Release-Tracy/zancle.lib /Fescratch/gunhullopt/fixture.exe /link /OPT:REF /OPT:ICF /INCREMENTAL:NO
