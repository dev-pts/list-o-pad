SRCS := src/TextToASTv2.c src/AST.c src/ASTSchema.c util/FileMap.c
OBJS := $(SRCS:.c=.o)

CFLAGS := -Wall -O2 -Iinclude/ -fPIC

all: liblop.so liblop.a test/lop-schema test/lop-ast

src/ASTSchema.o: src/ASTSchema.c src/RootSchema.c src/ErrorReport.c src/KV.c
src/TextToASTv2.o: src/TextToASTv2.c src/lex.yy.c src/ErrorReport.c
src/lex.yy.c: src/lop.l
	flex -o $@ $^

liblop.so: $(OBJS)
	$(LINK.c) -shared $^ -o $@

liblop.a: $(OBJS)
	ar rcs $@ $^

test/lop-schema: test/lop-schema.o liblop.a
	$(LINK.c) $^ liblop.a -o $@

test/lop-ast: test/lop-ast.o liblop.a
	$(LINK.c) $^ liblop.a -o $@

clean:
	rm -f src/*.o
	rm -f src/lex.yy.c
	rm -f util/*.o
	rm -f test/*.o
	rm -f liblop.*
	rm -f test/lop-schema
	rm -f test/lop-ast
