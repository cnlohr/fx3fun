all : latencytest

LCYPRIO:=../libcyprio
CFLAGS:=-I$(LCYPRIO) -O1 -g
LDFLAGS:=-lusb-1.0 -lpthread

CYPRIO:=$(LCYPRIO)/libcyprio.c $(LCYPRIO)/libcyprio_util.c

latencytest : latencytest.c $(CYPRIO)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~ latencytest
