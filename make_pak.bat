cd QC

.\fteqcc64.exe -O3 -Fautoproto -progdefs -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208

.\pak.exe -c -v pak11.pak vrprogs.dat
xcopy /y pak11.pak ..\ReleaseFiles\Id1

:: echo F | xcopy /y progdefs.h ..\Quake\progdefs_generated.hpp
