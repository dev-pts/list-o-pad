#include <LOP.h>
#include <string.h>
#include "FileMap.h"

static int cb_dummy(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	static int level;

	for (int i = 0; i < level; i++) {
		printf("\t");
	}

	if (n->type > LOP_TYPE_LIST_LAST) {
		printf("%s %s\n", (char *)cb_arg, LOP_symbol_value(n));
	} else {
		printf("%s\n", (char *)cb_arg);
	}
	level++;
	LOP_cb_default(hl, n, param, NULL);
	level--;
	return 0;
}

static int resolve(struct LOP *lop, const char *key, struct LOP_CB *cb)
{
	cb->func = cb_dummy;
	cb->arg = strdup(key);
	return 0;
}

int main(int argc, char *argv[])
{
	struct LOP lop = {
		.resolve = resolve,
	};
	struct FileMap schema = map_file(argv[1]);
	struct FileMap source = map_file(argv[2]);
	int rc;

	rc = LOP_schema_init(&lop, argv[1], schema.data, schema.len);
	if (rc == 0) {
		LOP_schema_parse_source(NULL, &lop, argv[2], source.data, source.len, argv[3]);
	} else {
		fprintf(stderr, "Schema parsing error\n");
	}
	LOP_schema_deinit(&lop);

	unmap_file(schema);
	unmap_file(source);
	return 0;
}
