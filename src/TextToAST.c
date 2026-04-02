#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <LOP.h>

#include "lex.yy.c"

static struct LOP_ASTNode *last_list;
static struct LOP_ASTNode *last_token;
static int indent;
static int newline_was;
static int continue_was;
static size_t l_line_offset;
static struct LOP_Location last_loc;
static size_t l_str_offset;

#include "ErrorReport.c"

static void l_report(enum LOP_ErrorType type, const char *filename, const char *string, size_t len, struct LOP_Location loc)
{
	const char *err_string = NULL;

	switch (type) {
	case LOP_ERROR_LEXER_OUT_OF_MEMORY:
		err_string = "Out of memory";
		break;
	case LOP_ERROR_LEXER_UNKNOWN:
		err_string = "Unknown symbol";
		break;
	case LOP_ERROR_LEXER_UNBALANCED:
		err_string = "Unbalanced list";
		break;
	case LOP_ERROR_LEXER_ROOT_CLOSED:
		err_string = "Root list is closed by ;";
		break;
	case LOP_ERROR_LEXER_ROOT_CLOSED_BY_INDENT:
		err_string = "Root list is closed by indent";
		break;
	case LOP_ERROR_LEXER_SEPARATOR:
		err_string = "Separator expected";
		break;
	case LOP_ERROR_LEXER_UNARY_ARGS:
		err_string = "Unary operator expects exactly 1 argument";
		break;
	case LOP_ERROR_LEXER_UNARY_UNKNOWN:
		err_string = "Unknown unary operator";
		break;
	case LOP_ERROR_LEXER_BINARY_ARGS:
		err_string = "Binary operator expects exactly 2 arguments";
		break;
	case LOP_ERROR_LEXER_BINARY_UNKNOWN:
		err_string = "Unknown binary operator";
		break;
	default:
		assert(1);
	}

	report_error(filename, string, len, loc, err_string);
}

static struct LOP_Location get_loc(void)
{
	return (struct LOP_Location) {
		.lineno = yylineno,
		.charno = l_str_offset - l_line_offset,
		.line_offset = l_line_offset,
	};
}

static struct LOP_ASTNode *create_token(enum LOP_ASTNodeType type)
{
	struct LOP_ASTNode *t = calloc(1, sizeof(*t));

	if (t == NULL) {
		return NULL;
	}

	t->type = type;
	t->loc = get_loc();
	t->indent = indent;

