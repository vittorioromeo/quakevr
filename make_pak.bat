C:
cd C:\OHWorkspace\quakevr\QC
.\fteqcc64.exe -O3 -Fautoproto -progdefs -Olo -Fiffloat -Fifvector -Fvectorlogic
echo F |xcopy /y progs.dat vrprogs.dat
.\pak.exe -c -v pak11.pak progs.dat
xcopy /y C:\OHWorkspace\quakevr\QC\pak11.pak C:\OHWorkspace\quakevr\ReleaseFiles\Id1
:: echo F | xcopy /y C:\OHWorkspace\quakevr\QC\progdefs.h C:\OHWorkspace\quakevr\Quake\progdefs_generated.hpp

:: xcopy /y C:\OHWorkspace\quakevr\QC\pak11.pak C:\OHWorkspace\quakevr\Windows\VisualStudio\Build-quakespasm-sdl2\x64\Debug\Id1
