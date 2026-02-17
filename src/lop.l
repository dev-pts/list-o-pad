%{

enum Token {
	L_DIGIT = 1,
	L_FLOAT,
	L_BNUMBER,
	L_ID,
	L_SQUOTE,
	L_DQUOTE,
	L_QQUOTE,
	L_STR_APPEND,
	L_STR_CONTINUE,
	L_QUOTE_CLOSE,
	L_COMMENT,
	L_OPERATOR,
	L_LIST_OPEN,
	L_LIST_CLOSE,
	L_CLIST_OPEN,
	L_CLIST_CLOSE,
	L_BLIST_OPEN,
	L_BLIST_CLOSE,
	L_TLIST_OPEN,
	L_TLIST_CLOSE,
	L_INDENT,
	L_NEWLINE,
	L_CONTINUE,
	L_COMMA,
	L_WHITESPACE,
	L_UNKNOWN,
};

%}

%option nounput noinput
%option yylineno
%option caseless

DIGIT [0-9][_0-9]*
NUMBER [0-9a-f_]
ID [_a-z][_a-z0-9]*
OPERATOR [.~!@#$%^&*+\-=<>/?|]
BASE [bdxo]

LIST_OPEN "("
LIST_CLOSE ")"
CLIST_OPEN "{"
CLIST_CLOSE "}"
BLIST_OPEN "["
BLIST_CLOSE "]"
TLIST_OPEN ":"
TLIST_CLOSE ";"

COMMA ","

DQUOTE "\""
SQUOTE "'"
QQUOTE "`"
COMMENT \\\\.*

INDENT "\t"
NEWLINE "\n"
CONTINUE \\
WHITESPACE [ ]+

%x str_sq str_dq str_qq

%%

{DIGIT}+ {
	return L_DIGIT;
}

{DIGIT}+"."{DIGIT}* {
	return L_FLOAT;
}

{DIGIT}+{BASE}{NUMBER}+ {
	return L_BNUMBER;
}

{ID} {
	return L_ID;
}

{SQUOTE} {
	BEGIN(str_sq);
	return L_SQUOTE;
}

<str_sq>[^\\'\n]*\n? {
	return L_STR_APPEND;
}

<str_sq>\\\n {
	return L_STR_CONTINUE;
}

<str_sq>\\. {
	return L_STR_APPEND;
}

<str_sq>{SQUOTE} {
	BEGIN(INITIAL);
	return L_QUOTE_CLOSE;
}

{DQUOTE} {
	BEGIN(str_dq);
	return L_DQUOTE;
}

<str_dq>[^\\"\n]*\n? {
	return L_STR_APPEND;
}

<str_dq>\\\n {
	return L_STR_CONTINUE;
}

<str_dq>\\. {
	return L_STR_APPEND;
}

<str_dq>{DQUOTE} {
	BEGIN(INITIAL);
	return L_QUOTE_CLOSE;
}

{QQUOTE} {
	BEGIN(str_qq);
	return L_QQUOTE;
}

<str_qq>[^\\`\n]*\n? {
	return L_STR_APPEND;
}

<str_qq>\\\n {
	return L_STR_CONTINUE;
}

<str_qq>\\. {
	return L_STR_APPEND;
}

<str_qq>{QQUOTE} {
	BEGIN(INITIAL);
	return L_QUOTE_CLOSE;
}

{COMMENT} {
	return L_COMMENT;
}

{OPERATOR}+ {
	return L_OPERATOR;
}

{LIST_OPEN} {
	return L_LIST_OPEN;
}

{LIST_CLOSE} {
	return L_LIST_CLOSE;
}

{CLIST_OPEN} {
	return L_CLIST_OPEN;
}

{CLIST_CLOSE} {
	return L_CLIST_CLOSE;
}

{BLIST_OPEN} {
	return L_BLIST_OPEN;
}

{BLIST_CLOSE} {
	return L_BLIST_CLOSE;
}

{TLIST_OPEN} {
	return L_TLIST_OPEN;
}

{TLIST_CLOSE} {
	return L_TLIST_CLOSE;
}

{INDENT} {
	return L_INDENT;
}

{NEWLINE} {
	return L_NEWLINE;
}

{CONTINUE} {
	return L_CONTINUE;
}

{COMMA} {
	return L_COMMA;
}

{WHITESPACE} {
	return L_WHITESPACE;
}

. {
	return L_UNKNOWN;
}

%%
