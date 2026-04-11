#ifndef LOP_H
#define LOP_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

struct LOP_Location {
	int lineno;
	int charno;
	size_t line_offset;
};

struct LOP_ASTNode {
	enum LOP_ASTNodeType {
		/* () */
		LOP_TYPE_LIST_ROUND,
		/* {} */
		LOP_TYPE_LIST_CURLY,
		/* [] */
		LOP_TYPE_LIST_SQUARE,
		/* :; */
		LOP_TYPE_LIST_COLON,
		/* xxx"", xxx'' */
		LOP_TYPE_LIST_STRING,
		/* Convenient indicator */
		LOP_TYPE_LIST_LAST_CALLABLE,
		/* Virtual list for operators */
		LOP_TYPE_LIST_OPERATOR_UNARY,
		LOP_TYPE_LIST_OPERATOR_BINARY,
		/* Convenient indicator */
		LOP_TYPE_LIST_LAST,

		LOP_TYPE_OPERATOR,
		LOP_TYPE_ID,
		LOP_TYPE_NUMBER,
		LOP_TYPE_STRING,
		LOP_TYPE_NIL,
	} type;

	struct LOP_ASTNode *parent;
	struct LOP_ASTNode *next;

	/* Needed for closing colon lists */
	int indent;

	union {
		struct {
			char *value;
		} symbol;

		struct {
			struct LOP_ASTNode *head;
			struct LOP_ASTNode *tail;
			/* a(b) vs (a, b) */
			int call;
			/* For operators which are special type of lists */
			int prio;
		} list;
	};

	struct LOP_Location loc;
};

struct LOP_Operator {
	const char *value;
	int prio;

	unsigned type;
#define LOP_OPERATOR_UNARY (1 << 0)
/* Left-To-Right: a.b.c => (a.b).c */
#define LOP_OPERATOR_LTR (1 << 1)
/* Right-To-Left: a.b.c => a.(b.c) */
#define LOP_OPERATOR_RTL (1 << 2)
#define LOP_OPERATOR_BINARY_MASK (LOP_OPERATOR_LTR | LOP_OPERATOR_RTL)
};

struct LOP_OperatorTable {
	struct LOP_Operator *data;
	int size;
};

struct LOP_Handler {
	const char *key;
	struct LOP_ASTNode *n;
	int delta;
};

struct LOP_HandlerList {
	struct LOP_Handler *handler;
	int count;
};

struct LOP_Schema {
	/* You must fill these */
	const char *filename;

	/* LOP will fill these */
	struct KV *kv;
	struct LOP_OperatorTable operator_table;
};

struct LOP {
	/* You must fill these */
	struct LOP_Schema *schema;
	const char *top_rule_name;
	const char *filename;

	/* LOP will fill these */
	struct LOP_ASTNode *ast;
	struct LOP_HandlerList hl;
};

typedef int (*LOP_resolve_t)(struct LOP *lop, struct LOP_Handler *handler);

enum LOP_ErrorType {
	LOP_ERROR_LEXER_UNKNOWN = INT_MIN,
	LOP_ERROR_LEXER_UNBALANCED,
	LOP_ERROR_LEXER_ROOT_CLOSED,
	LOP_ERROR_LEXER_ROOT_CLOSED_BY_INDENT,
	LOP_ERROR_LEXER_SEPARATOR,
	LOP_ERROR_LEXER_UNARY_ARGS,
	LOP_ERROR_LEXER_UNARY_UNKNOWN,
	LOP_ERROR_LEXER_BINARY_ARGS,
	LOP_ERROR_LEXER_BINARY_UNKNOWN,
	LOP_ERROR_LEXER_OUT_OF_MEMORY,

	LOP_ERROR_SCHEMA_SYNTAX,
	LOP_ERROR_SCHEMA_MISSING_RULE,
	LOP_ERROR_SCHEMA_MISSING_TOP,
};

/* AST functions */
int LOP_getAST(struct LOP_ASTNode **root, const char *filename, const char *string, size_t len,
	struct LOP_OperatorTable *operator_table);
void LOP_delAST(struct LOP_ASTNode *root);

void LOP_dump_ast(struct LOP_ASTNode *root);

const char *LOP_symbol_value(struct LOP_ASTNode *n);
struct LOP_ASTNode *LOP_list_head(struct LOP_ASTNode *n);
struct LOP_ASTNode *LOP_list_tail(struct LOP_ASTNode *n);

/* Schema functions */
int LOP_schema_init(struct LOP_Schema *schema, const char *src, size_t len);
void LOP_schema_deinit(struct LOP_Schema *schema);

int LOP_init(struct LOP *lop, const char *src, size_t len);
void LOP_deinit(struct LOP *lop);

#endif // LOP_H
