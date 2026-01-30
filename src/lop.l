%{
int charno = 1;

#define YY_USER_ACTION charno += yyleng;

enum Token {
	L_DIGIT = 1,
	L_FLOAT,
	L_BNUMBER,
	L_ID,
	L_SQUOTE,
	L_DQUOTE,
	L_STR_APPEND,
	L_STR_CONTINUE,
	L_QUOTE_CLOSE,
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
COMMENT `.+

INDENT "\t"
NEWLINE "\n"
CONTINUE "\\"
WHITESPACE [ ]+

%x str_sq str_dq

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

<str_sq>[^\\']+ {
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

<str_dq>[^\\"]+ {
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

{COMMENT} {
	/* Skip it */
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
	/* eat up whitespace */
}

[\\]. {
	printf("ERROR\n");
}

. printf("Unrecognized character: %s\n", yytext);

%%
