#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LOP.h>

const char *LOP_symbol_value(struct LOP_ASTNode *n)
{
	assert(n->type > LOP_TYPE_LIST_LAST);

	return n->symbol.value;
}

struct LOP_ASTNode *LOP_list_head(struct LOP_ASTNode *n)
{
	assert(n->type < LOP_TYPE_LIST_LAST);

	return n->list.head;
}

struct LOP_ASTNode *LOP_list_tail(struct LOP_ASTNode *n)
{
	assert(n->type < LOP_TYPE_LIST_LAST);

	return n->list.tail;
}

void LOP_delAST(struct LOP_ASTNode *root)
{
	if (root->type < LOP_TYPE_LIST_LAST) {
		for (struct LOP_ASTNode *n = LOP_list_head(root); n;) {
			struct LOP_ASTNode *nn = n->next;
			LOP_delAST(n);
			n = nn;
		}
	} else {
		free(root->symbol.value);
	}
	free(root);
}

static void dump_item(struct LOP_ASTNode *t, int level);

static void dump_indent(int level)
{
	for (int i = 0; i < level; i++) {
		printf("\t");
	}
}

static void dump_list(struct LOP_ASTNode *list, const char *list_brackets, int level)
{
	struct LOP_ASTNode *t = list->list.head;

	if (list->list.call) {
		if (list->type == LOP_TYPE_LIST_OPERATOR_UNARY) {
			printf("(unary ");
		} else if (list->type == LOP_TYPE_LIST_OPERATOR_BINARY) {
			printf("(binary ");
		} else {
			printf("(call '%s' ", list_brackets);
		}
		dump_item(t, level + 1);
		t = t->next;
	} else {
		printf("(list '%s'", list_brackets);
	}

	for (; t; t = t->next) {
		printf("\n");
		dump_indent(level);
		dump_item(t, level + 1);
	}
	printf(")");
}

static void dump_item(struct LOP_ASTNode *t, int level)
{
	if (t == NULL) {
		return;
	}

	switch (t->type) {
	case LOP_TYPE_LIST_ROUND:
		dump_list(t, "()", level);
		break;
	case LOP_TYPE_LIST_CURLY:
		dump_list(t, "{}", level);
		break;
	case LOP_TYPE_LIST_SQUARE:
		dump_list(t, "[]", level);
		break;
	case LOP_TYPE_LIST_COLON:
		dump_list(t, ":;", level);
		break;
	case LOP_TYPE_LIST_OPERATOR_UNARY:
	case LOP_TYPE_LIST_OPERATOR_BINARY:
		dump_list(t, NULL, level);
		break;
	case LOP_TYPE_LIST_STRING:
		dump_list(t, "\"\"", level);
		break;
	case LOP_TYPE_OPERATOR:
		printf("(operator %s)", t->symbol.value);
		break;
	case LOP_TYPE_ID:
		printf("(id %s)", t->symbol.value);
		break;
	case LOP_TYPE_NUMBER:
		printf("(number %s)", t->symbol.value);
		break;
	case LOP_TYPE_STRING:
		printf("(string '%s')", t->symbol.value);
		break;
	case LOP_TYPE_NIL:
		printf("nil");
		break;
	default:
		assert(0);
	}
}

void LOP_dump_ast(struct LOP_ASTNode *root)
{
	dump_item(root, 1);
	printf("\n");
}
