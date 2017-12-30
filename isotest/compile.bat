set TCC=C:\tcc\tcc.exe
set CFLAGS=-DTCC -DWINDOWS -DHIDAPI -DWIN32 -Os -I. -I../libcyprio
set LDFLAGS=-s  -lkernel32 -lgdi32 -luser32 -lsetupapi -ldbghelp

del teststream.exe
%TCC% -o teststream.exe teststream.c ../libcyprio/libcyprio.c os_generic.c %CFLAGS% %LDFLAGS%
teststream.exe