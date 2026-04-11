#define KV_ADD(name, sn_ctor) \
do { \
	struct SchemaNode *p = NULL; \
	sn_ctor; \
	assert(p); \
	kv_add(kv, name, p); \
} while (0)

#define SN_NEW(...) \
do { \
	struct SchemaNode *c = sn_create(); \
	if (p) { \
		sn_append(p, c); \
	} else { \
		p = c; \
	} \
	{ \
		struct SchemaNode *p = c; \
		(void)p; \
		__VA_ARGS__ \
	} \
} while (0)

#define SN_ONEOF(...) \
SN_NEW( \
	sn_set_oneof(c); \
	__VA_ARGS__ \
)

#define SN_LISTOF(...) \
SN_NEW( \
	sn_set_listof(c); \
	__VA_ARGS__ \
)

#define SN_SEQOF(...) \
SN_NEW( \
	sn_set_seqof(c); \
	__VA_ARGS__ \
)

#define SN_REF(name, ...) \
SN_NEW( \
	sn_set_ref(c, name, kv); \
	__VA_ARGS__ \
)

#define SN_IDENTIFIER(...) \
SN_NEW( \
	sn_set_symbol_type(c, LOP_TYPE_ID); \
	__VA_ARGS__ \
)

#define SN_NUMBER(...) \
SN_NEW( \
	sn_set_symbol_type(c, LOP_TYPE_NUMBER); \
	__VA_ARGS__ \
)

#define SN_OPERATOR(...) \
SN_NEW( \
	sn_set_symbol_type(c, LOP_TYPE_OPERATOR); \
	__VA_ARGS__ \
)

#define SN_STRING(...) \
SN_NEW( \
	sn_set_symbol_type(c, LOP_TYPE_STRING); \
	__VA_ARGS__ \
)

#define SN_TREE(...) \
SN_NEW( \
	sn_set_tree(c); \
	__VA_ARGS__ \
)

#define SN_AREF(...) \
SN_NEW( \
	sn_set_aref(c); \
	__VA_ARGS__ \
)

#define SN_STRUCT(...) \
SN_NEW( \
	sn_set_struct(c); \
	__VA_ARGS__ \
)

#define SN_TLIST(...) \
SN_NEW( \
	sn_set_tlist(c); \
	__VA_ARGS__ \
)

#define SN_ALIST(...) \
SN_NEW( \
	sn_set_alist(c); \
	__VA_ARGS__ \
)

#define SN_SLIST(...) \
SN_NEW( \
	sn_set_slist(c); \
	__VA_ARGS__ \
)

#define SN_BINARY(...) \
SN_NEW( \
	sn_set_binary(c); \
	__VA_ARGS__ \
)

#define SN_UNARY(...) \
SN_NEW( \
	sn_set_unary(c); \
	__VA_ARGS__ \
)

#define SN_CB(handler) \
do { \
	c->key = strdup(#handler); \
	kv_add(r->kv, #handler, handler); \
} while (0)

static struct SchemaNode *sn_create()
{
	struct SchemaNode *c;

	c = calloc(1, sizeof(*c));
	assert(c);
	return c;
}

static void sn_append(struct SchemaNode *p, struct SchemaNode *n)
{
	if (p->child_count) {
		p->child[p->child_count - 1]->next = n;
	}

	p->child_count++;
	p->child = realloc(p->child, p->child_count * sizeof(*p->child));
	assert(p->child);

	p->child[p->child_count - 1] = n;

	n->parent = p;
}

static void sn_free(struct SchemaNode *sn)
{
	if (!sn) {
		return;
	}
	for (int i = 0; i < sn->child_count; i++) {
		sn_free(sn->child[i]);
	}
	free(sn->child);
	if (sn->sn_type == SN_TYPE_AST && sn->type > LOP_TYPE_LIST_LAST) {
		free(sn->symbol.value);
	}
	free((void *)sn->key);
	free(sn);
}

static void sn_set_symbol_type(struct SchemaNode *c, enum LOP_ASTNodeType type)
{
	c->sn_type = SN_TYPE_AST;
	c->type = type;
}

static void sn_set_list_type(struct SchemaNode *c, enum LOP_ASTNodeType type, int call)
{
	c->sn_type = SN_TYPE_AST;
	c->type = type;
	c->list.call = call;
}

static void sn_set_tree(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_COLON, 1);
}

static void sn_set_call(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_ROUND, 1);
}

