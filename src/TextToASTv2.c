#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <LOP.h>

extern int charno;
extern int yylineno;
extern int yyleng;
extern char *yytext;
static int indent;

static struct LOP_ASTNode *last_list;
static struct LOP_ASTNode *last_token;
static int newline_was;
static int continue_was;

static void error(struct LOP_ASTNode *t)
{
	printf("SYNTAX ERROR at %i, %i\n", t->loc.lineno, t->loc.charno);
	abort();
}

static struct LOP_ASTNode *create_token(enum LOP_ASTNodeType type)
{
	struct LOP_ASTNode *t = calloc(1, sizeof(*t));

	t->type = type;
	t->loc.lineno = yylineno;
	t->loc.charno = charno - yyleng;
	t->indent = indent;

	if (type > LOP_TYPE_LIST_LAST) {
		if (type == LOP_TYPE_STRING) {
			t->symbol.value = strdup("");
		} else {
			t->symbol.value = strdup(yytext);
		}
	}

	return t;
}

static struct LOP_ASTNode *swap_token(struct LOP_ASTNode *t)
{
	struct LOP_ASTNode old;

	old = *last_token;
	*last_token = *t;
	*t = old;

	t->parent = NULL;

	return t;
}

static int vert_colon_closed(void)
{
	if (last_list->type != LOP_TYPE_LIST_COLON) {
		return 0;
	}
	if (newline_was == 0) {
		return 0;
	}
	if (continue_was == 1) {
		return 0;
	}
	if (indent <= last_list->indent) {
		return 1;
	}
	if (last_list->list.tail && last_list->list.tail->indent > last_list->indent && indent < last_list->list.tail->indent) {
		return 1;
	}
	return 0;
}

static int last_list_is_operator(void)
{
	return last_list->type == LOP_TYPE_LIST_OPERATOR_UNARY || last_list->type == LOP_TYPE_LIST_OPERATOR_BINARY;
}

static void operator_close(void)
{
	while (last_list_is_operator()) {
		last_list = last_list->parent;
	}
}

static void close_vert_colon(void)
{
	while (vert_colon_closed()) {
		last_list = last_list->parent;

		/* Unary/binary lists must be closed for proper grouping:
		 * %a: b\nc must be the (list ':;' (unary % (call ':;' b)) c)
		 * and not the (list ':;' (unary % (call ':;' b) c)) */
		operator_close();

		/* :;\n() must not be the call of :; */
		last_token = NULL;
	}
}

static void push_token(struct LOP_ASTNode *t)
{
	close_vert_colon();

	newline_was = 0;
	continue_was = 0;

	t->parent = last_list;

	if (last_token) {
		if (t->type < LOP_TYPE_LIST_LAST) {
			t->list.call = 1;

			t = swap_token(t);

			last_list = last_token;

			t->parent = last_list;

			last_list->list.head = last_list->list.tail = t;
			last_token = NULL;
		} else {
			error(t);
		}
	} else {
		if (last_list->list.tail) {
			last_list->list.tail->next = t;
		} else {
			last_list->list.head = t;
		}
		last_list->list.tail = t;

		if (t->type < LOP_TYPE_LIST_LAST) {
			last_list = t;
			last_token = NULL;
		} else {
			last_token = t;
		}
	}
}

static void l_str_open()
{
	if (last_token) {
		push_token(create_token(LOP_TYPE_LIST_STRING));
	}
	push_token(create_token(LOP_TYPE_STRING));
}

static void l_str_append()
{
	last_token->symbol.value = realloc((char *)last_token->symbol.value, strlen(last_token->symbol.value) + yyleng + 1);
	strcat((char *)last_token->symbol.value, yytext);
}

static void l_str_close()
{
	if (last_list->type == LOP_TYPE_LIST_STRING) {
		last_list = last_list->parent;
		last_token = last_list->list.tail;
	}
}

static struct LOP_OperatorTable *ot_find(struct LOP_OperatorTable *optable, const char *value)
{
	for (struct LOP_OperatorTable *ot = optable; ot->value; ot++) {
		if (!strcmp(ot->value, value)) {
			return ot;
		}
	}
	return NULL;
}

