set TCC=C:\tcc\tcc.exe
set CFLAGS=-DTCC -DWINDOWS -DHIDAPI -DWIN32 -Os -I.
set LDFLAGS=-s  -lkernel32 -lgdi32 -luser32 -lsetupapi -ldbghelp

rem Stole LDFLAGS/CFLAGS from libsurvive.

del teststream.exe
%TCC% -o teststream.exe teststream.c CyprIO.c os_generic.c %CFLAGS% %LDFLAGS%
teststream.exe