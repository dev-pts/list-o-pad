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

static void error_show_string(const char *ptr, size_t len, struct LOP_Location loc)
{
	size_t lineno = 1;
	size_t i = 0;

	for (; i < len; i++) {
		if (lineno == loc.lineno) {
			break;
		}
		if (ptr[i] == '\n') {
			lineno++;
			continue;
		}
	}

	if (i >= len) {
		fprintf(stderr, "at the end of file\n");
		return;
	}

	fprintf(stderr, "%li: ", lineno);
	for (; i < len; i++) {
		if (ptr[i] == '\n') {
			break;
		}
		fprintf(stderr, "%c", ptr[i]);
	}
	fprintf(stderr, "\n");
}

void LOP_default_error_cb(enum LOP_ErrorType type, union LOP_Error error)
{
	switch (type) {
	case LOP_ERROR_TOKEN_UNKNOWN:
		fprintf(stderr, "Unknown symbol at:\n");
		error_show_string(error.token.str, error.token.len, error.token.loc);
		break;
	case LOP_ERROR_TOKEN_BAD_INDENT:
		fprintf(stderr, "Bad indent at:\n");
		error_show_string(error.token.str, error.token.len, error.token.loc);
		fprintf(stderr, "Expected indent: %i\n", error.token.expindent);
		fprintf(stderr, "Actual indent: %i\n", error.token.actindent);
		break;
	case LOP_ERROR_TOKEN_BAD_INDENT_CLOSE:
		fprintf(stderr, "Bad indent for closing at:\n");
		error_show_string(error.token.str, error.token.len, error.token.loc);
		fprintf(stderr, "Expected indent: %i\n", error.token.expindent);
		fprintf(stderr, "Actual indent: %i\n", error.token.actindent);
		break;
	case LOP_ERROR_TOKEN_UNBALANCED:
		fprintf(stderr, "Unbalanced list at:\n");
		error_show_string(error.token.str, error.token.len, error.token.loc);
		break;
	case LOP_ERROR_TOKEN_SEPARATOR:
		fprintf(stderr, "Separator expected at:\n");
		error_show_string(error.token.str, error.token.len, error.token.loc);
		break;
	case LOP_ERROR_TOKEN_UNARY_ARGS:
		fprintf(stderr, "Unary operator expects exactly 1 argument at:\n");
		error_show_string(error.token.str, error.token.len, error.token.loc);
		break;
	case LOP_ERROR_TOKEN_UNARY_UNKNOWN:
		fprintf(stderr, "Unknown unary operator '%s' at:\n", error.token.value);
		error_show_string(error.token.str, error.token.len, error.token.loc);
		free((void *)error.token.value);
		break;
	case LOP_ERROR_TOKEN_BINARY_ARGS:
		fprintf(stderr, "Binary operator expects exactly 2 arguments at:\n");
		error_show_string(error.token.str, error.token.len, error.token.loc);
		break;
	case LOP_ERROR_TOKEN_BINARY_UNKNOWN:
		fprintf(stderr, "Unknown binary operator '%s' at:\n", error.token.value);
		error_show_string(error.token.str, error.token.len, error.token.loc);
		free((void *)error.token.value);
		break;
	case LOP_ERROR_SCHEMA_SYNTAX:
		if (!error.syntax.node) {
			fprintf(stderr, "Syntax error for the whole file\n");
			break;
		}
		fprintf(stderr, "Syntax error at:\n");
		error_show_string(error.syntax.src, strlen(error.syntax.src), error.syntax.node->loc);
		break;
	case LOP_ERROR_SCHEMA_MISSING_RULE:
		fprintf(stderr, "Rule '%s' not found\n", error.str);
		break;
	case LOP_ERROR_SCHEMA_MISSING_HANDLER:
		fprintf(stderr, "Handler '%s' not found\n", error.str);
		break;
	case LOP_ERROR_SCHEMA_MISSING_TOP:
		fprintf(stderr, "Top rule '%s' not found\n", error.str);
		break;
	}
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

void LOP_dump_ast(struct LOP_ASTNode *root, bool pretty)
{
	dump_item(root, 1);
	printf("\n");
}

void LOP_op_prepend(struct LOP_OperatorTable **table, int *cnt, struct LOP_OperatorTable *op)
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
