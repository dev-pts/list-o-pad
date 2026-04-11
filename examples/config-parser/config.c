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

static int cb_item(struct List *list, struct LOP_ASTNode *n, int delta)
{
	if (delta < 0) {
		return 0;
	}

	struct Item *item = calloc(1, sizeof(*item));

	assert(item);

	list->count++;
	list->item = realloc(list->item, list->count * sizeof(*list->item));
	assert(list->item);

	list->item[list->count - 1] = item;

	return 0;
}

static int cb_id(struct List *list, struct LOP_ASTNode *n, int delta)
{
	struct Item *item = list->item[list->count - 1];

	item->id = atoi(LOP_symbol_value(n));
	return 0;
}

static int cb_value(struct List *list, struct LOP_ASTNode *n, int delta)
{
	struct Item *item = list->item[list->count - 1];

	item->value = atoi(LOP_symbol_value(n));
	return 0;
}

static int cb_selected(struct List *list, struct LOP_ASTNode *n, int delta)
{
	struct Item *item = list->item[list->count - 1];

	item->selected = true;
	return 0;
}

static int cb_text(struct List *list, struct LOP_ASTNode *n, int delta)
{
	struct Item *item = list->item[list->count - 1];

	item->text = strdup(LOP_symbol_value(n));
	assert(item->text);
	return 0;
}

static int resolve(struct List *list, struct LOP_Handler *h)
{
	typedef int (*cb_t)(struct List *list, struct LOP_ASTNode *n, int delta);
	static struct {
		const char *key;
		cb_t handler;
	} entries[] = {
		{ "item", cb_item },
		{ "id", cb_id },
		{ "value", cb_value },
		{ "selected", cb_selected },
		{ "text", cb_text },
		{ "", NULL },
	};

	for (int i = 0; entries[i].handler; i++) {
		if (!strcmp(h->key, entries[i].key)) {
			return entries[i].handler(list, h->n, h->delta);
		}
	}
	return -1;
}

int main(int argc, char *argv[])
{
	struct LOP_Schema schema = {
		.filename = argv[1],
	};
	struct List list = {};
	struct FileMap schema_str = map_file(argv[1]);
	struct FileMap source = map_file(argv[2]);

	if (!LOP_schema_init(&schema, schema_str.data, schema_str.len)) {
		struct LOP lop = {
			.schema = &schema,
			.top_rule_name = argv[3],
			.filename = argv[2],
		};

		LOP_init(&lop, source.data, source.len);

		struct LOP_HandlerList *hl = &lop.hl;

		for (int i = 0; i < hl->count; i++) {
			struct LOP_Handler *h = &hl->handler[i];

			if (resolve(&list, h)) {
				break;
			}
		}

		LOP_deinit(&lop);
	}
	LOP_schema_deinit(&schema);

	unmap_file(schema_str);
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
