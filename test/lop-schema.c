#include <assert.h>
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
	int rc;

	if (argc < 4) {
		fprintf(stderr, "Usage: %s <schema-file> <top-rule-name> <source-file> ...\n", argv[0]);
		return -1;
	}

	struct FileMap schema = map_file(argv[1]);
	assert(schema.fd >= 0);
	rc = LOP_schema_init(&lop, argv[1], schema.data, schema.len);
	unmap_file(schema);

	if (rc < 0) {
		fprintf(stderr, "User schema parsing error\n");
		goto out;
	}

	for (int i = 3; i < argc; i++) {
		struct FileMap source = map_file(argv[i]);
		assert(source.fd >= 0);
		rc = LOP_schema_parse_source(NULL, &lop, argv[i], source.data, source.len, argv[2]);
		unmap_file(source);

		if (rc < 0) {
			fprintf(stderr, "Parsing error\n");
			break;
		}
	}

out:
	LOP_schema_deinit(&lop);
	return 0;
}
