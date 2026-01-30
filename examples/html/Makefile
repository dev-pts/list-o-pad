CFLAGS := -Wall -I../../include
LDFLAGS := -L../../
LDLIBS := -llop

all: html
	@LD_LIBRARY_PATH=../../ ./html html.schema html.lop top

html: html.o
	$(LINK.c) $^ $(LDLIBS) -o $@

clean:
	rm -f *.o
	rm -f html
