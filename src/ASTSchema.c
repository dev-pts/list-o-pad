#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LOP.h>

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
		sn_set_identifier(c); \
		__VA_ARGS__ \
	)

#define SN_NUMBER(...) \
	SN_NEW( \
		sn_set_number(c); \
		__VA_ARGS__ \
	)

#define SN_OPERATOR(...) \
	SN_NEW( \
		sn_set_operator(c); \
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

#define SN_STRING(...) \
	SN_NEW( \
		sn_set_string(c); \
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

typedef void (*free_value_t)(void *arg);

struct KV {
	int count;
	struct KVEntry {
		char *key;
		void *value;
	} *children;
};

struct SchemaNode {
	enum SchemaNodeType {
		SN_TYPE_AST,
		SN_TYPE_ONEOF,
		SN_TYPE_LISTOF,
		SN_TYPE_SEQOF,
		SN_TYPE_REF,
	} sn_type;

	bool optional;
	bool last;

	struct LOP_CB cb;

	struct SchemaNode **child;
	int child_count;

	int ref;

	enum LOP_ASTNodeType type;

	union {
		struct {
			enum LOP_SymbolType type;
			char *value;
		} symbol;

		struct {
			enum LOP_ListType type;
			enum LOP_ListOp op;
		} list;
	};
};

static void dump_sn(struct SchemaNode *sn)
{
#define CASE_SN_TYPE(type)	case type: printf(#type); break
	switch (sn->sn_type) {
		CASE_SN_TYPE(SN_TYPE_AST);
		CASE_SN_TYPE(SN_TYPE_ONEOF);
		CASE_SN_TYPE(SN_TYPE_LISTOF);
		CASE_SN_TYPE(SN_TYPE_SEQOF);
		CASE_SN_TYPE(SN_TYPE_REF);
	}
#undef CASE_SN_TYPE
	if (sn->optional) {
		printf(", optional");
	}
	if (sn->last) {
		printf(", last");
	}
	printf(", child_count = %i", sn->child_count);
	if (sn->sn_type == SN_TYPE_AST) {
		if (sn->type == LOP_AST_LIST) {
#define CASE_LIST_TYPE(type, val)	case type: printf(", " val); break
			switch (sn->list.type) {
				CASE_LIST_TYPE(LOP_LIST_TLIST, "tlist");
				CASE_LIST_TYPE(LOP_LIST_LIST, "list");
				CASE_LIST_TYPE(LOP_LIST_AREF, "aref");
				CASE_LIST_TYPE(LOP_LIST_STRUCT, "struct");
				CASE_LIST_TYPE(LOP_LIST_OPERATOR, "operator");
				CASE_LIST_TYPE(LOP_LIST_STRING, "string");
			}
#undef CASE_LIST_TYPE
#define CASE_LIST_OP(op, val)	case op: printf(", " val); break
			switch (sn->list.op) {
				CASE_LIST_OP(LOP_LIST_NOP, "nop");
				CASE_LIST_OP(LOP_LIST_CALL, "call");
				CASE_LIST_OP(LOP_LIST_BINARY, "binary");
				CASE_LIST_OP(LOP_LIST_UNARY, "unary");
			}
#undef CASE_LIST_OP
		} else {
#define CASE_SYMBOL_TYPE(type, val)	case type: printf(", " val); break
			switch (sn->symbol.type) {
				CASE_SYMBOL_TYPE(LOP_SYMBOL_IDENTIFIER, "identifier");
				CASE_SYMBOL_TYPE(LOP_SYMBOL_NUMBER, "number");
				CASE_SYMBOL_TYPE(LOP_SYMBOL_STRING, "string");
				CASE_SYMBOL_TYPE(LOP_SYMBOL_OPERATOR, "operator");
			}
#undef CASE_SYMBOL_TYPE
			printf(", \"%s\"", sn->symbol.value);
		}
	}
	printf("\n");
}

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
	if (sn->sn_type == SN_TYPE_AST && sn->type == LOP_AST_SYMBOL) {
		free(sn->symbol.value);
	}
	if (sn->cb.dtor) {
		sn->cb.dtor(&sn->cb);
	}
	free(sn);
}

int LOP_handler_eval(struct LOP_HandlerList hl, unsigned child, void *param)
{
	struct LOP_Handler h = hl.handler[child];

	return h.sn->cb.func(h.hl, h.n, param, h.sn->cb.arg);
}

bool LOP_handler_evalable(struct LOP_HandlerList hl, unsigned child)
{
	return child < hl.count && hl.handler[child].sn;
}

static struct LOP_HandlerList *handler_child(struct LOP_HandlerList *parent, int i)
{
	return &parent->handler[i].hl;
}

static struct LOP_HandlerList *handler_tail(struct LOP_HandlerList *parent)
{
	return handler_child(parent, parent->count - 1);
}

static void handler_free(struct LOP_HandlerList *hl)
{
	for (int i = 0; i < hl->count; i++) {
		handler_free(handler_child(hl, i));
	}
	free(hl->handler);
}

static void handler_resize(struct LOP_HandlerList *c, int count)
{
	for (int i = count; i < c->count; i++) {
		handler_free(handler_child(c, i));
	}
	c->count = count;
	c->handler = realloc(c->handler, c->count * sizeof(*c->handler));
	if (c->count) {
		assert(c->handler);
	}
}

static void handler_add(struct LOP_HandlerList *c, struct SchemaNode *sn, struct LOP_ASTNode *n)
{
	handler_resize(c, c->count + 1);

	c->handler[c->count - 1] = (struct LOP_Handler) { sn, n };
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

static int kv_get(struct KV *kv, const char *key, bool alloc)
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

static bool check_entry(struct LOP_HandlerList *hl, struct LOP_ASTNode **n, struct SchemaNode *c, struct LOP_ASTNode **err, struct KV *kv);

static bool check_oneof(struct LOP_HandlerList *hl, struct LOP_ASTNode **n, struct SchemaNode *c, struct LOP_ASTNode **err, struct KV *kv)
{
	for (int i = 0; i < c->child_count; i++) {
		if (check_entry(hl, n, c->child[i], err, kv)) {
			return true;
		}
	}
	return false;
}

static bool check_listof(struct LOP_HandlerList *hl, struct LOP_ASTNode **n, struct SchemaNode *c, struct LOP_ASTNode **err, struct KV *kv)
{
	bool found = false;

	while (check_oneof(hl, n, c, err, kv)) {
		found = true;
	}
	return found;
}

static bool check_seqof(struct LOP_HandlerList *hl, struct LOP_ASTNode **n, struct SchemaNode *c, struct LOP_ASTNode **err, struct KV *kv)
{
	int i = 0;

	for (i = 0; i < c->child_count; i++) {
		struct SchemaNode *ci = c->child[i];

		if (check_entry(hl, n, ci, err, kv)) {
			if (!*n) {
				i++;
				break;
			}
		} else if (ci->optional) {
			handler_add(hl, NULL, NULL);
		} else {
			return false;
		}
	}

	if (!*n) {
		for (; i < c->child_count; i++) {
			struct SchemaNode *ci = c->child[i];

			if (!ci->optional) {
				return false;
			}
		}
	}

	return true;
}

static bool check_entry(struct LOP_HandlerList *hl, struct LOP_ASTNode **n, struct SchemaNode *c, struct LOP_ASTNode **err, struct KV *kv)
{
	struct LOP_ASTNode *nn = *n;
	int hl_count = hl->count;

	if (!nn) {
		goto mismatch;
	}

	handler_add(hl, c, nn);

	switch (c->sn_type) {
	case SN_TYPE_ONEOF:
		if (!check_oneof(handler_tail(hl), n, c, err, kv)) {
			goto mismatch;
		}
		break;
	case SN_TYPE_LISTOF:
		if (!check_listof(handler_tail(hl), n, c, err, kv)) {
			goto mismatch;
		}
		break;
	case SN_TYPE_SEQOF:
		if (!check_seqof(handler_tail(hl), n, c, err, kv)) {
			goto mismatch;
		}
		break;
	case SN_TYPE_REF:
		if (!check_entry(handler_tail(hl), n, kv->children[c->ref].value, err, kv)) {
			goto mismatch;
		}
		break;
	case SN_TYPE_AST:
		if (c->type != nn->type) {
			goto mismatch;
		}

		if (c->type == LOP_AST_LIST) {
			struct LOP_ASTNode *h = LOP_list_head(nn);

			if (c->list.type != nn->list.type) {
				goto mismatch;
			}
			if (c->list.op != nn->list.op) {
				goto mismatch;
			}
			if (!check_seqof(handler_tail(hl), &h, c, err, kv)) {
				goto mismatch;
			}
			if (h) {
				goto mismatch;
			}
		} else {
			if (c->symbol.type != nn->symbol.type) {
				goto mismatch;
			}
			if (c->symbol.value != NULL) {
				if (strcmp(c->symbol.value, nn->symbol.value)) {
					goto mismatch;
				}
			}
		}

		*n = nn->next;
		if (*n) {
			*err = *n;
		}
		break;
	default:
		assert(0);
	}

	if (c->last && *n) {
		goto mismatch;
	}

	return true;

mismatch:
	handler_resize(hl, hl_count);
	return false;
}

int LOP_cb_default(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	for (int i = 0; i < hl.count; i++) {
		if (LOP_handler_evalable(hl, i)) {
			int rc = LOP_handler_eval(hl, i, param);
			if (rc < 0) {
				return rc;
			}
		}
	}
	return 0;
}

static void sn_set_symbol_type(struct SchemaNode *c, enum LOP_SymbolType symbol_type)
{
	c->sn_type = SN_TYPE_AST;
	c->type = LOP_AST_SYMBOL;
	c->symbol.type = symbol_type;
	c->cb.func = LOP_cb_default;
}

static void sn_set_identifier(struct SchemaNode *c)
{
	sn_set_symbol_type(c, LOP_SYMBOL_IDENTIFIER);
}

static void sn_set_number(struct SchemaNode *c)
{
	sn_set_symbol_type(c, LOP_SYMBOL_NUMBER);
}

static void sn_set_string(struct SchemaNode *c)
{
	sn_set_symbol_type(c, LOP_SYMBOL_STRING);
}

static void sn_set_operator(struct SchemaNode *c)
{
	sn_set_symbol_type(c, LOP_SYMBOL_OPERATOR);
}

static void sn_set_list_type(struct SchemaNode *c, enum LOP_ListType list_type, enum LOP_ListOp list_op)
{
	c->sn_type = SN_TYPE_AST;
	c->type = LOP_AST_LIST;
	c->list.type = list_type;
	c->list.op = list_op;
}

static void sn_set_tree(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_TLIST, LOP_LIST_CALL);
	c->cb.func = LOP_cb_default;
}

static void sn_set_call(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_LIST, LOP_LIST_CALL);
	c->cb.func = LOP_cb_default;
}