static void sn_set_aref(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_SQUARE, 1);
}

static void sn_set_struct(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_CURLY, 1);
}

static void sn_set_fstring(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_STRING, 1);
}

static void sn_set_list(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_ROUND, 0);
}

static void sn_set_tlist(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_COLON, 0);
}

static void sn_set_alist(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_SQUARE, 0);
}

static void sn_set_slist(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_CURLY, 0);
}

static void sn_set_binary(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_OPERATOR_BINARY, 1);
}

static void sn_set_unary(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_OPERATOR_UNARY, 1);
}

static void sn_set_oneof(struct SchemaNode *c)
{
	c->sn_type = SN_TYPE_ONEOF;
}

static void sn_set_listof(struct SchemaNode *c)
{
	c->sn_type = SN_TYPE_LISTOF;
}

static void sn_set_seqof(struct SchemaNode *c)
{
	c->sn_type = SN_TYPE_SEQOF;
}

static void sn_set_ref(struct SchemaNode *c, const char *name, struct KV *kv)
{
	c->sn_type = SN_TYPE_REF;
	c->ref = kv_get_index(kv, name, true);
	assert(c->ref >= 0);
}

static void sn_set_symbol(struct SchemaNode *c, const char *value)
{
	c->symbol.value = strdup(value);
	assert(c->symbol.value);
}

static void sn_set_optional(struct SchemaNode *c)
{
	c->optional = true;
}

struct Runtime {
	struct LOP_Schema *schema;

	int unary_count;
	int binary_count;
	int prio;

	const char *key;
	struct SchemaNode *c;

	struct KV *kv;
};

static void ot_add(struct LOP_OperatorTable *table, const char *operator_string, int prio, unsigned type)
{
	struct LOP_Operator *op;

	table->size++;
	table->data = realloc(table->data, table->size * sizeof(*table->data));
	assert(table->data);

	op = &table->data[table->size - 1];

	op->value = strdup(operator_string);
	assert(op->value);
	op->prio = prio;
	op->type = type;
}

static void ot_destroy(struct LOP_OperatorTable *table)
{
	for (int i = 0; i < table->size; i++) {
		free((void *)table->data[i].value);
	}
	free(table->data);
}

static int cb_unary(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	struct LOP_Schema *schema = r->schema;

	ot_add(&schema->operator_table, LOP_symbol_value(n), r->prio, LOP_OPERATOR_UNARY);
	return 0;
}

static int cb_binary(struct Runtime *r, struct LOP_ASTNode *n, unsigned optype)
{
	struct LOP_Schema *schema = r->schema;

	ot_add(&schema->operator_table, LOP_symbol_value(n), r->prio, optype);
	return 0;
}

static int cb_binary_lr(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	cb_binary(r, n, LOP_OPERATOR_LTR);
	return 0;
}

static int cb_binary_rl(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	cb_binary(r, n, LOP_OPERATOR_RTL);
	return 0;
}

static int cb_dec_prio(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	if (delta == -1) {
		r->prio++;
	}
	return 0;
}

static int cb_kv_set_key(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	r->key = LOP_symbol_value(n);
	return 0;
}

static int cb_sn_create(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	if (delta == 1) {
		struct SchemaNode *c = sn_create();

		if (r->c) {
			sn_append(r->c, c);
		} else {
			kv_add(r->schema->kv, r->key, c);
		}

		r->c = c;
	} else if (delta == -1) {
		r->c = r->c->parent;
	}
	return 0;
}

static int cb_sn_set_symbol(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_symbol(r->c, LOP_symbol_value(n));
	return 0;
}

static int cb_sn_set_cb(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	r->c->key = strdup(LOP_symbol_value(n));
	return 0;
}

static int cb_sn_set_optional(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_optional(r->c);
	return 0;
}

static int cb_sn_set_oneof(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_oneof(r->c);
	return 0;
}

static int cb_sn_set_listof(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_listof(r->c);
	return 0;
}

static int cb_sn_set_seqof(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_seqof(r->c);
	return 0;
}

static int cb_sn_set_ref(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_ref(r->c, LOP_symbol_value(n), r->schema->kv);
	return 0;
}

static int cb_sn_set_identifier(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_symbol_type(r->c, LOP_TYPE_ID);
	return 0;
}

static int cb_sn_set_number(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_symbol_type(r->c, LOP_TYPE_NUMBER);
	return 0;
}

