C:
cd C:\OHWorkspace\quakevr\QC
.\fteqcc64.exe -O3 -Fautoproto
.\pak.exe -c -v pak3.pak progs.dat
xcopy /y C:\OHWorkspace\quakevr\QC\pak3.pak C:\OHWorkspace\quakevr\ReleaseFiles\Id1

:: xcopy /y C:\OHWorkspace\quakevr\QC\pak3.pak C:\OHWorkspace\quakevr\Windows\VisualStudio\Build-quakespasm-sdl2\x64\Debug\Id1
