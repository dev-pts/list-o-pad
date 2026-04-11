#include <stdlib.h>
#include <string.h>

#include <FileMap.h>
#include <LOP.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

typedef int (*cb_t)(struct LOP_ASTNode *n, int delta);

static struct LOP_HandlerList *hl;
static int hl_index;

static cb_t resolve(int index);

static int node_inside(void)
{
	if (hl_index >= hl->count) {
		return 0;
	}

	return hl->handler[hl_index].delta != -1;
}

static int eval_node(void)
{
	if (hl_index >= hl->count) {
		return -1;
	}

	cb_t cb = resolve(hl_index);
	struct LOP_ASTNode *n = hl->handler[hl_index].n;
	int delta = hl->handler[hl_index].delta;

	hl_index++;

	if (hl_index >= hl->count) {
		return -1;
	}

	if (cb) {
		int rc = cb(n, delta);

		if (rc) {
			return rc;
		}
	}

	if (delta > 0) {
		hl_index++;
	}

	return 0;
}

static int cb_expr(struct LOP_ASTNode *n, int delta)
{
	printf("( ");
	while (node_inside()) {
		eval_node();
	}
	printf(") ");
	return 0;
}

static int cb_define(struct LOP_ASTNode *n, int delta)
{
	printf("( define ");
	while (node_inside()) {
		eval_node();
	}
	printf(") ");
	return 0;
}

static int cb_lambda(struct LOP_ASTNode *n, int delta)
{
	eval_node();
	printf("( lambda ");
	while (node_inside()) {
		eval_node();
	}
	printf(") ");
	return 0;
}

static int cb_symbol(struct LOP_ASTNode *n, int delta)
{
	printf("%s ", LOP_symbol_value(n));
	return 0;
}

static int cb_string(struct LOP_ASTNode *n, int delta)
{
	printf("\"%s\" ", LOP_symbol_value(n));
	return 0;
}

static int cb_operator(struct LOP_ASTNode *n, int delta)
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

static int cb_if(struct LOP_ASTNode *n, int delta)
{
	printf("( if ");
	while (node_inside()) {
		eval_node();
	}
	printf(") ");
	return 0;
}

static cb_t resolve(int index)
{
	static struct {
		const char *key;
		cb_t handler;
	} entries[] = {
		{ "expr", cb_expr },
		{ "define", cb_define },
		{ "lambda", cb_lambda },
		{ "symbol", cb_symbol },
		{ "string", cb_string },
		{ "operator", cb_operator },
		{ "if", cb_if },
	};

	for (int i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!strcmp(hl->handler[index].key, entries[i].key)) {
			return entries[i].handler;
		}
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	struct LOP_Schema schema = {
		.filename = argv[1],
	};
	struct FileMap schema_str = map_file(argv[1]);
	struct FileMap source = map_file(argv[2]);

	if (!LOP_schema_init(&schema, schema_str.data, schema_str.len)) {
		struct LOP lop = {
			.schema = &schema,
			.top_rule_name = argv[3],
			.filename = argv[2],
		};

		LOP_init(&lop, source.data, source.len);

		hl = &lop.hl;

		while (node_inside()) {
			eval_node();
		}

		LOP_deinit(&lop);
	}
	LOP_schema_deinit(&schema);

	unmap_file(schema_str);
	unmap_file(source);
	return 0;
}
