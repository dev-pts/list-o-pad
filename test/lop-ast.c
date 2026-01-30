#include <LOP.h>
#include "FileMap.h"

static int cb_dummy(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	return 0;
}

static int resolve(struct LOP *lop, const char *key, struct LOP_CB *cb)
{
	cb->func = cb_dummy;
	return 0;
}

int main(int argc, char *argv[])
{
	struct LOP_OperatorTable unary[] = {
		{ /* sentinel */ },
	};
	struct LOP_OperatorTable binary[] = {
		{ "=", 1, LOP_OPERATOR_LEFT },
		{ /* sentinel */ },
	};
	struct LOP lop = {
		.resolve = resolve,
		.error_cb = LOP_default_error_cb,
		.unary = unary,
		.binary = binary,
	};
	struct FileMap source;
	struct LOP_ASTNode *ast;
	int rc;

	source = map_file(argv[1]);

	rc = LOP_getAST(&ast, source.data, source.len, lop.unary, lop.binary, lop.error_cb);
	if (rc < 0) {
		return rc;
	}
	LOP_dump_ast(ast, true);
	LOP_delAST(ast);

	unmap_file(source);
	return 0;
}
