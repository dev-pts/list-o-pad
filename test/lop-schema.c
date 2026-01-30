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
