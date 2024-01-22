#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LOP.h>

struct Token {
	enum TokenType {
		TOKEN_EOF = -1,

		TOKEN_SYMBOL,

		TOKEN_COMMA,
		TOKEN_COMMIT,

		TOKEN_LIST_OPEN,
		TOKEN_LIST_CLOSE,
	} type;

	union {
		enum LOP_ListType list_type;
		struct SymbolToken {
			char *str;
			const char *buf;
			int buf_size;

			enum LOP_SymbolType type;
		} symbol;
	};

	struct LOP_Location loc;
};

struct State {
	const char *string;
	/* Current position inside the string */
	size_t c;
	/* strlen of the string */
	size_t end;

	struct Token token;
	struct LOP_Location loc;

	int indent;

	struct {
		enum {
			STRINGER_IDLE,
			STRINGER_GOBBLE,
			STRINGER_CLOSING,
		} state;
		int stop_indent;
	} stringer;

	LOP_error_cb_t error_cb;
};

static void dump_loc(struct LOP_Location *loc, const char *fmt, FILE *stream)
{
	fprintf(stream, fmt, loc->lineno, loc->charno);
}

static void dump_state(struct State *state)
{
	struct Token *t = &state->token;

	printf("Token type: ");
#define CASE_TOKEN_TYPE(type)	case type: printf(#type); break
	switch (t->type) {
		CASE_TOKEN_TYPE(TOKEN_EOF);
		CASE_TOKEN_TYPE(TOKEN_SYMBOL);
		CASE_TOKEN_TYPE(TOKEN_COMMA);
		CASE_TOKEN_TYPE(TOKEN_COMMIT);
		CASE_TOKEN_TYPE(TOKEN_LIST_OPEN);
		CASE_TOKEN_TYPE(TOKEN_LIST_CLOSE);
	}
#undef CASE_TOKEN_TYPE
	if (t->type == TOKEN_LIST_OPEN || t->type == TOKEN_LIST_CLOSE) {
		printf("/");
#define CASE_LIST_TYPE(type)	case type: printf(#type); break
		switch (t->list_type) {
			CASE_LIST_TYPE(LOP_LIST_TLIST);
			CASE_LIST_TYPE(LOP_LIST_LIST);
			CASE_LIST_TYPE(LOP_LIST_AREF);
			CASE_LIST_TYPE(LOP_LIST_STRUCT);
			CASE_LIST_TYPE(LOP_LIST_OPERATOR);
			CASE_LIST_TYPE(LOP_LIST_STRING);
		}
#undef CASE_LIST_TYPE
	} else if (t->type == TOKEN_SYMBOL) {
		printf("/");
#define CASE_SYMBOL_TYPE(type)	case type: printf(#type); break
		switch (t->symbol.type) {
			CASE_SYMBOL_TYPE(LOP_SYMBOL_IDENTIFIER);
			CASE_SYMBOL_TYPE(LOP_SYMBOL_NUMBER);
			CASE_SYMBOL_TYPE(LOP_SYMBOL_STRING);
			CASE_SYMBOL_TYPE(LOP_SYMBOL_OPERATOR);
		}
#undef CASE_LIST_TYPE
	}
	printf("\n");
	if (t->type == TOKEN_SYMBOL) {
		printf("\tValue: '");
		for (int i = 0; i < t->symbol.buf_size; i++) {
			printf("%c", t->symbol.buf[i]);
		}
		printf("'\n");
	}
	dump_loc(&t->loc, "\tLocation: %i:%i\n", stdout);
	printf("\tLevel: %i\n", state->indent);
}

static void token_set_type(struct State *state, enum TokenType type)
{
	struct Token *t = &state->token;

	t->type = type;
	t->loc = state->loc;
}

static void token_set_list_type(struct State *state, int c)
{
	struct Token *t = &state->token;

#define CASE_LIST_TYPE(lb, le, type)	case lb: case le: t->list_type = type; break
	switch (c) {
		CASE_LIST_TYPE(':', ';', LOP_LIST_TLIST);
		CASE_LIST_TYPE('(', ')', LOP_LIST_LIST);
		CASE_LIST_TYPE('[', ']', LOP_LIST_AREF);
		CASE_LIST_TYPE('{', '}', LOP_LIST_STRUCT);
		CASE_LIST_TYPE('\'', '"', LOP_LIST_STRING);
	}
#undef CASE_LIST_TYPE
}

static int cur_char(struct State *state)
{
	return state->string[state->c];
}

