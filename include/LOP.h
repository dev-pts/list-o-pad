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
typedef int (*LOP_resolve_t)(struct LOP *lop, const char *key, struct LOP_CB *cb);
typedef void (*LOP_cb_dtor_t)(struct LOP_CB *cb);

typedef void (*LOP_error_cb_t)(enum LOP_ErrorType type, union LOP_Error error);

enum LOP_ListType {
	// :;
	LOP_LIST_TLIST = 1 << 0,
	// ()
	LOP_LIST_LIST = 1 << 1,
	// []
	LOP_LIST_AREF = 1 << 2,
	// {}
	LOP_LIST_STRUCT = 1 << 3,
	// implicit list for 'a`op`b' and '`op`c'
	LOP_LIST_OPERATOR = 1 << 4,
	// '', ""
	LOP_LIST_STRING = 1 << 5,
};

enum LOP_ListOp {
	LOP_LIST_NOP = 0,
	LOP_LIST_CALL,
	LOP_LIST_BINARY,
	LOP_LIST_UNARY,
};

enum LOP_SymbolType {
	// starts with letter and until stop
	LOP_SYMBOL_IDENTIFIER,
	// starts with number and until stop
	LOP_SYMBOL_NUMBER,
	// starts and ends with quotes " or '
	LOP_SYMBOL_STRING,
	// non-letters, numbers and punctuations
	LOP_SYMBOL_OPERATOR,
};

struct LOP_Location {
	int lineno;
	int charno;
};

struct LOP_ASTNode {
	enum LOP_ASTNodeType {
		LOP_AST_SYMBOL,
		LOP_AST_LIST,
	} type;

	struct LOP_ASTNode *parent;
	struct LOP_ASTNode *prev;
	struct LOP_ASTNode *next;

	union {
		struct LOP_ASTSymbol {
			enum LOP_SymbolType type;
			char *value;
		} symbol;

		struct LOP_ASTList {
			enum LOP_ListType type;
			enum LOP_ListOp op;
			struct LOP_ASTNode *head;
			struct LOP_ASTNode *tail;
			int count;
			int indent;
			bool multiline;
			int prio;
		} list;
	};

	struct LOP_Location loc;
};

struct LOP_OperatorTable {
	const char *value;
	int prio;
	enum LOP_OperatorType {
		LOP_OPERATOR_LEFT,
		LOP_OPERATOR_RIGHT,
	} type;
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

struct LOP_Handler {
	struct SchemaNode *sn;
	struct LOP_ASTNode *n;
	struct LOP_HandlerList {
		struct LOP_Handler *handler;
		int count;
	} hl;
};

struct LOP {
	struct KV *kv;
	struct LOP_OperatorTable *unary;
	struct LOP_OperatorTable *binary;

	LOP_resolve_t resolve;
	LOP_error_cb_t error_cb;
};

/* AST functions */
int LOP_getAST(struct LOP_ASTNode **root, const char *string, size_t len,
	struct LOP_OperatorTable *unary, struct LOP_OperatorTable *binary,
	LOP_error_cb_t error_cb);
void LOP_delAST(struct LOP_ASTNode *root);

void LOP_dump_ast(struct LOP_ASTNode *root, bool pretty);

const char *LOP_symbol_value(struct LOP_ASTNode *n);
struct LOP_ASTNode *LOP_list_head(struct LOP_ASTNode *n);
struct LOP_ASTNode *LOP_list_tail(struct LOP_ASTNode *n);

/* Schema functions */
int LOP_handler_eval(struct LOP_HandlerList hl, unsigned child, void *param);
bool LOP_handler_evalable(struct LOP_HandlerList hl, unsigned child);

int LOP_schema_init(struct LOP *lop, const char *user_schema, size_t len);
int LOP_schema_parse_source(void *ctx, struct LOP *lop, const char *string, size_t len, const char *key);
void LOP_schema_deinit(struct LOP *lop);

/* Standard callbacks */
void LOP_default_error_cb(enum LOP_ErrorType type, union LOP_Error error);
int LOP_cb_default(struct LOP_HandlerList hl, struct LOP_ASTNode *n, void *param, void *cb_arg);

#endif // LOP_H
