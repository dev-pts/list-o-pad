#include <stdlib.h>
#include <string.h>

#include <FileMap.h>
#include <LOP.h>

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(*(a)))

static int cb_expr(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	printf("( ");
	LOP_cb_default(hl, n, param, NULL);
	printf(") ");
	return 0;
}

static int cb_define(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	printf("define ");
	LOP_cb_default(hl, n, param, NULL);
	return 0;
}

static int cb_lambda(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	printf("( lambda ( ");
	LOP_cb_default(hl, n, param, NULL);
	printf(") ");
	return 0;
}

static int cb_lambda_close(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	LOP_cb_default(hl, n, param, NULL);
	printf(") ");
	return 0;
}

static int cb_symbol(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	printf("%s ", LOP_symbol_value(n));
	return 0;
}

static int cb_string(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	printf("\"%s\" ", LOP_symbol_value(n));
	return 0;
}

static int cb_operator(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	const char *val = LOP_symbol_value(n);

	if (!strcmp(val, "=")) {
		printf("setq");
	} else if (!strcmp(val, "==")) {
		printf("eq?");
	} else if (!strcmp(val, "&&")) {
		printf("and");
	} else if (!strcmp(val, "||")) {
		printf("or");
	} else if (!strcmp(val, "!")) {
		printf("not");
	} else {
		printf("%s", val);
	}
	printf(" ");
	return 0;
}

static int cb_if(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	printf("if ");
	LOP_handler_eval(hl, 1, param);
	LOP_handler_eval(hl, 2, param);
	return 0;
}

static int resolve(struct LOP *lop, const char *key, struct LOP_CB *cb)
{
	static struct {
		const char *key;
		LOP_handler_t handler;
	} entries[] = {
		{ "expr", cb_expr },
		{ "define", cb_define },
		{ "lambda", cb_lambda },
		{ "lambda_close", cb_lambda_close },
		{ "symbol", cb_symbol },
		{ "string", cb_string },
		{ "operator", cb_operator },
		{ "if", cb_if },
	};

	for (int i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!strcmp(key, entries[i].key)) {
			cb->func = entries[i].handler;
			return 0;
		}
	}
	return -1;
}

int main(int argc, char *argv[])
{
	struct LOP lop = {
		.resolve = resolve,
		.error_cb = LOP_default_error_cb,
	};
	struct FileMap schema = map_file(argv[1]);
	struct FileMap source = map_file(argv[2]);

	if (!LOP_schema_init(&lop, schema.data)) {
		LOP_schema_parse_source(NULL, &lop, source.data, argv[3]);
	}
	LOP_schema_deinit(&lop);

	printf("\n");

	unmap_file(schema);
	unmap_file(source);
	return 0;
}