static int next_char(struct State *state)
{
	state->c++;
	state->loc.charno++;
	return cur_char(state);
}

static void reset_buf(struct State *state)
{
	struct Token *t = &state->token;

	t->symbol.str = NULL;
	t->symbol.buf = &state->string[state->c];
	t->symbol.buf_size = 0;
}

static void append_buf(struct State *state)
{
	struct SymbolToken *s = &state->token.symbol;

	s->buf_size++;
}

static int next_token(struct State *state)
{
	static const char *operator_mask = ".~!@#$%^&*+-=<>/?|";

	if (state->stringer.state == STRINGER_GOBBLE) {
		bool escape = false;
		char *res = calloc(1, 1);
		size_t len = 1;

		assert(res);

		token_set_type(state, TOKEN_SYMBOL);

		state->token.symbol.type = LOP_SYMBOL_STRING;

		reset_buf(state);

		while (cur_char(state) != 0) {
			if (!escape && (cur_char(state) == '"' || cur_char(state) == '\'')) {
				break;
			}

			if (cur_char(state) == '\n') {
				if (escape) {
					escape = false;
					state->token.symbol.buf_size--;
				} else {
					append_buf(state);
				}

				len += state->token.symbol.buf_size;
				res = realloc(res, len);
				assert(res);
				strncat(res, state->token.symbol.buf, state->token.symbol.buf_size);

				state->indent = 0;
				state->loc.lineno++;
				state->loc.charno = 0;

				next_char(state);

				while (cur_char(state) == '\t' && state->indent != state->stringer.stop_indent) {
					state->indent++;
					next_char(state);
				}

				reset_buf(state);

				if (cur_char(state) == '\n') {
					continue;
				}

				if (cur_char(state) == 0 || cur_char(state) == '"' || cur_char(state) == '\'') {
					break;
				}

				if (state->indent != state->stringer.stop_indent) {
					free(res);

					int rc = LOP_ERROR_TOKEN_BAD_INDENT;
					if (state->error_cb) {
						state->error_cb(rc,
							(union LOP_Error) {
								.token = {
									.str = state->string,
									.len = state->end,
									.loc = state->loc,
									.expindent = state->stringer.stop_indent,
									.actindent = state->indent
								}
							}
						);
					}
					return rc;
				}
			}

			escape = !escape && (cur_char(state) == '\\');
			append_buf(state);
			next_char(state);
		}

		len += state->token.symbol.buf_size;
		res = realloc(res, len);
		assert(res);
		strncat(res, state->token.symbol.buf, state->token.symbol.buf_size);
		reset_buf(state);

		state->token.symbol.str = res;

		state->stringer.state = STRINGER_CLOSING;
		return 0;
	}

	if (state->loc.charno == 1) {
		while (cur_char(state) == '\t') {
			state->indent++;
			next_char(state);
		}
	} else if (isspace(cur_char(state))) {
		while (cur_char(state) != 0 && cur_char(state) != '\n' && isspace(cur_char(state))) {
			next_char(state);
		}
	}

	if (cur_char(state) == '`') {
		while (cur_char(state) != 0 && cur_char(state) != '\n') {
			next_char(state);
		}
	}

	if (cur_char(state) == 0) {
		token_set_type(state, TOKEN_EOF);
	} else if (cur_char(state) == '\n') {
		token_set_type(state, TOKEN_COMMIT);

		state->indent = 0;
		state->loc.lineno++;
		state->loc.charno = 0;

		next_char(state);
	} else if (isalpha(cur_char(state)) || cur_char(state) == '_') {
		token_set_type(state, TOKEN_SYMBOL);

		state->token.symbol.type = LOP_SYMBOL_IDENTIFIER;

		reset_buf(state);

		append_buf(state);
		next_char(state);

		while (isalnum(cur_char(state)) || cur_char(state) == '_') {
			append_buf(state);
			next_char(state);
		}
	} else if (isdigit(cur_char(state))) {
		token_set_type(state, TOKEN_SYMBOL);

		state->token.symbol.type = LOP_SYMBOL_NUMBER;

		reset_buf(state);

		append_buf(state);
		next_char(state);

		while (isalnum(cur_char(state)) || cur_char(state) == '_' || cur_char(state) == '.') {
			append_buf(state);
			next_char(state);
		}
	} else if (strchr(operator_mask, cur_char(state))) {
		token_set_type(state, TOKEN_SYMBOL);

		state->token.symbol.type = LOP_SYMBOL_OPERATOR;

		reset_buf(state);

		append_buf(state);
		next_char(state);

		while (strchr(operator_mask, cur_char(state))) {
			append_buf(state);
			next_char(state);
		}
	} else {
		switch (cur_char(state)) {
		case '(':
		case '[':
		case '{':
		case ':':
			token_set_type(state, TOKEN_LIST_OPEN);
			token_set_list_type(state, cur_char(state));
			break;
		case ')':
		case ']':
		case '}':
		case ';':
			token_set_type(state, TOKEN_LIST_CLOSE);
			token_set_list_type(state, cur_char(state));
			break;
		case '"':
		case '\'':
			if (state->stringer.state == STRINGER_CLOSING) {
				token_set_type(state, TOKEN_LIST_CLOSE);
				state->stringer.state = STRINGER_IDLE;
			} else {
				token_set_type(state, TOKEN_LIST_OPEN);
				state->stringer.state = STRINGER_GOBBLE;
			}
			token_set_list_type(state, cur_char(state));
			break;
		case ',':
			token_set_type(state, TOKEN_COMMA);
			break;
		default:
			int rc = LOP_ERROR_TOKEN_UNKNOWN;
			if (state->error_cb) {
				state->error_cb(rc,
					(union LOP_Error) {
						.token = {
							.str = state->string,
							.len = state->end,
							.loc = state->loc,
						}
					}
				);
			}
			return rc;
		}

		next_char(state);
	}

	return 0;
}

