#include <stdlib.h>
#include <string.h>

#include <FileMap.h>
#include <LOP.h>

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(*(a)))

static int cb_num(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	int *result = param;

	*result = atoi(LOP_symbol_value(n));
	return 0;
}

static int cb_bin_minus(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	int *result = param;

	LOP_handler_eval(hl, 1, param);
	*result = -*result;
	return 0;
}

static int cb_bin_plus(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	int *result = param;

	LOP_handler_eval(hl, 1, param);
	*result = +*result;
	return 0;
}

static int cb_plus(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	int *result = param;
	int a, b;

	LOP_handler_eval(hl, 1, &a);
	LOP_handler_eval(hl, 2, &b);
	*result = a + b;
	return 0;
}

static int cb_minus(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	int *result = param;
	int a, b;

	LOP_handler_eval(hl, 1, &a);
	LOP_handler_eval(hl, 2, &b);
	*result = a - b;
	return 0;
}

static int cb_mul(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	int *result = param;
	int a, b;

	LOP_handler_eval(hl, 1, &a);
	LOP_handler_eval(hl, 2, &b);
	*result = a * b;
	return 0;
}

static int cb_div(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	int *result = param;
	int a, b;

	LOP_handler_eval(hl, 1, &a);
	LOP_handler_eval(hl, 2, &b);
	*result = a / b;
	return 0;
}

static int cb_print(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	int result;

	LOP_handler_eval(hl, 0, &result);
	printf("%i\n", result);
	return 0;
}

static int resolve(struct LOP *lop, const char *key, struct LOP_CB *cb)
{
	static struct {
		const char *key;
		LOP_handler_t handler;
	} entries[] = {
		{ "num", cb_num },
		{ "bin_minus", cb_bin_minus },
		{ "bin_plus", cb_bin_plus },
		{ "plus", cb_plus },
		{ "minus", cb_minus },
		{ "mul", cb_mul },
		{ "div", cb_div },
		{ "print", cb_print },
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

	if (!LOP_schema_init(&lop, schema.data, schema.len)) {
		LOP_schema_parse_source(NULL, &lop, source.data, source.len, argv[3]);
	}

	LOP_schema_deinit(&lop);

	unmap_file(schema);
	unmap_file(source);
	return 0;
}