static void l_operator(struct LOP_OperatorTable *unary, struct LOP_OperatorTable *binary)
{
	struct LOP_ASTNode *t = create_token(LOP_TYPE_OPERATOR);
	struct LOP_OperatorTable *ot = NULL;

	if (last_token) {
		assert(binary);
		ot = ot_find(binary, yytext);
		assert(ot);

		while (last_list->type > LOP_TYPE_LIST_LAST_CALLABLE && last_list->list.prio < ot->prio) {
			last_list = last_list->parent;
			last_token = last_list->list.tail;
		}

		last_token = last_list->list.tail;

		t = swap_token(t);

		push_token(create_token(LOP_TYPE_LIST_OPERATOR_BINARY));
		push_token(t);

		/* a(b) + c must be (binary + (call a '()' b) c)
		 * and not the (binary + (call a '()' b c)) */
		last_list = t->parent;
		last_token = NULL;
	} else {
		push_token(t);
		push_token(create_token(LOP_TYPE_LIST_OPERATOR_UNARY));

		assert(unary);
		ot = ot_find(unary, yytext);
		assert(ot);
	}

	last_list->list.prio = ot->prio;
}

static void l_push_token(enum LOP_ASTNodeType t)
{
	if (t < LOP_TYPE_LIST_LAST_CALLABLE && last_list->type > LOP_TYPE_LIST_LAST_CALLABLE) {
		/* Priority 0 is needed to have these possibilites:
		 * 1. $a() => (call '()' (unary (operator $) (identifier a)))
		 * 2. -b() => (unary (call '()' (operator $) (identifier b)))
		 * 3. a - b() => (binary (operator -) (identifier a) (call '()' (identifier b)))
		 * 3. a->b() => (call '()' (binary (operator ->) (identifier a) (identifier b)))
		 */
		if (last_list->list.prio == 0) {
			last_list = last_list->parent;
			last_token = last_list->list.tail;
		}
	}

	push_token(create_token(t));
}

static void l_list_close(void)
{
	while (last_list_is_operator() || last_list->type == LOP_TYPE_LIST_COLON) {
		last_list = last_list->parent;
	}
	last_list = last_list->parent;
	last_token = last_list->list.tail;
}

static void l_tlist_close(void)
{
	close_vert_colon();
	operator_close();

	last_list = last_list->parent;
	last_token = last_list->list.tail;
}

static void l_indent()
{
	if (charno - 1 == indent + 1) {
		indent++;
	}
}

static void l_newline()
{
	if (continue_was == 0) {
		operator_close();
		last_token = NULL;
	}

	charno = 1;
	indent = 0;
	newline_was = 1;
}

static void l_comma()
{
	if (last_token == NULL) {
		push_token(create_token(LOP_TYPE_NIL));
	}

	operator_close();
	last_token = NULL;
}

#include "lex.yy.c"

int yywrap() {
	return 1;
}

int LOP_getAST(struct LOP_ASTNode **root, const char *string, size_t len, struct LOP_OperatorTable *unary, struct LOP_OperatorTable *binary, LOP_error_cb_t error_cb)
{
	enum Token t;

	charno = 1;
	yylineno = 1;
	indent = -1;
	newline_was = 1;
	*root = last_list = create_token(LOP_TYPE_LIST_COLON);
	indent = 0;

	yy_scan_bytes(string, len);

	while ((t = yylex())) {
		switch (t) {
		case L_DIGIT:
		case L_FLOAT:
		case L_BNUMBER:
			l_push_token(LOP_TYPE_NUMBER);
			break;
		case L_ID:
			l_push_token(LOP_TYPE_ID);
			break;
		case L_SQUOTE:
		case L_DQUOTE:
			l_str_open();
			break;
		case L_STR_APPEND:
			l_str_append();
			break;
		case L_STR_CONTINUE:
			break;
		case L_QUOTE_CLOSE:
			l_str_close();
			break;
		case L_OPERATOR:
			l_operator(unary, binary);
			break;
		case L_LIST_OPEN:
			l_push_token(LOP_TYPE_LIST_ROUND);
			break;
		case L_CLIST_OPEN:
			l_push_token(LOP_TYPE_LIST_CURLY);
			break;
		case L_BLIST_OPEN:
			l_push_token(LOP_TYPE_LIST_SQUARE);
			break;
		case L_TLIST_OPEN:
			l_push_token(LOP_TYPE_LIST_COLON);
			break;
		case L_LIST_CLOSE:
		case L_CLIST_CLOSE:
		case L_BLIST_CLOSE:
			l_list_close();
			break;
		case L_TLIST_CLOSE:
			l_tlist_close();
			break;
		case L_INDENT:
			l_indent();
			break;
		case L_NEWLINE:
			l_newline();
			break;
		case L_CONTINUE:
			continue_was = 1;
			break;
		case L_COMMA:
			l_comma();
			break;
		default:
			assert(0);
		}
	}

	return 0;
}
