set TCC=C:\tcc\tcc.exe
set CFLAGS=-DTCC -DWINDOWS -DHIDAPI -DWIN32 -Os -I. -I../libcyprio -Werror
set LDFLAGS=-s  -lkernel32 -lgdi32 -luser32 -lsetupapi -ldbghelp

del cyprflash.exe
%TCC% -o cyprflash.exe cyprflash.c ../libcyprio/libcyprio.c ../libcyprio/libcyprio_util.c %CFLAGS% %LDFLAGS%

cyprflash.exe -i -f streaming_test.img
