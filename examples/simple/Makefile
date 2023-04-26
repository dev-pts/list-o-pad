CFLAGS := -Wall -I../../include
LDFLAGS := -L../../
LDLIBS := -llop

all: simple
	LD_LIBRARY_PATH=../../ ./simple simple.schema simple.lop top

simple: simple.o
	$(LINK.c) $^ $(LDLIBS) -o $@

clean:
	rm -f *.o
	rm -f simple
