#include <LOP.h>
#include "FileMap.h"

int main(int argc, char *argv[])
{
	struct LOP lop = {
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