static void dump_ast_recursive(struct LOP_ASTNode *root, int level, bool pretty)
{
	if (root->type == LOP_AST_LIST) {
		printf("(");
		if (pretty) {
#define CASE_LIST_TYPE(type, val)	case type: printf("#" val " "); break
			switch (root->list.type) {
				CASE_LIST_TYPE(LOP_LIST_TLIST, "tlist");
				CASE_LIST_TYPE(LOP_LIST_LIST, "list");
				CASE_LIST_TYPE(LOP_LIST_AREF, "aref");
				CASE_LIST_TYPE(LOP_LIST_STRUCT, "struct");
				CASE_LIST_TYPE(LOP_LIST_OPERATOR, "operator");
				CASE_LIST_TYPE(LOP_LIST_STRING, "string");
			}
#undef CASE_LIST_TYPE
#define CASE_LIST_OP(op, val)	case op: printf("#" val " "); break
			switch (root->list.op) {
				CASE_LIST_OP(LOP_LIST_NOP, "nop");
				CASE_LIST_OP(LOP_LIST_CALL, "call");
				CASE_LIST_OP(LOP_LIST_BINARY, "binary");
				CASE_LIST_OP(LOP_LIST_UNARY, "unary");
			}
#undef CASE_LIST_OP
		}
		for (struct LOP_ASTNode *iter = root->list.head; iter; iter = iter->next) {
			if (pretty) {
				printf("\n");
				for (int i = 0; i < level + 1; i++) {
					printf("\t");
				}
			}
			if (iter->type == LOP_AST_LIST) {
				dump_ast_recursive(iter, level + 1, pretty);
			} else {
				dump_ast_recursive(iter, level, pretty);
			}
			if (iter->next != NULL) {
				printf(" ");
			}
		}
		if (pretty) {
			printf("\n");
			for (int i = 0; i < level; i++) {
				printf("\t");
			}
		}
		printf(")");
	} else {
		if (root->symbol.type == LOP_SYMBOL_STRING) {
			printf("\"");
		}
		printf("%s", root->symbol.value);
		if (root->symbol.type == LOP_SYMBOL_STRING) {
			printf("\"");
		}
	}
}

void dump_ast(struct LOP_ASTNode *root, bool pretty)
{
	dump_ast_recursive(root, 0, pretty);
	printf("\n");
}

static void node_append(struct LOP_ASTNode *p, struct LOP_ASTNode *n)
{
	assert(n->parent == NULL);

	n->parent = p;
	if (n->type == LOP_AST_LIST) {
		n->list.indent = p->list.indent + 1;
	}
	n->prev = p->list.tail;

	if (p->list.count == 0) {
		p->list.head = p->list.tail = n;
	} else {
		p->list.tail->next = n;
		p->list.tail = n;
	}

	p->list.count++;
}

static struct LOP_ASTNode *node_remove_tail(struct LOP_ASTNode *p)
{
	struct LOP_ASTNode *ret = p->list.tail;

