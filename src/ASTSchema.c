#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LOP.h>

#include "KV.c"

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
			char *value;
		} symbol;

		struct {
			bool call;
		} list;
	};
};

static void dump_sn(struct SchemaNode *sn, int level, struct KV *kv)
{
	for (int i = 0; i < level; i++) {
		printf("\t");
	}

#define CASE_SN_TYPE(type)	case type: printf(#type); break
	switch (sn->sn_type) {
		CASE_SN_TYPE(SN_TYPE_AST);
		CASE_SN_TYPE(SN_TYPE_ONEOF);
		CASE_SN_TYPE(SN_TYPE_LISTOF);
		CASE_SN_TYPE(SN_TYPE_SEQOF);
		CASE_SN_TYPE(SN_TYPE_REF);
	}
#undef CASE_SN_TYPE
	if (sn->sn_type == SN_TYPE_REF) {
		printf(", $%s", kv->children[sn->ref].key);
	}
	if (sn->sn_type == SN_TYPE_AST) {
#define CASE_TYPE(type, val) case type: printf(", " val); break
		switch (sn->type) {
		CASE_TYPE(LOP_TYPE_LIST_COLON, "tlist");
		CASE_TYPE(LOP_TYPE_LIST_ROUND, "list");
		CASE_TYPE(LOP_TYPE_LIST_SQUARE, "aref");
		CASE_TYPE(LOP_TYPE_LIST_CURLY, "struct");
		CASE_TYPE(LOP_TYPE_LIST_OPERATOR_UNARY, "unary");
		CASE_TYPE(LOP_TYPE_LIST_OPERATOR_BINARY, "binary");
		CASE_TYPE(LOP_TYPE_LIST_STRING, "string");
		CASE_TYPE(LOP_TYPE_ID, "identifier");
		CASE_TYPE(LOP_TYPE_NUMBER, "number");
		CASE_TYPE(LOP_TYPE_STRING, "string");
		CASE_TYPE(LOP_TYPE_OPERATOR, "operator");
		CASE_TYPE(LOP_TYPE_NIL, "nil");
		default:
			assert(0);
		}
#undef CASE_TYPE
		if (sn->type > LOP_TYPE_LIST_LAST && sn->symbol.value) {
			printf(", \"%s\"", sn->symbol.value);
		}
	}
	if (sn->optional) {
		printf(", #optional");
	}
	if (sn->last) {
		printf(", #last");
	}
	printf("\n");
}

static void dump_sn_recurse(struct SchemaNode *sn, int level, struct KV *kv)
{
	dump_sn(sn, level, kv);

	for (int i = 0; i < sn->child_count; i++) {
		dump_sn_recurse(sn->child[i], level + 1, kv);
	}
}

