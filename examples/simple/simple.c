#include <stdlib.h>
#include <string.h>

#include <FileMap.h>
#include <LOP.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

typedef int (*cb_t)(struct LOP_ASTNode *n, int delta);

static struct LOP_HandlerList *hl;
static int hl_index;

static cb_t resolve(int index);

static int result;

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

static int cb_num(struct LOP_ASTNode *n, int delta)
{
	result = atoi(LOP_symbol_value(n));
	return 0;
}

static int cb_bin_minus(struct LOP_ASTNode *n, int delta)
{
	eval_node();
	result = -result;
	return 0;
}

static int cb_bin_plus(struct LOP_ASTNode *n, int delta)
{
	eval_node();
	result = +result;
	return 0;
}

static int cb_plus(struct LOP_ASTNode *n, int delta)
{
	eval_node();

	int tmp = result;

	eval_node();
	result = tmp + result;
	return 0;
}

static int cb_minus(struct LOP_ASTNode *n, int delta)
{
	eval_node();

	int tmp = result;

	eval_node();
	result = tmp - result;
	return 0;
}

static int cb_mul(struct LOP_ASTNode *n, int delta)
{
	eval_node();

	int tmp = result;

	eval_node();
	result = tmp * result;
	return 0;
}

static int cb_div(struct LOP_ASTNode *n, int delta)
{
	eval_node();

	int tmp = result;

	eval_node();
	result = tmp / result;
	return 0;
}

static int cb_print(struct LOP_ASTNode *n, int delta)
{
	eval_node();
	printf("%i\n", result);
	return 0;
}

static cb_t resolve(int index)
{
	static struct {
		const char *key;
		cb_t handler;
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