static int cb_sn_set_string(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_symbol_type(r->c, LOP_TYPE_STRING);
	return 0;
}

static int cb_sn_set_operator(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_symbol_type(r->c, LOP_TYPE_OPERATOR);
	return 0;
}

static int cb_sn_set_tree(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_tree(r->c);
	return 0;
}

static int cb_sn_set_call(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_call(r->c);
	return 0;
}

static int cb_sn_set_aref(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_aref(r->c);
	return 0;
}

static int cb_sn_set_struct(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_struct(r->c);
	return 0;
}

static int cb_sn_set_fstring(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_fstring(r->c);
	return 0;
}

static int cb_sn_set_list(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_list(r->c);
	return 0;
}

static int cb_sn_set_tlist(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_tlist(r->c);
	return 0;
}

static int cb_sn_set_alist(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_alist(r->c);
	return 0;
}

static int cb_sn_set_slist(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_slist(r->c);
	return 0;
}

static int cb_sn_set_binary(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_binary(r->c);
	return 0;
}

static int cb_sn_set_unary(struct Runtime *r, struct LOP_ASTNode *n, int delta)
{
	sn_set_unary(r->c);
	return 0;
}

static struct LOP_Schema root_schema_init(struct Runtime *r)
{
	struct LOP_Schema schema = {
		.kv = kv_alloc(),
	};
	struct KV *kv = schema.kv;

	ot_add(&schema.operator_table, "$", 0, LOP_OPERATOR_UNARY);
	ot_add(&schema.operator_table, "@", 0, LOP_OPERATOR_UNARY);
	ot_add(&schema.operator_table, "#", 0, LOP_OPERATOR_UNARY);

