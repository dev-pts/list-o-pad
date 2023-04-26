#include <string.h>

#include <FileMap.h>
#include <LOP.h>

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(*(a)))

static int level;

static void lprint(void)
{
	for (int i = 0; i < level; i++) {
		printf("\t");
	}
}

static int cb_html(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	printf("<html");
	if (LOP_handler_evalable(hl, 1)) {
		LOP_handler_eval(hl, 1, NULL);
	}
	if (LOP_handler_evalable(hl, 2)) {
		printf(">\n");
		level++;
		LOP_handler_eval(hl, 2, NULL);
		level--;
		printf("</html>\n");
	} else {
		printf(" />\n");
	}
	return 0;
}

static int cb_node(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	lprint();
	printf("<");
	LOP_handler_eval(hl, 0, NULL);
	if (LOP_handler_evalable(hl, 1)) {
		LOP_handler_eval(hl, 1, NULL);
	}
	if (LOP_handler_evalable(hl, 2)) {
		printf(">\n");
		level++;
		LOP_handler_eval(hl, 2, NULL);
		level--;
		lprint();
		printf("</");
		LOP_handler_eval(hl, 0, NULL);
		printf(">\n");
	} else {
		printf(" />\n");
	}
	return 0;
}

static int cb_print(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	printf("%s", LOP_symbol_value(n));
	return 0;
}

static int cb_attr_assign(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	LOP_handler_eval(hl, 1, NULL);
	printf("=\"");
	LOP_handler_eval(hl, 2, NULL);
	printf("\"");
	return 0;
}

static int cb_attr_separator(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	printf(" ");
	LOP_handler_eval(hl, 0, NULL);
	return 0;
}

static int cb_iprintln(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
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

static int resolve(struct LOP *lop, const char *key, struct LOP_CB *cb)
{
	static struct {
		const char *key;
		LOP_handler_t handler;
	} entries[] = {
		{ "html", cb_html },
		{ "node", cb_node },
		{ "attr_separator", cb_attr_separator },
		{ "attr_assign", cb_attr_assign },
		{ "iprintln", cb_iprintln },
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

	if (!LOP_schema_init(&lop, schema.data)) {
		LOP_schema_parse_source(NULL, &lop, source.data, argv[3]);
	}
	LOP_schema_deinit(&lop);

	unmap_file(schema);
	unmap_file(source);
	return 0;
}
