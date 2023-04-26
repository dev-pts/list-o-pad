CFLAGS := -Wall -I../../include
LDFLAGS := -L../../
LDLIBS := -llop

all: config
	LD_LIBRARY_PATH=../../ ./config config.schema config.lop top

config: config.o
	$(LINK.c) $^ $(LDLIBS) -o $@

clean:
	rm -f *.o
	rm -f config
