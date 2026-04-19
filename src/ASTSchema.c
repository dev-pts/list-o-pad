#include <assert.h>
#include <ctype.h>
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

	const char *key;

	struct SchemaNode *parent;
	struct SchemaNode *next;

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
		if (sn->type < LOP_TYPE_LIST_LAST_CALLABLE && sn->list.call) {
			printf(", call");
		}
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
	case LOP_ERROR_SCHEMA_MISSING_RULE:
		fprintf(stderr, "Rule '%s' not found\n", str);
		break;
	case LOP_ERROR_SCHEMA_MISSING_TOP:
		fprintf(stderr, "Top rule '%s' not found\n", str);
		break;
	default:
		assert(1);
	}

	return type;
}

static void handler_resize(struct LOP_HandlerList *hl, int count)
{
	hl->count = count;
	hl->handler = realloc(hl->handler, hl->count * sizeof(*hl->handler));
	if (hl->count) {
		assert(hl->handler);
	}
}

static void handler_add(struct LOP_HandlerList *hl, struct SchemaNode *sn, struct LOP_ASTNode *n, int delta)
{
	/* FIXME remove? */
	if (!sn->key) {
		return;
	}

	handler_resize(hl, hl->count + 1);

	hl->handler[hl->count - 1] = (struct LOP_Handler) { sn->key, n, delta };
}

struct Context {
	struct KV *kv;
	struct LOP_HandlerList *hl;
};

/* Because we have refs which have parent NULL,
 * but when they in tree, they do have parent.
 * That's why we need to construct this list in runtime. */
struct CEContext {
	struct SchemaNode *sn;
	struct CEContext *parent;
};

static bool check_optional(struct Context *ctx, struct SchemaNode *sn)
{
	if (sn->optional) {
		return true;
	}

	switch (sn->sn_type) {
	case SN_TYPE_ONEOF:
	case SN_TYPE_LISTOF:
	case SN_TYPE_SEQOF:
		return false;
	case SN_TYPE_REF:
		return check_optional(ctx, ctx->kv->children[sn->ref].value);
	case SN_TYPE_AST:
		return false;
	default:
		assert(0);
	}
}

/* "Close" SNs, until it's AST */
static bool sn_ast_up(struct Context *ctx, struct CEContext *cec)
{
	struct CEContext *save = cec;
	struct CEContext *cecp = cec->parent;

	while (cecp) {
		handler_add(ctx->hl, cecp->sn, NULL, -1);

		switch (cecp->sn->sn_type) {
		case SN_TYPE_ONEOF:
		case SN_TYPE_LISTOF:
		case SN_TYPE_REF:
			break;
		case SN_TYPE_SEQOF:
		case SN_TYPE_AST:
			for (struct SchemaNode *sn = cec->sn->next; sn; sn = sn->next) {
				if (!check_optional(ctx, sn)) {
					return false;
				}
			}
			break;
		}

		if (cecp->sn->sn_type == SN_TYPE_AST) {
			break;
		}
		cec = cecp;
		cecp = cecp->parent;
	}

	if (!cecp) {
		save->sn = NULL;
		save->parent = NULL;
		return true;
	}

	*save = *cecp;
	return true;
}

/* Traverses all the possibilities, until the first success.
 * Uses recursion and native stack to track the state. */