	if (type > LOP_TYPE_LIST_LAST) {
		if (type == LOP_TYPE_STRING) {
			t->symbol.value = strdup("");
		} else {
			t->symbol.value = strdup(yytext);
		}

		if (t->symbol.value == NULL) {
			free(t);
			return NULL;
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

static int operator_close_verify(void)
{
	if (last_list->type == LOP_TYPE_LIST_OPERATOR_UNARY) {
		if (last_list->list.head->next != last_list->list.tail) {
			return LOP_ERROR_LEXER_UNARY_ARGS;
		}
	} else {
		if (last_list->list.head->next && last_list->list.head->next->next != last_list->list.tail) {
			return LOP_ERROR_LEXER_BINARY_ARGS;
		}
	}
	return 0;
}

static int operator_close(void)
{
	while (last_list_is_operator()) {
		int rc = operator_close_verify();
		if (rc < 0) {
			return rc;
		}
		last_list = last_list->parent;
	}

	return 0;
}

static int close_vert_colon(void)
{
	while (vert_colon_closed()) {
		int rc;

		last_list = last_list->parent;
		if (last_list == NULL) {
			return LOP_ERROR_LEXER_ROOT_CLOSED_BY_INDENT;
		}

		/* Unary/binary lists must be closed for proper grouping:
		 * %a: b\nc must be the (list ':;' (unary % (call ':;' b)) c)
		 * and not the (list ':;' (unary % (call ':;' b) c)) */
		rc = operator_close();
		if (rc < 0) {
			return rc;
		}

		/* :\n() must not be the call of :; */
		last_token = NULL;
	}

	return 0;
}

static int push_token(struct LOP_ASTNode *t)
{
	int rc;

	if (t == NULL) {
		return LOP_ERROR_LEXER_OUT_OF_MEMORY;
	}

	rc = close_vert_colon();
	if (rc < 0) {
		return rc;
	}

#if 0
	Here could be something like this:

	if (last_list->type == LOP_TYPE_LIST_COLON) {
		if (newline_was && continue_was == 0 && last_list->indent + 1 != indent) {
			return LOP_ERROR_LEXER_INDENTATION;
		}
	}

	but, we should allow this:

	a: b:
			c
		d

	just for the sake of user-friendly.
	But this opens the problem with this:

	a:
			b
		c
	d

	'c' goes to root list, and 'd' goes to NULL.
#endif

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
			return LOP_ERROR_LEXER_SEPARATOR;
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

	return 0;
}

static int l_str_open()
{
	if (last_token) {
		int rc = push_token(create_token(LOP_TYPE_LIST_STRING));
		if (rc < 0) {
			return rc;
		}
	}
	return push_token(create_token(LOP_TYPE_STRING));
}

static void set_newline(void)
{
	l_line_offset = l_str_offset + yyleng;
	newline_was = 1;
}

static int l_str_append()
{
	last_token->symbol.value = realloc((char *)last_token->symbol.value, strlen(last_token->symbol.value) + yyleng + 1);

	if (last_token->symbol.value == NULL) {
		return LOP_ERROR_LEXER_OUT_OF_MEMORY;
	}

	strcat((char *)last_token->symbol.value, yytext);

	if (yytext[yyleng - 1] == '\n') {
		set_newline();
	}
	return 0;
}

static int l_str_close()
{
	if (last_list->type == LOP_TYPE_LIST_STRING) {
		last_list = last_list->parent;
	}
	last_token = last_list->list.tail;
	newline_was = 0;
	return 0;
}

static struct LOP_Operator *op_find(struct LOP_OperatorTable *table, const char *value, unsigned mask)
{
	for (int i = 0; i < table->size; i++) {
		struct LOP_Operator *op = &table->data[i];

		if ((op->type & mask) == 0) {
			continue;
		}
		if (!strcmp(op->value, value)) {
			return op;
		}
	}
	return NULL;
}

static int l_operator(struct LOP_OperatorTable *operator_table)
{
	struct LOP_ASTNode *t = create_token(LOP_TYPE_OPERATOR);
	struct LOP_Operator *op = NULL;
	int rc;

	if (t == NULL) {
		return LOP_ERROR_LEXER_OUT_OF_MEMORY;
	}

	if (last_token) {
		op = op_find(operator_table, yytext, LOP_OPERATOR_BINARY_MASK);
		if (op == NULL) {
			return LOP_ERROR_LEXER_BINARY_UNKNOWN;
		}

		while (last_list->type > LOP_TYPE_LIST_LAST_CALLABLE) {
			if (last_list->list.prio > op->prio) {
				break;
			}
			if (last_list->list.prio == op->prio && op->type == LOP_OPERATOR_RTL) {
				break;
			}
			last_list = last_list->parent;
			last_token = last_list->list.tail;
		}

		last_token = last_list->list.tail;

		t = swap_token(t);

		rc = push_token(create_token(LOP_TYPE_LIST_OPERATOR_BINARY));
		if (rc < 0) {
			return rc;
		}

		rc = push_token(t);
		if (rc < 0) {
			return rc;
		}

		/* a(b) + c must be (binary + (call a '()' b) c)
		 * and not the (binary + (call a '()' b c)) */
		last_list = t->parent;
		last_token = NULL;
	} else {
		op = op_find(operator_table, yytext, LOP_OPERATOR_UNARY);
		if (op == NULL) {
			return LOP_ERROR_LEXER_UNARY_UNKNOWN;
		}

		rc = push_token(t);
		if (rc < 0) {
			return rc;
		}

		rc = push_token(create_token(LOP_TYPE_LIST_OPERATOR_UNARY));
		if (rc < 0) {
			return rc;
		}
	}

	last_list->list.prio = op->prio;
	return 0;
}

static int l_push_token(enum LOP_ASTNodeType t)
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

	return push_token(create_token(t));
}

static int l_list_close(enum LOP_ASTNodeType t)
{
	if (t == LOP_TYPE_LIST_COLON) {
		int rc;

		rc = operator_close();
		if (rc < 0) {
			return rc;
		}

		rc = close_vert_colon();
		if (rc < 0) {
			return rc;
		}
	} else {
		while (1) {
			if (last_list_is_operator()) {
				int rc = operator_close_verify();
				if (rc < 0) {
					return rc;
				}
			} else if (last_list->type != LOP_TYPE_LIST_COLON) {
				break;
			}
			last_list = last_list->parent;
			if (last_list == NULL) {
				return LOP_ERROR_LEXER_UNBALANCED;
			}
		}
	}

	if (last_list->type != t) {
		return LOP_ERROR_LEXER_UNBALANCED;
	}

	last_list = last_list->parent;
	if (last_list == NULL) {
		return LOP_ERROR_LEXER_ROOT_CLOSED;
	}
	last_token = last_list->list.tail;

	newline_was = 0;
	return 0;
}

static int l_newline()
{
	if (continue_was == 0) {
		int rc = operator_close();
		if (rc < 0) {
			return rc;
		}
		last_token = NULL;
	}

	indent = 0;
	set_newline();
	return 0;
}

static int l_comma()
{
	if (!last_list_is_operator() && last_token == NULL) {
		int rc;

		rc = push_token(create_token(LOP_TYPE_NIL));
		if (rc < 0) {
			return rc;
		}
	}

	last_token = NULL;
	return operator_close();
}

int yywrap() {
	return 1;
}

static int finish(void)
{
	if (last_token && last_token->type == LOP_TYPE_STRING) {
		last_loc = last_token->loc;
		return LOP_ERROR_LEXER_UNBALANCED;
	}

	while (last_list) {
		if (last_list_is_operator()) {
			int rc = operator_close_verify();
			if (rc < 0) {
				return rc;
			}
		} else if (last_list->type != LOP_TYPE_LIST_COLON) {
			last_loc = last_list->loc;
			return LOP_ERROR_LEXER_UNBALANCED;
		}
		last_list = last_list->parent;
	}

	return 0;
}

int LOP_getAST(struct LOP_ASTNode **root, const char *filename, const char *string, size_t len, struct LOP_OperatorTable *operator_table)
{
	enum Token t;
	int rc = 0;

	yylineno = 1;
	indent = -1;
	newline_was = 1;
	continue_was = 0;
	l_str_offset = 0;
	l_line_offset = 0;
	memset(&last_loc, 0, sizeof(last_loc));
	*root = last_list = create_token(LOP_TYPE_LIST_COLON);
	last_token = NULL;
	indent = 0;

	yy_scan_bytes(string, len);

	while (rc == 0 && (t = yylex())) {
		switch (t) {
		case L_DIGIT:
		case L_FLOAT:
		case L_BNUMBER:
			last_loc = get_loc();
			rc = l_push_token(LOP_TYPE_NUMBER);
			break;
		case L_ID:
			last_loc = get_loc();
			rc = l_push_token(LOP_TYPE_ID);
			break;
		case L_SQUOTE:
		case L_DQUOTE:
		case L_QQUOTE:
			last_loc = get_loc();
			rc = l_str_open();
			break;
		case L_STR_APPEND:
			last_loc = get_loc();
			rc = l_str_append();
			break;
		case L_STR_CONTINUE:
			break;
		case L_QUOTE_CLOSE:
			last_loc = get_loc();
			rc = l_str_close();
			break;
		case L_COMMENT:
			break;
		case L_OPERATOR:
			last_loc = get_loc();
			rc = l_operator(operator_table);
			break;
		case L_LIST_OPEN:
			last_loc = get_loc();
			rc = l_push_token(LOP_TYPE_LIST_ROUND);
			break;
		case L_CLIST_OPEN:
			last_loc = get_loc();
			rc = l_push_token(LOP_TYPE_LIST_CURLY);
			break;
		case L_BLIST_OPEN:
			last_loc = get_loc();
			rc = l_push_token(LOP_TYPE_LIST_SQUARE);
			break;
		case L_TLIST_OPEN:
			last_loc = get_loc();
			rc = l_push_token(LOP_TYPE_LIST_COLON);
			break;
		case L_LIST_CLOSE:
			last_loc = get_loc();
			rc = l_list_close(LOP_TYPE_LIST_ROUND);
			break;
		case L_CLIST_CLOSE:
			last_loc = get_loc();
			rc = l_list_close(LOP_TYPE_LIST_CURLY);
			break;
		case L_BLIST_CLOSE:
			last_loc = get_loc();
			rc = l_list_close(LOP_TYPE_LIST_SQUARE);
			break;
		case L_TLIST_CLOSE:
			last_loc = get_loc();
			rc = l_list_close(LOP_TYPE_LIST_COLON);
			break;
		case L_INDENT:
			if (newline_was) {
				indent++;
			}
			break;
		case L_NEWLINE:
			rc = l_newline();
			break;
		case L_CONTINUE:
			continue_was = 1;
			break;
		case L_COMMA:
			rc = l_comma();
			break;
		case L_WHITESPACE:
			break;
		case L_UNKNOWN:
			rc = LOP_ERROR_LEXER_UNKNOWN;
			break;
		default:
			assert(0);
		}

		l_str_offset += yyleng;
	}

	if (rc == 0) {
		rc = finish();
	}

	if (rc < 0) {
		l_report(rc, filename, string, len, last_loc);
	}

	return rc;
}
