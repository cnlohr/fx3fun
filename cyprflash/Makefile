all : cyprflash

LCYPRIO:=../libcyprio
CFLAGS:=-I$(LCYPRIO) -Os
LDFLAGS:=-lusb-1.0

CYPRIO:=$(LCYPRIO)/libcyprio.c $(LCYPRIO)/libcyprio_util.c

cyprflash : cyprflash.c $(CYPRIO)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~ cyprflash

