#include <string.h>
#include <assert.h>

#include <FileMap.h>
#include <LOP.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

typedef int (*cb_t)(struct LOP_ASTNode *n, int delta);

static struct LOP_HandlerList *hl;
static int hl_index;

static cb_t resolve(int index);
static int level;

static int handler_is(const char *key)
{
	if (hl_index >= hl->count) {
		return 0;
	}

	return strcmp(hl->handler[hl_index].key, key) == 0;
}

static int node_inside(void)
{
	if (hl_index >= hl->count) {
		return 0;
	}

	return hl->handler[hl_index].delta != -1;
}

static int eval_simple(int index)
{
	cb_t cb = resolve(index);
	struct LOP_ASTNode *n = hl->handler[index].n;
	int delta = hl->handler[index].delta;
	int prev_index = hl_index;
	int rc = 0;

	hl_index = index + 1;

	if (cb) {
		rc = cb(n, delta);
	}

	hl_index = prev_index;
	return 0;
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

static void lprint(void)
{
	for (int i = 0; i < level; i++) {
		printf("\t");
	}
}

static int cb_html(struct LOP_ASTNode *n, int delta)
{
	printf("<html");

	if (handler_is("attr_separator")) {
		eval_node();
	}
	if (node_inside()) {
		printf(">\n");
		level++;

		while (node_inside()) {
			eval_node();
		}

		level--;
		printf("</html>\n");
	} else {
		printf(" />\n");
	}
	return 0;
}

static int cb_node(struct LOP_ASTNode *n, int delta)
{
	int index = hl_index;

	lprint();
	printf("<");

	eval_node();

	if (handler_is("attr_separator")) {
		eval_node();
	}
	if (node_inside()) {
		printf(">\n");
		level++;

		while (node_inside()) {
			eval_node();
		}

		level--;
		lprint();
		printf("</");
		eval_simple(index);
		printf(">\n");
	} else {
		printf(" />\n");
	}
	return 0;
}

static int cb_print(struct LOP_ASTNode *n, int delta)
{
	printf("%s", LOP_symbol_value(n));
	return 0;
}

static int cb_attr_assign(struct LOP_ASTNode *n, int delta)
{
	eval_node();
	printf("=\"");
	eval_node();
	printf("\"");
	return 0;
}

static int cb_attr_separator(struct LOP_ASTNode *n, int delta)
{
	while (node_inside()) {
		printf(" ");
		eval_node();
	}
	return 0;
}

static int cb_iprintln(struct LOP_ASTNode *n, int delta)
{
	const char *str = LOP_symbol_value(n);

	lprint();
	while (*str) {
		if (*str == '\n') {
			printf("\n");
			lprint();
		} else {
			printf("%c", *str);
		}
		str++;
	}
	printf("\n");
	return 0;
}

static cb_t resolve(int index)
{
	static struct {
		const char *key;
		cb_t handler;
	} entries[] = {
		{ "html", cb_html },
		{ "node", cb_node },
		{ "attr_separator", cb_attr_separator },
		{ "attr_assign", cb_attr_assign },
		{ "iprintln", cb_iprintln },
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

		eval_node();

		LOP_deinit(&lop);
	}
	LOP_schema_deinit(&schema);

	unmap_file(schema_str);
	unmap_file(source);
	return 0;
}