	if (p->list.count == 1) {
		p->list.head = p->list.tail = NULL;
	} else {
		p->list.tail = ret->prev;
		p->list.tail->next = NULL;
	}

	ret->prev = NULL;
	ret->parent = NULL;

	p->list.count--;

	return ret;
}

static struct LOP_ASTNode *new_node(struct LOP_ASTNode init)
{
	struct LOP_ASTNode *n = malloc(sizeof(*n));

	assert(n);
	*n = init;
	return n;
}

static struct LOP_ASTNode *new_node_symbol(struct Token *t)
{
	char *value = t->symbol.str ? t->symbol.str : strndup(t->symbol.buf, t->symbol.buf_size);

	assert(value);

	return new_node((struct LOP_ASTNode) {
		.type = LOP_AST_SYMBOL,
		.symbol = {
			.type = t->symbol.type,
			.value = value,
		},
		.loc = t->loc,
	});
}

static struct LOP_ASTNode *new_node_list(struct Token *t)
{
	return new_node((struct LOP_ASTNode) {
		.type = LOP_AST_LIST,
		.list = {
			.type = t->list_type,
		},
		.loc = t->loc,
	});
}

static int list_close(struct State *state, struct LOP_ASTNode **cptr, enum LOP_ListType type)
{
	assert(cptr);
	assert(*cptr);

	struct LOP_ASTNode *c = *cptr;
	struct LOP_ASTNode *p = c->parent;
	int rc = 0;

	if (p) {
		p->list.multiline |= c->list.multiline;
	}

	if ((c->list.type & type) == 0) {
		rc = LOP_ERROR_TOKEN_UNBALANCED;
		goto error_report;
	}

	if (c->list.type & LOP_LIST_OPERATOR) {
		if (c->list.op == LOP_LIST_BINARY) {
			if (c->list.count != 3) {
				rc = LOP_ERROR_TOKEN_BINARY_ARGS;
				goto error_report;
			}
		} else if (c->list.op == LOP_LIST_UNARY) {
			if (c->list.count != 2) {
				rc = LOP_ERROR_TOKEN_UNARY_ARGS;
				goto error_report;
			}
		} else {
			assert(0);
		}
	}

	if (p && c->list.type == LOP_LIST_STRING && c->list.op == LOP_LIST_NOP) {
		assert(c->list.count == 1);

		struct LOP_ASTNode *child = node_remove_tail(c);

		LOP_delAST(node_remove_tail(p));
		node_append(p, child);
	}

	*cptr = p;

	return 0;

error_report:
	if (state->error_cb) {
		state->error_cb(rc,
			(union LOP_Error) {
				.token = {
					.str = state->string,
					.len = state->end,
					.loc = c->loc
				}
			}
		);
	}
	return rc;
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

int LOP_getAST(struct LOP_ASTNode **root, const char *string, struct LOP_OperatorTable *unary, struct LOP_OperatorTable *binary, LOP_error_cb_t error_cb)
{
	struct State state = {
		.loc = {
			.charno = 1,
			.lineno = 1,
		},
		.string = string,
		.end = string ? strlen(string) : 0,
		.error_cb = error_cb,
	};
	struct Token *t = &state.token;
	*root = new_node_list(&(struct Token) {
		.list_type = LOP_LIST_TLIST,
		.loc = state.loc,
	});
	struct LOP_ASTNode *c = *root;
	bool new_line = true;
	bool no_symbol = false;
	bool comma = false;
	int rc = 0;
	union LOP_Error error;

	if (!string) {
		return 0;
	}

	while (true) {
		rc = next_token(&state);
		if (rc < 0) {
			goto error;
		}

		if (t->type == TOKEN_EOF) {
			while (c) {
				rc = list_close(&state, &c, LOP_LIST_TLIST | LOP_LIST_OPERATOR);
				if (rc < 0) {
					goto error;
				}
			}
			break;
		} else if (t->type == TOKEN_COMMIT) {
			c->list.multiline = true;

			new_line = true;
			no_symbol = false;
			comma = false;
			continue;
		} else if (t->type == TOKEN_COMMA) {
			while (c->list.type == LOP_LIST_OPERATOR) {
				rc = list_close(&state, &c, LOP_LIST_OPERATOR);
				if (rc < 0) {
					goto error;
				}
				assert(c);
			}

			new_line = false;
			no_symbol = false;
			comma = true;
			continue;
		}

		if (new_line) {
			if (state.indent < c->list.indent) {
				while ((c->list.type & (LOP_LIST_TLIST | LOP_LIST_OPERATOR)) && c->list.indent > state.indent + 1) {
					rc = list_close(&state, &c, LOP_LIST_TLIST | LOP_LIST_OPERATOR);
					if (rc < 0) {
						goto error;
					}
					assert(c);
				}

				if (!(t->type == TOKEN_LIST_CLOSE && t->list_type == LOP_LIST_TLIST) && c->list.type == LOP_LIST_TLIST) {
					rc = list_close(&state, &c, LOP_LIST_TLIST);
					if (rc < 0) {
						goto error;
					}
					assert(c);
				}

				while (c->list.type == LOP_LIST_OPERATOR) {
					if (c->list.op == LOP_LIST_UNARY && c->list.count == 2) {
						rc = list_close(&state, &c, LOP_LIST_OPERATOR);
						if (rc < 0) {
							goto error;
						}
						assert(c);
					} else if (c->list.op == LOP_LIST_BINARY && c->list.count == 3) {
						rc = list_close(&state, &c, LOP_LIST_OPERATOR);
						if (rc < 0) {
							goto error;
						}
						assert(c);
					} else {
						break;
					}
				}
			} else if (state.indent > c->list.indent) {
				rc = LOP_ERROR_TOKEN_BAD_INDENT;
				error = (union LOP_Error) {
					.token = {
						.str = state.string,
						.len = state.end,
						.loc = state.loc,
						.expindent = c->list.indent,
						.actindent = state.indent
					}
				};
				goto error_report;
			}
		}

		if (t->type == TOKEN_SYMBOL) {
			if (new_line && state.indent != c->list.indent) {
				rc = LOP_ERROR_TOKEN_BAD_INDENT;
				error = (union LOP_Error) {
					.token = {
						.str = state.string,
						.len = state.end,
						.loc = state.loc,
						.expindent = c->list.indent,
						.actindent = state.indent
					}
				};
				goto error_report;
			}

			if (t->symbol.type != LOP_SYMBOL_OPERATOR && no_symbol) {
				rc = LOP_ERROR_TOKEN_SEPARATOR;
				error = (union LOP_Error) {
					.token = {
						.str = state.string,
						.len = state.end,
						.loc = state.loc
					}
				};
				goto error_report;
			}

			if (t->symbol.type == LOP_SYMBOL_OPERATOR) {
				struct LOP_ASTNode *n = new_node_symbol(t);
				struct SymbolToken *s = &t->symbol;

				while (1) {
					int len = s->buf_size;

					struct LOP_ASTNode *nc = new_node_list(&(struct Token) { .list_type = LOP_LIST_OPERATOR, .loc = t->loc });

					if ((new_line || comma) || !c->list.tail || (c->list.tail->type == LOP_AST_SYMBOL && c->list.tail->symbol.type == LOP_SYMBOL_OPERATOR)) {
						struct LOP_OperatorTable *ot;

						while (1) {
							ot = ot_find(unary, n->symbol.value);
							if (ot) {
								break;
							}

							if (len > 1) {
								len--;
								n->symbol.value[len] = 0;
								continue;
							}

							rc = LOP_ERROR_TOKEN_UNARY_UNKNOWN;
							error = (union LOP_Error) {
								.token = {
									.str = state.string,
									.len = state.end,
									.loc = state.loc,
									.value = strdup(LOP_symbol_value(n)),
								}
							};
							LOP_delAST(n);
							LOP_delAST(nc);
							goto error_report;
						}

						nc->list.op = LOP_LIST_UNARY;
						nc->list.prio = ot->prio;

						node_append(nc, n);
						node_append(c, nc);
					} else {
						struct LOP_OperatorTable *ot;
						struct LOP_ASTNode *lhs;

						while (1) {
							ot = ot_find(binary, n->symbol.value);
							if (ot) {
								break;
							}

							if (len > 1) {
								len--;
								n->symbol.value[len] = 0;
								continue;;
							}

							rc = LOP_ERROR_TOKEN_BINARY_UNKNOWN;
							error = (union LOP_Error) {
								.token = {
									.str = state.string,
									.len = state.end,
									.loc = state.loc,
									.value = strdup(LOP_symbol_value(n)),
								}
							};
							LOP_delAST(n);
							LOP_delAST(nc);
							goto error_report;
						}

						while (c->list.type == LOP_LIST_OPERATOR) {
							if (c->list.prio > ot->prio) {
								break;
							} else if (c->list.prio == ot->prio) {
								if (ot->type == LOP_OPERATOR_RIGHT) {
									break;
								}
							}

							rc = list_close(&state, &c, LOP_LIST_OPERATOR);
							if (rc < 0) {
								goto error;
							}
							assert(c);
						}

						lhs = node_remove_tail(c);

						nc->list.op = LOP_LIST_BINARY;
						nc->list.prio = ot->prio;

						node_append(nc, n);
						node_append(nc, lhs);
						node_append(c, nc);
					}

					if (c->list.type == LOP_LIST_OPERATOR) {
						nc->list.indent = c->list.indent;
					}

					c = nc;

					no_symbol = false;
					comma = true;

					if (len == s->buf_size) {
						break;
					}

					s->buf += len;
					s->buf_size -= len;
					t->loc.charno += len - 1;
					n = new_node_symbol(t);
				}
			} else {
				node_append(c, new_node_symbol(t));
				no_symbol = true;
				comma = false;
			}

			new_line = false;
		} else if (t->type == TOKEN_LIST_OPEN) {
			if (new_line && state.indent != c->list.indent) {
				rc = LOP_ERROR_TOKEN_BAD_INDENT;
				error = (union LOP_Error) {
					.token = {
						.str = state.string,
						.len = state.end,
						.loc = state.loc,
						.expindent = c->list.indent,
						.actindent = state.indent
					}
				};
				goto error_report;
			}

			struct LOP_ASTNode *nc = new_node_list(t);

			if (c->list.tail && !(new_line || comma)) {
				while (c->list.type == LOP_LIST_OPERATOR && c->list.prio == 0) {
					rc = list_close(&state, &c, LOP_LIST_OPERATOR);
					if (rc < 0) {
						goto error;
					}
					assert(c);
				}

				struct LOP_ASTNode *op = node_remove_tail(c);

				nc->list.op = LOP_LIST_CALL;
				node_append(nc, op);

				comma = true;
			} else {
				comma = false;
			}

			node_append(c, nc);

			c = nc;

			if (state.stringer.state) {
				state.stringer.stop_indent = c->list.indent;
			}

			new_line = false;
			no_symbol = false;
		} else if (t->type == TOKEN_LIST_CLOSE) {
			while (c->list.type & (LOP_LIST_TLIST | LOP_LIST_OPERATOR)) {
				if (t->list_type == LOP_LIST_TLIST && c->list.type == LOP_LIST_TLIST && (!c->list.multiline || state.indent == c->list.indent - 1)) {
					break;
				}
				rc = list_close(&state, &c, LOP_LIST_TLIST | LOP_LIST_OPERATOR);
				if (rc < 0) {
					goto error;
				}
				if (!c) {
					rc = LOP_ERROR_TOKEN_UNBALANCED;
					error = (union LOP_Error) {
						.token = {
							.str = state.string,
							.len = state.end,
							.loc = state.loc
						}
					};
					goto error_report;
				}
			}

			if (c->list.multiline && state.indent != c->list.indent - 1) {
				rc = LOP_ERROR_TOKEN_BAD_INDENT_CLOSE;
				error = (union LOP_Error) {
					.token = {
						.str = state.string,
						.len = state.end,
						.loc = state.loc,
						.expindent = c->list.indent - 1,
						.actindent = state.indent
					}
				};
				goto error_report;
			}

			rc = list_close(&state, &c, t->list_type);
			if (rc < 0) {
				goto error;
			}
			assert(c);

			new_line = false;
			no_symbol = true;
			comma = false;
		} else {
			assert(0);
		}
	}

	return rc;
error_report:
	if (state.error_cb) {
		state.error_cb(rc, error);
	}
error:
	LOP_delAST(*root);
	*root = NULL;
	return rc;
}

void LOP_delAST(struct LOP_ASTNode *root)
{
	if (root->type == LOP_AST_LIST) {
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

const char *LOP_symbol_value(struct LOP_ASTNode *n)
{
	assert(n->type == LOP_AST_SYMBOL);

	return n->symbol.value;
}

struct LOP_ASTNode *LOP_list_head(struct LOP_ASTNode *n)
{
	assert(n->type == LOP_AST_LIST);

	return n->list.head;
}

struct LOP_ASTNode *LOP_list_tail(struct LOP_ASTNode *n)
{
	assert(n->type == LOP_AST_LIST);

	return n->list.tail;
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

	assert(i < len);

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
