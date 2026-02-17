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
	c->cb.func = handler; \
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
	p->child_count++;
	p->child = realloc(p->child, p->child_count * sizeof(*p->child));
	assert(p->child);

	p->child[p->child_count - 1] = n;
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
	free(sn);
}

static void sn_set_symbol_type(struct SchemaNode *c, enum LOP_ASTNodeType type)
{
	c->sn_type = SN_TYPE_AST;
	c->type = type;
	c->cb.func = LOP_cb_default;
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
	c->cb.func = LOP_cb_default;
}

static void sn_set_call(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_ROUND, 1);
	c->cb.func = LOP_cb_default;
}

static void sn_set_aref(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_SQUARE, 1);
	c->cb.func = LOP_cb_default;
}

static void sn_set_struct(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_CURLY, 1);
	c->cb.func = LOP_cb_default;
}

static void sn_set_fstring(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_STRING, 1);
	c->cb.func = LOP_cb_default;
}

static void sn_set_list(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_ROUND, 0);
	c->cb.func = LOP_cb_default;
}

static void sn_set_tlist(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_COLON, 0);
	c->cb.func = LOP_cb_default;
}

static void sn_set_alist(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_SQUARE, 0);
	c->cb.func = LOP_cb_default;
}

static void sn_set_slist(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_CURLY, 0);
	c->cb.func = LOP_cb_default;
}

static void sn_set_binary(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_OPERATOR_BINARY, 1);
	c->cb.func = LOP_cb_default;
}

static void sn_set_unary(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_TYPE_LIST_OPERATOR_UNARY, 1);
	c->cb.func = LOP_cb_default;
}

static void sn_set_oneof(struct SchemaNode *c)
{
	c->sn_type = SN_TYPE_ONEOF;
	c->cb.func = LOP_cb_default;
}

static void sn_set_listof(struct SchemaNode *c)
{
	c->sn_type = SN_TYPE_LISTOF;
	c->cb.func = LOP_cb_default;
}

static void sn_set_seqof(struct SchemaNode *c)
{
	c->sn_type = SN_TYPE_SEQOF;
	c->cb.func = LOP_cb_default;
}

