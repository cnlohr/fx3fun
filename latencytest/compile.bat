set CFLAGS=-DTCC -DWINDOWS -DHIDAPI -DWIN32 -O2 -m64 -I. -I../libcyprio -Irawdraw -g -rdynamic -DTCCCRASH_STANDALONE
set LDFLAGS=-s  C:/windows/system32/kernel32.dll -lgdi32 -luser32 -lsetupapi -lws2_32 -ldbghelp C:\\windows\\system32\\opengl32.dll C:\\windows\\system32\msvcrt.dll
del latencytest.exe
C:\tcc\tcc latencytest.c ../libcyprio/libcyprio.c -o latencytest.exe %CFLAGS% %LDFLAGS%

