SRCS := src/TextToAST.c src/ASTSchema.c util/FileMap.c
OBJS := $(SRCS:.c=.o)

CFLAGS := -Wall -Iinclude/ -fPIC

all: liblop.so liblop.a test/lop-verify

liblop.so: $(OBJS)
	$(LINK.c) -shared $^ -o $@

liblop.a: $(OBJS)
	ar rcs $@ $^

test/lop-verify: test/lop-verify.o liblop.a
	$(LINK.c) $^ liblop.a -o $@

clean:
	rm -f src/*.o
	rm -f util/*.o
	rm -f test/*.o
	rm -f liblop.*
	rm -f test/lop-verify
