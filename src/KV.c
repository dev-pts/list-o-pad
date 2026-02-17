struct KVEntry {
	char *key;
	void *value;
};

struct KV {
	int count;
	struct KVEntry *children;
};

typedef void (*free_value_t)(void *arg);
typedef int (*kv_iterate_t)(void *arg, struct KVEntry *kv);

static struct KV *kv_alloc(void)
{
	struct KV *kv = calloc(1, sizeof(*kv));
	assert(kv);
	return kv;
}

static void kv_free(struct KV *kv, free_value_t free_value)
{
	for (int i = 0; i < kv->count; i++) {
		free(kv->children[i].key);
		if (free_value) {
			free_value(kv->children[i].value);
		}
	}
	free(kv->children);
	free(kv);
}

static int kv_add(struct KV *kv, const char *key, void *value)
{
	char *nkey;

	for (int i = 0; i < kv->count; i++) {
		if (!strcmp(kv->children[i].key, key)) {
			assert(kv->children[i].value == NULL);
			assert(value);
			kv->children[i].value = value;
			return kv->count;
		}
	}

	kv->count++;
	kv->children = realloc(kv->children, kv->count * sizeof(*kv->children));
	assert(kv->children);

	nkey = strdup(key);
	assert(nkey);

	kv->children[kv->count - 1] = (struct KVEntry) { nkey, value };

	return kv->count;
}

static int kv_get_index(struct KV *kv, const char *key, bool alloc)
{
	for (int i = 0; i < kv->count; i++) {
		if (!strcmp(kv->children[i].key, key)) {
			return i;
		}
	}

	if (alloc) {
		return kv_add(kv, key, NULL) - 1;
	}

	return -1;
}

static void kv_iterate(struct KV *kv, kv_iterate_t it, void *arg)
{
	for (int i = 0; i < kv->count; i++) {
		if (it(arg, &kv->children[i])) {
			break;
		}
	}
}