static bool check_entry(struct Context *ctx, struct LOP_ASTNode *ast, struct CEContext *cec)
{
	struct CEContext cec_next = {
		.parent = cec,
	};
	struct SchemaNode *sn = cec->sn;
	struct LOP_HandlerList *hl = ctx->hl;
	int hl_count = hl->count;

	if (ast->parsed == 0) {
		ast->parsed = 1;
	}

	if (0) {
		dump_sn_recurse(sn, 0, ctx->kv);
		LOP_dump_ast(ast);
	}

	switch (sn->sn_type) {
	case SN_TYPE_ONEOF:
	case SN_TYPE_LISTOF:
		handler_add(hl, sn, ast, 1);

		for (int i = 0; i < sn->child_count; i++) {
			cec_next.sn = sn->child[i];

			if (check_entry(ctx, ast, &cec_next)) {
				return true;
			}
		}
		goto mismatch;
	case SN_TYPE_SEQOF:
		handler_add(hl, sn, ast, 1);

		for (int i = 0; i < sn->child_count; i++) {
			cec_next.sn = sn->child[i];

			if (check_entry(ctx, ast, &cec_next)) {
				return true;
			}

			if (!check_optional(ctx, sn->child[i])) {
				goto mismatch;
			}
		}
		goto mismatch;
	case SN_TYPE_REF:
		handler_add(hl, sn, ast, 1);

		cec_next.sn = ctx->kv->children[sn->ref].value;

		if (check_entry(ctx, ast, &cec_next)) {
			return true;
		}
		goto mismatch;
	case SN_TYPE_AST:
		if (!ast) {
			goto mismatch;
		}

		if (sn->type != ast->type) {
			goto mismatch;
		}

		if (sn->type < LOP_TYPE_LIST_LAST) {
			if (sn->list.call != ast->list.call) {
				goto mismatch;
			}

			handler_add(hl, sn, ast, 1);

			for (int i = 0; i < sn->child_count; i++) {
				cec_next.sn = sn->child[i];

				if (LOP_list_head(ast)) {
					if (check_entry(ctx, LOP_list_head(ast), &cec_next)) {
						return true;
					}
				}

				if (!check_optional(ctx, sn->child[i])) {
					goto mismatch;
				}
			}

			if (LOP_list_head(ast)) {
				goto mismatch;
			}

			handler_add(hl, sn, NULL, -1);
		} else {
			handler_add(hl, sn, ast, 0);

			if (sn->symbol.value != NULL) {
				if (strcmp(sn->symbol.value, ast->symbol.value)) {
					goto mismatch;
				}
			}
		}

		break;
	default:
		assert(0);
	}

	assert(ast->parsed == 1);
	ast->parsed = 2;

	struct LOP_ASTNode *next_ast = ast->next;
	struct CEContext cec2 = *cec;

	while (next_ast == NULL) {
		if (!sn_ast_up(ctx, &cec2)) {
			goto mismatch;
		}
		ast = ast->parent;
		if (ast) {
			assert(ast->parsed == 1);
			ast->parsed = 2;
		}
		if (!ast) {
			assert(cec2.sn == NULL);
			return true;
		}
		next_ast = ast->next;
	}

	cec = &cec2;

	struct CEContext *save;

	assert(next_ast);

	if (next_ast->parsed == 0) {
		next_ast->parsed = 1;
	}

	if (0) {
		dump_sn_recurse(sn, 0, ctx->kv);
		LOP_dump_ast(next_ast);
	}

again:
	save = cec;
	cec = cec->parent;

	while (cec && (cec->sn->sn_type == SN_TYPE_ONEOF || cec->sn->sn_type == SN_TYPE_REF)) {
		handler_add(ctx->hl, cec->sn, NULL, -1);

		save = cec;
		cec = cec->parent;
	}

	if (!cec) {
		/* We have next_ast, but no SNs left */
		goto mismatch;
	}

	sn = cec->sn;
	cec_next.parent = cec;

	if (0) {
		dump_sn_recurse(sn, 0, ctx->kv);
		LOP_dump_ast(next_ast);
	}

	if (sn->sn_type == SN_TYPE_LISTOF) {
		for (int i = 0; i < sn->child_count; i++) {
			cec_next.sn = sn->child[i];

			if (check_entry(ctx, next_ast, &cec_next)) {
				return true;
			}
		}

		handler_add(ctx->hl, cec->sn, NULL, -1);
		goto again;
	}

	if (sn->sn_type == SN_TYPE_SEQOF) {
		for (sn = save->sn->next; sn; sn = sn->next) {
			cec_next.sn = sn;

			if (check_entry(ctx, next_ast, &cec_next)) {
				return true;
			}

			if (!check_optional(ctx, sn)) {
				goto mismatch;
			}
		}

		handler_add(ctx->hl, cec->sn, NULL, -1);
		goto again;
	}

	if (sn->sn_type == SN_TYPE_AST) {
		for (sn = save->sn->next; sn; sn = sn->next) {
			cec_next.sn = sn;

			if (check_entry(ctx, next_ast, &cec_next)) {
				return true;
			}

			if (!check_optional(ctx, sn)) {
				goto mismatch;
			}
		}
		goto mismatch;
	}

	assert(sn->sn_type != SN_TYPE_REF);

mismatch:
	handler_resize(hl, hl_count);
	return false;
}

static int kv_dump_sn(void *arg, struct KVEntry *kv)
{
	struct SchemaNode *sn = kv->value;

	printf("%s:\n", kv->key);
	dump_sn_recurse(sn, 1, arg);
	return 0;
}

#include "ErrorReport.c"

static struct LOP_ASTNode *ast_find_err(struct LOP_ASTNode *t)
{
	if (t == NULL || t->parsed != 1) {
		return NULL;
	}

	if (t->type < LOP_TYPE_LIST_LAST) {
		for (struct LOP_ASTNode *i = LOP_list_head(t); i; i = i->next) {
			struct LOP_ASTNode *ret = ast_find_err(i);

			if (ret) {
				return ret;
			}
		}
	}

	return t;
}

int LOP_init(struct LOP *lop, const char *src, size_t len)
{
	struct LOP_Schema *schema = lop->schema;
	struct KV *kv = schema->kv;
	struct LOP_ASTNode *ast;
	struct SchemaNode *sn;
	struct Context lop_ctx = {
		.kv = kv,
		.hl = &lop->hl,
	};
	int kv_key;
	int rc = 0;

	/* If you want to see how it's look */
	if (0) {
		kv_iterate(kv, kv_dump_sn, kv);
	}

	/* Get the top rule from which everything starts */
	kv_key = kv_get_index(kv, lop->top_rule_name, false);
	if (kv_key < 0) {
		return s_report(LOP_ERROR_SCHEMA_MISSING_TOP, lop->top_rule_name);
	}
	sn = kv->children[kv_key].value;

	/* Translate the source text to the AST */
	rc = LOP_getAST(&ast, lop->filename, src, len, &schema->operator_table);
	if (rc < 0) {
		return rc;
	}

	struct CEContext cec = {
		.sn = sn,
		.parent = NULL,
	};

	/* Apply sn to the AST and get a tree of handlers to call */
	if (check_entry(&lop_ctx, ast, &cec)) {
		assert(lop->hl.count);

		lop->ast = ast;
	} else {
		struct LOP_ASTNode *err = ast_find_err(ast);

		assert(err);

		/* Get to the head, otherwise the error will point, for example,
		 * to the :, instead of the first element, which is more convenient. */
		while (err->type < LOP_TYPE_LIST_LAST) {
			err = LOP_list_head(err);
		}

		report_error(lop->filename, src, len, err->loc, "Syntax error");
		rc = LOP_ERROR_SCHEMA_SYNTAX;

		/* AST has allocs, we must free them */
		LOP_delAST(ast);
	}

	return rc;
}

void LOP_deinit(struct LOP *lop)
{
	LOP_delAST(lop->ast);

	free(lop->hl.handler);

	lop->ast = NULL;
	lop->hl.count = 0;
	lop->hl.handler = NULL;
}

#include "RootSchema.c"
