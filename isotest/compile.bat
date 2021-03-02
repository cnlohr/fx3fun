set TCC=C:\tcc\tcc.exe
set CFLAGS=-DTCC -DWINDOWS -DHIDAPI -DWIN32 -Os -I. -I../libcyprio
set LDFLAGS=-s  C:/windows/system32/kernel32.dll -lgdi32 -luser32 -lsetupapi -ldbghelp

del teststream.exe recstream.exe

%TCC% -o teststream.exe teststream.c ../libcyprio/libcyprio.c os_generic.c %CFLAGS% %LDFLAGS%
%TCC% -o recstream.exe recstream.c ../libcyprio/libcyprio.c os_generic.c %CFLAGS% %LDFLAGS%

