CFLAGS := -Wall -I../../include
LDFLAGS := -L../../
LDLIBS := -llop

all: fancy-lisp lisp
	LD_LIBRARY_PATH=../../ ./fancy-lisp fancy-lisp.schema hanoi.lop top | ./lisp

fancy-lisp: fancy-lisp.o
	$(LINK.c) $^ $(LDLIBS) -o $@

lisp: lisp.o
	$(LINK.c) $^ -o $@

clean:
	rm -f *.o
	rm -f lisp
	rm -f fancy-lisp