static void sn_set_aref(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_AREF, LOP_LIST_CALL);
	c->cb.func = LOP_cb_default;
}

static void sn_set_struct(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_STRUCT, LOP_LIST_CALL);
	c->cb.func = LOP_cb_default;
}

static void sn_set_fstring(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_STRING, LOP_LIST_CALL);
	c->cb.func = LOP_cb_default;
}

static void sn_set_list(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_LIST, LOP_LIST_NOP);
	c->cb.func = LOP_cb_default;
}

static void sn_set_tlist(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_TLIST, LOP_LIST_NOP);
	c->cb.func = LOP_cb_default;
}

static void sn_set_alist(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_AREF, LOP_LIST_NOP);
	c->cb.func = LOP_cb_default;
}

static void sn_set_slist(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_STRUCT, LOP_LIST_NOP);
	c->cb.func = LOP_cb_default;
}

static void sn_set_binary(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_OPERATOR, LOP_LIST_BINARY);
	c->cb.func = LOP_cb_default;
}

static void sn_set_unary(struct SchemaNode *c)
{
	sn_set_list_type(c, LOP_LIST_OPERATOR, LOP_LIST_UNARY);
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
	c->ref = kv_get(kv, name, true);
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

static const char *kv_check(struct KV *kv)
{
	for (int i = 0; i < kv->count; i++) {
		if (!kv->children[i].value) {
			return kv->children[i].key;
		}
	}
	return NULL;
}

struct Runtime {
	struct LOP *lop;

	int unary_count;
	int binary_count;
	int prio;

	const char *key;
	struct SchemaNode *c;
};

static void op_add(struct LOP_OperatorTable **table, int *cnt, struct LOP_OperatorTable *op)
{
	if (*cnt > 0) {
		struct LOP_OperatorTable *t = &(*table)[*cnt - 1];
		assert(op->value);

		*t = *op;
		t->value = strdup(t->value);
		assert(t->value);
	}
	*table = realloc(*table, (*cnt + 1) * sizeof(**table));
	assert(table);
	(*table)[*cnt] = (struct LOP_OperatorTable) {};
	(*cnt)++;
}

static int cb_unary(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;
	struct LOP *lop = r->lop;
	struct LOP_OperatorTable op = {
		.value = LOP_symbol_value(n),
		.prio = r->prio,
		.type = 0,
	};

	op_add(&lop->unary, &r->unary_count, &op);
	return 0;
}

static int cb_binary(struct Runtime *r, struct LOP_ASTNode *n, enum LOP_OperatorType optype)
{
	struct LOP *lop = r->lop;
	struct LOP_OperatorTable op = {
		.value = LOP_symbol_value(n),
		.prio = r->prio,
		.type = optype,
	};

	op_add(&lop->binary, &r->binary_count, &op);
	return 0;
}

static int cb_binary_lr(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	cb_binary(param, n, LOP_OPERATOR_LEFT);
	return 0;
}

static int cb_binary_rl(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	cb_binary(param, n, LOP_OPERATOR_RIGHT);
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
		int rc = LOP_ERROR_SCHEMA_MISSING_HANDLER;
		if (r->lop->error_cb) {
			r->lop->error_cb(rc, (union LOP_Error) { .str = key });
		}
		return rc;
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

	sn_set_identifier(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_number(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_number(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_string(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_string(r->c);

	return LOP_cb_default(hl, NULL, param, NULL);
}

static int cb_sn_set_operator(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg)
{
	struct Runtime *r = param;

	sn_set_operator(r->c);

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
	int unary_count = 0;
	int binary_count = 0;

	op_add(&lop.unary, &unary_count, NULL);
	op_add(&lop.unary, &unary_count, &(struct LOP_OperatorTable) { "$", 0, 0 });
	op_add(&lop.unary, &unary_count, &(struct LOP_OperatorTable) { "@", 0, 0 });
	op_add(&lop.unary, &unary_count, &(struct LOP_OperatorTable) { "#", 0, 0 });
	op_add(&lop.binary, &binary_count, NULL);

	struct KV *kv = lop.kv;

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

	kv_check(kv);

	return lop;
}

void LOP_schema_deinit(struct LOP *lop)
{
	kv_free(lop->kv, (free_value_t)sn_free);

	for (struct LOP_OperatorTable *op = lop->unary; op->value; op++) {
		free((char *)op->value);
	}
	for (struct LOP_OperatorTable *op = lop->binary; op->value; op++) {
		free((char *)op->value);
	}
	free(lop->unary);
	free(lop->binary);
}

int LOP_schema_parse_source(void *ctx, struct LOP *lop, const char *string, const char *key)
{
	struct KV *kv = lop->kv;
	struct LOP_ASTNode *ast;
	int kv_key = kv_get(kv, key, false);
	struct SchemaNode *schema;
	struct LOP_HandlerList hl = {};
	struct LOP_ASTNode *n;
	struct LOP_ASTNode *err = NULL;
	int rc = 0;

	if (kv_key < 0) {
		rc = LOP_ERROR_SCHEMA_MISSING_TOP;
		if (lop->error_cb) {
			lop->error_cb(rc, (union LOP_Error) { .str = key });
		}
		return rc;
	}

	schema = kv->children[kv_key].value;
	rc = LOP_getAST(&ast, string, lop->unary, lop->binary, lop->error_cb);
	if (rc < 0) {
		return rc;
	}
	n = ast;

	if (check_entry(&hl, &n, schema, &err, kv)) {
		assert(hl.count == 1);
		rc = LOP_handler_eval(hl, 0, ctx);
	} else {
		rc = LOP_ERROR_SCHEMA_SYNTAX;

		if (lop->error_cb) {
			if (!err) {
				err = LOP_list_head(ast);
			}
			lop->error_cb(rc, (union LOP_Error) { .syntax = { .node = err, .src = string } });
		}
	}

	handler_free(&hl);

	LOP_delAST(ast);

	return rc;
}

int LOP_schema_init(struct LOP *lop, const char *user_schema)
{
	struct LOP root_schema = root_schema_init();
	struct Runtime r = {
		.lop = lop,
	};
	int rc = 0;

	root_schema.error_cb = lop->error_cb;

	lop->kv = kv_alloc();

	op_add(&lop->unary, &r.unary_count, NULL);
	op_add(&lop->binary, &r.binary_count, NULL);

	rc = LOP_schema_parse_source(&r, &root_schema, user_schema, "root");

	if (!rc) {
		const char *err_key = kv_check(lop->kv);
		if (err_key) {
			rc = LOP_ERROR_SCHEMA_MISSING_RULE;
			if (lop->error_cb) {
				lop->error_cb(rc, (union LOP_Error) { .str = err_key });
			}
		}
	}

	LOP_schema_deinit(&root_schema);

	return rc;
}
