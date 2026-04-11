#include <assert.h>
#include <LOP.h>
#include <string.h>
#include "FileMap.h"

static int cb_dummy(struct LOP_Handler *h)
{
	static int delta;

	if (h->delta != -1) {
		for (int i = 0; i < delta; i++) {
			printf("\t");
		}
	}

	if (h->delta == 1) {
		printf("%s\n", h->key);
	} else if (h->delta == 0) {
		if (h->n->type > LOP_TYPE_LIST_LAST) {
			printf("%s %s\n", h->key, LOP_symbol_value(h->n));
		}
	}

	delta += h->delta;
	return 0;
}

int main(int argc, char *argv[])
{
	struct LOP_Schema schema = {
		.filename = argv[1],
	};
	int rc;

	if (argc < 4) {
		fprintf(stderr, "Usage: %s <schema-file> <top-rule-name> <source-file> ...\n", argv[0]);
		return -1;
	}

	struct FileMap schema_str = map_file(argv[1]);
	assert(schema_str.fd >= 0);
	rc = LOP_schema_init(&schema, schema_str.data, schema_str.len);
	unmap_file(schema_str);

	if (rc < 0) {
		fprintf(stderr, "User schema parsing error\n");
		goto out;
	}

	for (int i = 3; i < argc; i++) {
		struct LOP lop = {
			.schema = &schema,
			.top_rule_name = argv[2],
			.filename = argv[i],
		};
		struct FileMap source = map_file(argv[i]);

		assert(source.fd >= 0);

		rc = LOP_init(&lop, source.data, source.len);
		unmap_file(source);

		struct LOP_HandlerList *hl = &lop.hl;

		for (int i = 0; i < hl->count; i++) {
			struct LOP_Handler *h = &hl->handler[i];

			cb_dummy(h);
		}

		LOP_deinit(&lop);

		if (rc < 0) {
			fprintf(stderr, "Parsing error\n");
			break;
		}
	}

out:
	LOP_schema_deinit(&schema);
	return 0;
}