	KV_ADD("root",
		SN_TLIST(
			SN_ONEOF(
				sn_set_optional(c);
				SN_REF("optable");
			);
			SN_LISTOF(
				sn_set_optional(c);
				SN_REF("rule");
			);
		);
	);
	KV_ADD("optable",
		SN_TLIST(
			SN_UNARY(
				SN_OPERATOR(
					sn_set_symbol(c, "#");
				);
				SN_IDENTIFIER(
					sn_set_symbol(c, "operators");
				);
			);
			SN_SLIST(
				SN_CB(cb_dec_prio);
				SN_LISTOF(
					sn_set_optional(c);
					SN_REF("opdesc");
				);
			);
			SN_LISTOF(
				sn_set_optional(c);
				SN_REF("opdesc",
					SN_CB(cb_dec_prio);
				);
				SN_SLIST(
					SN_CB(cb_dec_prio);
					SN_LISTOF(
						sn_set_optional(c);
						SN_REF("opdesc");
					);
				);
			);
		);
	);
	KV_ADD("opdesc",
		SN_ONEOF(
			SN_TREE(
				SN_IDENTIFIER(
					sn_set_symbol(c, "unary");
				);
				SN_LISTOF(
					SN_STRING(
						SN_CB(cb_unary);
					);
				);
			);
			SN_TREE(
				SN_IDENTIFIER(
					sn_set_symbol(c, "binary_left_to_right");
				);
				SN_LISTOF(
					SN_STRING(
						SN_CB(cb_binary_lr);
					);
				);
			);
			SN_TREE(
				SN_IDENTIFIER(
					sn_set_symbol(c, "binary_right_to_left");
				);
				SN_LISTOF(
					SN_STRING(
						SN_CB(cb_binary_rl);
					);
				);
			);
		);
	);
	KV_ADD("rule",
		SN_TREE(
			SN_IDENTIFIER(
				SN_CB(cb_kv_set_key);
			);
			SN_REF("handler",
				sn_set_optional(c);
			);
			SN_REF("snode");
		);
	);
	KV_ADD("handler",
		SN_UNARY(
			SN_OPERATOR(
				sn_set_symbol(c, "@");
			);
			SN_IDENTIFIER(
				SN_CB(cb_sn_set_cb);
			);
		);
	);
	KV_ADD("option",
		SN_SEQOF(
			SN_UNARY(
				sn_set_optional(c);
				SN_CB(cb_sn_set_optional);
				SN_OPERATOR(
					sn_set_symbol(c, "#");
				);
				SN_IDENTIFIER(
					sn_set_symbol(c, "optional");
				);
			);
			SN_REF("handler",
				sn_set_optional(c);
			);
		);
	);
	KV_ADD("ref_one",
		SN_UNARY(
			SN_OPERATOR(
				sn_set_symbol(c, "$");
			);
			SN_IDENTIFIER(
				SN_CB(cb_sn_set_ref);
			);
		);
	);
	KV_ADD("snode",
		SN_ONEOF(
			SN_CB(cb_sn_create);
			SN_TREE(
				SN_CB(cb_sn_set_oneof);
				SN_IDENTIFIER(
					sn_set_symbol(c, "oneof");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_LISTOF(
					SN_REF("snode");
				);
			);
			SN_TREE(
				SN_CB(cb_sn_set_listof);
				SN_IDENTIFIER(
					sn_set_symbol(c, "listof");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_LISTOF(
					SN_REF("snode");
				);
			);
			SN_TREE(
				SN_CB(cb_sn_set_seqof);
				SN_IDENTIFIER(
					sn_set_symbol(c, "seqof");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_LISTOF(
					SN_REF("snode");
				);
			);
			SN_ONEOF(
				SN_REF("ref_one");
				SN_TREE(
					SN_REF("ref_one");
					SN_REF("option",
						sn_set_optional(c);
					);
				);
			);
			SN_ONEOF(
				SN_CB(cb_sn_set_identifier);
				SN_IDENTIFIER(
					sn_set_symbol(c, "identifier");
				);
				SN_TREE(
					SN_IDENTIFIER(
						sn_set_symbol(c, "identifier");
					);
					SN_REF("option",
						sn_set_optional(c);
					);
					SN_STRING(
						sn_set_optional(c);
						SN_CB(cb_sn_set_symbol);
					);
				);
			);
			SN_ONEOF(
				SN_CB(cb_sn_set_number);
				SN_IDENTIFIER(
					sn_set_symbol(c, "number");
				);
				SN_TREE(
					SN_IDENTIFIER(
						sn_set_symbol(c, "number");
					);
					SN_REF("option",
						sn_set_optional(c);
					);
					SN_STRING(
						sn_set_optional(c);
						SN_CB(cb_sn_set_symbol);
					);
				);
			);
			SN_ONEOF(
				SN_CB(cb_sn_set_string);
				SN_IDENTIFIER(
					sn_set_symbol(c, "string");
				);
				SN_TREE(
					SN_IDENTIFIER(
						sn_set_symbol(c, "string");
					);
					SN_REF("option",
						sn_set_optional(c);
					);
					SN_STRING(
						sn_set_optional(c);
						SN_CB(cb_sn_set_symbol);
					);
				);
			);
			SN_ONEOF(
				SN_CB(cb_sn_set_operator);
				SN_IDENTIFIER(
					sn_set_symbol(c, "operator");
				);
				SN_TREE(
					SN_IDENTIFIER(
						sn_set_symbol(c, "operator");
					);
					SN_REF("option",
						sn_set_optional(c);
					);
					SN_STRING(
						sn_set_optional(c);
						SN_CB(cb_sn_set_symbol);
					);
				);
			);
			SN_TREE(
				SN_CB(cb_sn_set_tree);
				SN_IDENTIFIER(
					sn_set_symbol(c, "tree");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_LISTOF(
					SN_REF("snode");
				);
			);
			SN_TREE(
				SN_CB(cb_sn_set_call);
				SN_IDENTIFIER(
					sn_set_symbol(c, "call");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_LISTOF(
					SN_REF("snode");
				);
			);
			SN_TREE(
				SN_CB(cb_sn_set_aref);
				SN_IDENTIFIER(
					sn_set_symbol(c, "aref");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_LISTOF(
					SN_REF("snode");
				);
			);
			SN_TREE(
				SN_CB(cb_sn_set_struct);
				SN_IDENTIFIER(
					sn_set_symbol(c, "struct");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_LISTOF(
					SN_REF("snode");
				);
			);
			SN_TREE(
				SN_CB(cb_sn_set_fstring);
				SN_IDENTIFIER(
					sn_set_symbol(c, "fstring");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_REF("snode");
				SN_REF("snode");
			);
			SN_ONEOF(
				SN_CB(cb_sn_set_list);
				SN_IDENTIFIER(
					sn_set_symbol(c, "list");
				);
				SN_TREE(
					SN_IDENTIFIER(
						sn_set_symbol(c, "list");
					);
					SN_REF("option",
						sn_set_optional(c);
					);
					SN_LISTOF(
						sn_set_optional(c);
						SN_REF("snode");
					);
				);
			);
			SN_ONEOF(
				SN_CB(cb_sn_set_tlist);
				SN_IDENTIFIER(
					sn_set_symbol(c, "tlist");
				);
				SN_TREE(
					SN_IDENTIFIER(
						sn_set_symbol(c, "tlist");
					);
					SN_REF("option",
						sn_set_optional(c);
					);
					SN_LISTOF(
						sn_set_optional(c);
						SN_REF("snode");
					);
				);
			);
			SN_ONEOF(
				SN_CB(cb_sn_set_alist);
				SN_IDENTIFIER(
					sn_set_symbol(c, "alist");
				);
				SN_TREE(
					SN_IDENTIFIER(
						sn_set_symbol(c, "alist");
					);
					SN_REF("option",
						sn_set_optional(c);
					);
					SN_LISTOF(
						sn_set_optional(c);
						SN_REF("snode");
					);
				);
			);
			SN_ONEOF(
				SN_CB(cb_sn_set_slist);
				SN_IDENTIFIER(
					sn_set_symbol(c, "slist");
				);
				SN_TREE(
					SN_IDENTIFIER(
						sn_set_symbol(c, "slist");
					);
					SN_REF("option",
						sn_set_optional(c);
					);
					SN_LISTOF(
						sn_set_optional(c);
						SN_REF("snode");
					);
				);
			);
			SN_TREE(
				SN_CB(cb_sn_set_binary);
				SN_IDENTIFIER(
					sn_set_symbol(c, "binary");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_REF("snode");
				SN_REF("snode");
				SN_REF("snode");
			);
			SN_TREE(
				SN_CB(cb_sn_set_unary);
				SN_IDENTIFIER(
					sn_set_symbol(c, "unary");
				);
				SN_REF("option",
					sn_set_optional(c);
				);
				SN_REF("snode");
				SN_REF("snode");
			);
		);
	);

	return schema;
}

static int kv_check(void *arg, struct KVEntry *kv)
{
	if (kv->value == NULL) {
		*(void **)arg = kv->key;
		return 1;
	}
	return 0;
}

static void call_handlers(struct Runtime *r, struct LOP *lop)
{
	struct LOP_HandlerList *hl = &lop->hl;
	int delta = 0;

	for (int i = 0; i < hl->count; i++) {
		struct LOP_Handler *h = &hl->handler[i];
		int (*cb)(struct Runtime *r, struct LOP_ASTNode *n, int delta);

		if (h->delta == -1) {
			delta--;
		}
		assert(delta >= 0);
		if (h->delta == 1) {
			delta++;
		}

		cb = kv_get(r->kv, h->key);
		cb(r, h->n, h->delta);
	}
}

int LOP_schema_init(struct LOP_Schema *schema, const char *src, size_t len)
{
	struct LOP_Schema root_schema = {};
	struct Runtime r = {
		.schema = schema,
		.kv = kv_alloc(),
	};
	int rc = 0;

	/* We will fill the structure */
	assert(schema->operator_table.size == 0);
	assert(schema->operator_table.data == NULL);
	assert(schema->kv == NULL);

	/* Prepare root schema to parse user schema */
	root_schema = root_schema_init(&r);

	/* Alocate KV storage where rules from user schema will be stored */
	schema->kv = kv_alloc();

	struct LOP lop = {
		.schema = &root_schema,
		.top_rule_name = "root",
		.filename = schema->filename,
	};

	/* Parse user schema and fill schema with rules and operators table */
	rc = LOP_init(&lop, src, len);
	if (rc == 0) {
		const char *err_key = NULL;

		call_handlers(&r, &lop);

		/* Ensure that all references to the rules refer to the existed rules in the user schema */
		kv_iterate(schema->kv, kv_check, &err_key);
		if (err_key) {
			rc = s_report(LOP_ERROR_SCHEMA_MISSING_RULE, err_key);
		}
	}

	LOP_deinit(&lop);
	kv_free(r.kv, NULL);

	/* Root schema is on the stack, we must free its internal allocs */
	LOP_schema_deinit(&root_schema);
	return rc;
}

void LOP_schema_deinit(struct LOP_Schema *schema)
{
	kv_free(schema->kv, (free_value_t)sn_free);

	ot_destroy(&schema->operator_table);

	schema->kv = NULL;
	schema->operator_table.size = 0;
	schema->operator_table.data = NULL;
}
