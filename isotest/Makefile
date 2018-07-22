all : isotest recstream

LCYPRIO:=../libcyprio
CFLAGS:=-I$(LCYPRIO) -Os
LDFLAGS:=-lusb-1.0 -lpthread

CYPRIO:=$(LCYPRIO)/libcyprio.c $(LCYPRIO)/libcyprio_util.c

isotest : teststream.c os_generic.c $(CYPRIO)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

recstream : recstream.c os_generic.c $(CYPRIO)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~ isotest recstream

