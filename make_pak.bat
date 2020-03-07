C:
cd C:\OHWorkspace\quakevr\QC
.\frikqcc.exe -summary -nolog -nopause -Ot -Oi -Op -Oc -Od -Os -Ol -On -Of -Ou -Oo -Or -Oa +warn 2 -cos
pak.exe -c -v pak3.pak progs.dat
:: xcopy /y C:\OHWorkspace\quakevr\QC\pak3.pak C:\OHWorkspace\quakevr\Windows\VisualStudio\Build-quakespasm-sdl2\x64\Debug\Id1
xcopy /y C:\OHWorkspace\quakevr\QC\pak3.pak C:\OHWorkspace\quakevr\ReleaseFiles\Id1
