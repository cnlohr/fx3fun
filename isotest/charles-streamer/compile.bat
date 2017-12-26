set TCC=C:\tcc\tcc.exe
set CFLAGS=-DTCC -DWINDOWS -DHIDAPI -DWIN32 -Os
set LDFLAGS=-s  -lkernel32 -lgdi32 -luser32 -lsetupapi -ldbghelp

rem Stole LDFLAGS/CFLAGS from libsurvive.

%TCC% -o teststream.exe teststream.c %CFLAGS% %LDFLAGS%