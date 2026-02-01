#ifndef LOP_H
#define LOP_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

struct LOP_ASTNode;
struct LOP;
struct LOP_HandlerList;
struct LOP_CB;

enum LOP_ErrorType;
union LOP_Error;

typedef int (*LOP_handler_t)(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg);
typedef int (*LOP_resolve_t)(struct LOP *lop, const char *handler_name, struct LOP_CB *cb);
typedef void (*LOP_cb_dtor_t)(struct LOP_CB *cb);

typedef void (*LOP_error_cb_t)(enum LOP_ErrorType type, union LOP_Error error);

struct LOP_Location {
	int lineno;
	int charno;
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
		/* virtual list for operators */
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

#define LOP_OPERATOR_UNARY (1 << 0)
/* Left-To-Right: a.b.c => (a.b).c */
#define LOP_OPERATOR_LTR (1 << 1)
/* Right-To-Left: a.b.c => a.(b.c) */
#define LOP_OPERATOR_RTL (1 << 2)
#define LOP_OPERATOR_BINARY_MASK (LOP_OPERATOR_LTR | LOP_OPERATOR_RTL)
	unsigned type;
};

struct LOP_OperatorTable {
	struct LOP_Operator *data;
	int size;
};

enum LOP_ErrorType {
	LOP_ERROR_TOKEN_UNKNOWN = INT_MIN,
	LOP_ERROR_TOKEN_BAD_INDENT,
	LOP_ERROR_TOKEN_BAD_INDENT_CLOSE,
	LOP_ERROR_TOKEN_UNBALANCED,
	LOP_ERROR_TOKEN_SEPARATOR,
	LOP_ERROR_TOKEN_UNARY_ARGS,
	LOP_ERROR_TOKEN_UNARY_UNKNOWN,
	LOP_ERROR_TOKEN_BINARY_ARGS,
	LOP_ERROR_TOKEN_BINARY_UNKNOWN,

	LOP_ERROR_SCHEMA_SYNTAX,
	LOP_ERROR_SCHEMA_MISSING_RULE,
	LOP_ERROR_SCHEMA_MISSING_HANDLER,
	LOP_ERROR_SCHEMA_MISSING_TOP,
};

union LOP_Error {
	struct {
		struct LOP_ASTNode *node;
		const char *src;
	} syntax;
	const char *str;
	struct {
		const char *str;
		size_t len;
		struct LOP_Location loc;
		int expindent;
		int actindent;
		const char *value;
	} token;
};

struct LOP_CB {
	LOP_handler_t func;
	void *arg;
	LOP_cb_dtor_t dtor;
};

struct LOP_HandlerList {
	struct LOP_Handler *handler;
	int count;
};

struct LOP_Handler {
	struct SchemaNode *sn;
	struct LOP_ASTNode *n;
	struct LOP_HandlerList hl;
};

struct LOP {
	struct KV *kv;
	struct LOP_OperatorTable operator_table;

	LOP_resolve_t resolve;
	LOP_error_cb_t error_cb;
};

/* AST functions */
int LOP_getAST(struct LOP_ASTNode **root, const char *string, size_t len,
	struct LOP_OperatorTable *operator_table, LOP_error_cb_t error_cb);
void LOP_delAST(struct LOP_ASTNode *root);

void LOP_dump_ast(struct LOP_ASTNode *root, bool pretty);

const char *LOP_symbol_value(struct LOP_ASTNode *n);
struct LOP_ASTNode *LOP_list_head(struct LOP_ASTNode *n);
struct LOP_ASTNode *LOP_list_tail(struct LOP_ASTNode *n);

/* Schema functions */
int LOP_handler_eval(struct LOP_HandlerList hl, unsigned child, void *param);
bool LOP_handler_evalable(struct LOP_HandlerList hl, unsigned child);

int LOP_schema_init(struct LOP *lop, const char *user_schema, size_t len);
int LOP_schema_parse_source(void *ctx, struct LOP *lop, const char *string, size_t len, const char *top_rule_name);
void LOP_schema_deinit(struct LOP *lop);

/* Standard callbacks */
void LOP_default_error_cb(enum LOP_ErrorType type, union LOP_Error error);
int LOP_cb_default(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg);

#endif // LOP_H
