#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "FileMap.h"
#include <LOP.h>

struct List {
	struct Item {
		int id;
		int value;
		bool selected;
		char *text;
	} **item;
	int count;
};

static int cb_item(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct List *list = param;
	struct Item *item = calloc(1, sizeof(*item));

	assert(item);

	list->count++;
	list->item = realloc(list->item, list->count * sizeof(*list->item));
	assert(list->item);

	list->item[list->count - 1] = item;

	return LOP_cb_default(hl, n, item, NULL);
}

static int cb_id(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Item *item = param;

	item->id = atoi(LOP_symbol_value(n));
	return 0;
}

static int cb_value(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Item *item = param;

	item->value = atoi(LOP_symbol_value(n));
	return 0;
}

static int cb_selected(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Item *item = param;

	item->selected = true;
	return 0;
}

static int cb_text(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Item *item = param;

	item->text = strdup(LOP_symbol_value(n));
	assert(item->text);
	return 0;
}

static int resolve(struct LOP *lop, const char *key, struct LOP_CB *cb)
{
	static struct {
		const char *key;
		LOP_handler_t handler;
	} entries[] = {
		{ "item", cb_item },
		{ "id", cb_id },
		{ "value", cb_value },
		{ "selected", cb_selected },
		{ "text", cb_text },
		{ "", NULL },
	};

	for (int i = 0; entries[i].handler; i++) {
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
	struct List list = {};
	struct FileMap schema = map_file(argv[1]);
	struct FileMap source = map_file(argv[2]);

	if (!LOP_schema_init(&lop, schema.data)) {
		LOP_schema_parse_source(&list, &lop, source.data, argv[3]);
	}
	LOP_schema_deinit(&lop);

	unmap_file(schema);
	unmap_file(source);

	for (int i = 0; i < list.count; i++) {
		struct Item *item = list.item[i];

		printf("id: %i, value: %i, selected: %i, text: %s\n", item->id, item->value, item->selected, item->text);

		free(item->text);
		free(item);
	}
	free(list.item);

	return 0;
}
