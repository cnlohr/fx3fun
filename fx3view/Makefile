all : fx3view

CYPERIO:=../libcyprio
CYPERIO_C:=$(CYPERIO)/libcyprio.c $(CYPERIO)/libcyprio_util.c

CFLAGS := -I$(CYPERIO) -Irawdraw -Os
LDFLAGS:= -lm -lX11 -lXext -lusb-1.0 -lpthread

rawdraw/CNFGFunctions.c :
	echo "Did you forget to checkout submodules?"
	echo "I'll try doing it for you."
	git submodule update --init --recursive

fx3view : fx3view.c rawdraw/CNFGFunctions.c rawdraw/CNFGXDriver.c $(CYPERIO_C)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~ fx3view