static void sn_set_ref(struct SchemaNode *c, const char *name, struct KV *kv)
{
	c->sn_type = SN_TYPE_REF;
	c->ref = kv_get_index(kv, name, true);
	assert(c->ref >= 0);
	c->cb.func = LOP_cb_default;
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

static void sn_set_last(struct SchemaNode *c)
{
	c->last = true;
}

struct Runtime {
	struct LOP *lop;

	int unary_count;
	int binary_count;
	int prio;

	const char *key;
	struct SchemaNode *c;
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

static int cb_unary(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;
	struct LOP *lop = r->lop;

	ot_add(&lop->operator_table, LOP_symbol_value(n), r->prio, LOP_OPERATOR_UNARY);
	return 0;
}

static int cb_binary(struct Runtime *r, struct LOP_ASTNode *n, unsigned optype)
{
	struct LOP *lop = r->lop;

	ot_add(&lop->operator_table, LOP_symbol_value(n), r->prio, optype);
	return 0;
}

static int cb_binary_lr(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	cb_binary(param, n, LOP_OPERATOR_LTR);
	return 0;
}

static int cb_binary_rl(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	cb_binary(param, n, LOP_OPERATOR_RTL);
	return 0;
}

static int cb_dec_prio(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;
	int rc = LOP_cb_default(hl, NULL, param, NULL);

	if (rc < 0) {
		return rc;
	}

	r->prio++;
	return 0;
}

static int cb_kv_set_key(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	r->key = LOP_symbol_value(n);
	return 0;
}

static int cb_sn_create(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;
	struct SchemaNode *p = r->c;
	struct SchemaNode *c = sn_create();
	int rc;

	if (p) {
		sn_append(p, c);
	} else {
		kv_add(r->lop->kv, r->key, c);
	}

	r->c = c;
	rc = LOP_cb_default(hl, NULL, param, NULL);
	if (rc < 0) {
		return rc;
	}
	r->c = p;
	return 0;
}

static int cb_sn_set_symbol(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_symbol(r->c, LOP_symbol_value(n));
	return 0;
}

static int cb_sn_set_cb(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;
	const char *key = LOP_symbol_value(n);

	if (!r->lop->resolve || r->lop->resolve(r->lop, key, &r->c->cb) < 0 || !r->c->cb.func) {
		return s_report(LOP_ERROR_SCHEMA_MISSING_HANDLER, key);
	}
	return 0;
}

static int cb_sn_set_optional(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_optional(r->c);
	return 0;
}

static int cb_sn_set_last(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_last(r->c);
	return 0;
}

static int cb_sn_set_oneof(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_oneof(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_listof(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_listof(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_seqof(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_seqof(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_ref(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_ref(r->c, LOP_symbol_value(n), r->lop->kv);
	return 0;
}

static int cb_sn_set_identifier(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_symbol_type(r->c, LOP_TYPE_ID);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_number(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_symbol_type(r->c, LOP_TYPE_NUMBER);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_string(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_symbol_type(r->c, LOP_TYPE_STRING);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_operator(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_symbol_type(r->c, LOP_TYPE_OPERATOR);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_tree(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_tree(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_call(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_call(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_aref(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_aref(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_struct(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_struct(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_fstring(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_fstring(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_list(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_list(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_tlist(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_tlist(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_alist(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_alist(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_slist(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_slist(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_binary(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_binary(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_unary(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_unary(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static struct LOP root_schema_init(void)
{
	struct LOP lop = {
		.kv = kv_alloc(),
	};
	struct KV *kv = lop.kv;

	ot_add(&lop.operator_table, "$", 0, LOP_OPERATOR_UNARY);
	ot_add(&lop.operator_table, "@", 0, LOP_OPERATOR_UNARY);
	ot_add(&lop.operator_table, "#", 0, LOP_OPERATOR_UNARY);

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
			SN_UNARY(
				sn_set_optional(c);
				SN_CB(cb_sn_set_last);
				SN_OPERATOR(
					sn_set_symbol(c, "#");
				);
				SN_IDENTIFIER(
					sn_set_symbol(c, "last");
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

	return lop;
}

static int kv_check(void *arg, struct KVEntry *kv)
{
	if (kv->value == NULL) {
		*(void **)arg = kv->key;
		return 1;
	}
	return 0;
}

int LOP_schema_init(struct LOP *lop, const char *filename, const char *user_schema, size_t len)
{
	struct LOP root_schema;
	struct Runtime r = {
		.lop = lop,
	};
	int rc = 0;

	/* We will fill the structure */
	assert(lop->operator_table.size == 0);
	assert(lop->operator_table.data == NULL);
	assert(lop->kv == NULL);

	/* Prepare root schema to parse user schema */
	root_schema = root_schema_init();

	/* Alocate KV storage where rules from user schema will be stored */
	lop->kv = kv_alloc();

	/* Parse user schema and fill lop with rules and operators table */
	rc = LOP_schema_parse_source(&r, &root_schema, filename, user_schema, len, "root");
	if (rc == 0) {
		const char *err_key = NULL;

		/* Ensure that all references to the rules refer to the existed rules in the user schema */
		kv_iterate(lop->kv, kv_check, &err_key);
		if (err_key) {
			rc = s_report(LOP_ERROR_SCHEMA_MISSING_RULE, err_key);
		}
	}

	/* Root schema is on the stack, we must free its internal allocs */
	LOP_schema_deinit(&root_schema);
	return rc;
}

void LOP_schema_deinit(struct LOP *lop)
{
	kv_free(lop->kv, (free_value_t)sn_free);

	ot_destroy(&lop->operator_table);
}