static int s_report(enum LOP_ErrorType type, const char *str)
{
	switch (type) {
	case LOP_ERROR_SCHEMA_SYNTAX:
		fprintf(stderr, "Syntax error\n");
		break;
	case LOP_ERROR_SCHEMA_MISSING_RULE:
		fprintf(stderr, "Rule '%s' not found\n", str);
		break;
	case LOP_ERROR_SCHEMA_MISSING_HANDLER:
		fprintf(stderr, "Handler '%s' not found\n", str);
		break;
	case LOP_ERROR_SCHEMA_MISSING_TOP:
		fprintf(stderr, "Top rule '%s' not found\n", str);
		break;
	default:
		assert(1);
	}

	return type;
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

static bool check_entry(struct LOP_HandlerList *hl, struct LOP_ASTNode **n, struct SchemaNode *c, struct LOP_ASTNode **err, struct KV *kv);

static bool check_oneof(struct LOP_HandlerList *hl, struct LOP_ASTNode **n, struct SchemaNode *c, struct LOP_ASTNode **err, struct KV *kv)
{
	/* Find the first SchemaNode which consumes the ASTNode */
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

	/* Consume as much ASTNodes as possible */
	while (check_oneof(hl, n, c, err, kv)) {
		found = true;
	}
	return found;
}

static bool check_seqof(struct LOP_HandlerList *hl, struct LOP_ASTNode **n, struct SchemaNode *c, struct LOP_ASTNode **err, struct KV *kv)
{
	int i = 0;

	for (; i < c->child_count; i++) {
		struct SchemaNode *ci = c->child[i];

		if (check_entry(hl, n, ci, err, kv)) {
			/* If all ASTNodes are consumed, then ... */
			if (!*n) {
				i++;
				break;
			}
		} else if (ci->optional) {
			/* We must not make holes in the handler list,
			 * so that the user can properly trigger its callbacks
			 * according to the schema node placement.
			 * Storing the handler number is just meh,
			 * because lookup and call will have for-loop which is meh */
			handler_add(hl, NULL, NULL);
		} else {
			return false;
		}
	}

	/* ... the rest of the SchemaNodes must be optional */
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
	struct LOP_ASTNode *n_orig = *n;
	struct LOP_ASTNode *nn = *n;
	int hl_count = hl->count;

	if (!nn) {
		return false;
	}

	/* If user provided the cb, then the cb will be called,
	 * if not, either NULL or default cb will be called.
	 * The latter one will just iterate over its children
	 * and call them one after another */
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
		/* Check SchemaNode and ASTNode for "equality" */
		if (c->type != nn->type) {
			goto mismatch;
		}

		if (c->type < LOP_TYPE_LIST_LAST) {
			struct LOP_ASTNode *h = LOP_list_head(nn);

			if (c->list.call != nn->list.call) {
				goto mismatch;
			}
			if (!check_seqof(handler_tail(hl), &h, c, err, kv)) {
				goto mismatch;
			}
			/* SchemaNode's children does not consume ASTNode's children */
			if (h) {
				goto mismatch;
			}
		} else {
			if (c->symbol.value != NULL) {
				if (strcmp(c->symbol.value, nn->symbol.value)) {
					goto mismatch;
				}
			}
		}

		/* Consume this ASTNode and move to the next */
		*n = nn->next;
		/* Update the err indicator */
		if (*n) {
			*err = *n;
		}
		break;
	default:
		assert(0);
	}

	/* If SchemaNode is marked as last, then no ASTNode must be left */
	if (c->last && *n) {
		goto mismatch;
	}

	return true;

mismatch:
	/* Rewind everything back, so that we can procceed the search with another SchemaNodes */
	*n = n_orig;
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

static int kv_dump_sn(void *arg, struct KVEntry *kv)
{
	struct SchemaNode *schema = kv->value;

	printf("%s:\n", kv->key);
	dump_sn_recurse(schema, 1, arg);
	return 0;
}

int LOP_schema_parse_source(void *ctx, struct LOP *lop, const char *filename, const char *string, size_t len, const char *top_rule_name)
{
	struct KV *kv = lop->kv;
	struct LOP_ASTNode *ast;
	struct SchemaNode *schema;
	struct LOP_HandlerList hl = {};
	struct LOP_ASTNode *n;
	struct LOP_ASTNode *err = NULL;
	int kv_key;
	int rc = 0;

	/* If you want to see how it's look */
	if (0) {
		kv_iterate(kv, kv_dump_sn, kv);
	}

	/* Get the top rule from which everything starts */
	kv_key = kv_get_index(kv, top_rule_name, false);
	if (kv_key < 0) {
		return s_report(LOP_ERROR_SCHEMA_MISSING_TOP, top_rule_name);
	}
	schema = kv->children[kv_key].value;

	/* Translate the source text to the AST */
	rc = LOP_getAST(&ast, filename, string, len, &lop->operator_table);
	if (rc < 0) {
		return rc;
	}
	n = ast;

	/* Apply schema to the AST and get a tree of handlers to call */
	if (check_entry(&hl, &n, schema, &err, kv)) {
		assert(hl.count == 1);
		/* Parsing succeeded, call the handlers to complete the job */
		rc = LOP_handler_eval(hl, 0, ctx);
	} else {
		LOP_dump_ast(err);
		return s_report(LOP_ERROR_SCHEMA_SYNTAX, NULL);
	}

	handler_free(&hl);

	/* AST has allocs, we must free them */
	LOP_delAST(ast);
	return rc;
}

#include "RootSchema.c"
