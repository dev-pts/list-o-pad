#include <LOP.h>
#include "FileMap.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

int main(int argc, char *argv[])
{
	struct LOP_Operator op[] = {
		{ ".", 0, LOP_OPERATOR_UNARY },
		{ "$", 0, LOP_OPERATOR_UNARY },
		{ "@", 0, LOP_OPERATOR_UNARY },

		{ ".", 0, LOP_OPERATOR_LTR },
		{ "->", 0, LOP_OPERATOR_LTR },
		{ "++", 0, LOP_OPERATOR_LTR },
		{ "--", 0, LOP_OPERATOR_LTR },

		{ "**", 1, LOP_OPERATOR_LTR },

		{ "++", 2, LOP_OPERATOR_UNARY },
		{ "--", 2, LOP_OPERATOR_UNARY },
		{ "!", 2, LOP_OPERATOR_UNARY },
		{ "~", 2, LOP_OPERATOR_UNARY },
		{ "+", 2, LOP_OPERATOR_UNARY },
		{ "-", 2, LOP_OPERATOR_UNARY },
		{ "*", 2, LOP_OPERATOR_UNARY },
		{ "/", 2, LOP_OPERATOR_UNARY },
		{ "&", 2, LOP_OPERATOR_UNARY },
		{ "|", 2, LOP_OPERATOR_UNARY },
		{ "^", 2, LOP_OPERATOR_UNARY },

		{ "*", 3, LOP_OPERATOR_LTR },
		{ "/", 3, LOP_OPERATOR_LTR },
		{ "%", 3, LOP_OPERATOR_LTR },

		{ "+", 4, LOP_OPERATOR_LTR },
		{ "-", 4, LOP_OPERATOR_LTR },

		{ "<<", 5, LOP_OPERATOR_LTR },
		{ ">>", 5, LOP_OPERATOR_LTR },

		{ "<", 6, LOP_OPERATOR_LTR },
		{ ">", 6, LOP_OPERATOR_LTR },
		{ "<=", 6, LOP_OPERATOR_LTR },
		{ ">=", 6, LOP_OPERATOR_LTR },

		{ "==", 7, LOP_OPERATOR_LTR },
		{ "!=", 7, LOP_OPERATOR_LTR },

		{ "&", 8, LOP_OPERATOR_LTR },
		{ "^", 9, LOP_OPERATOR_LTR },
		{ "|", 10, LOP_OPERATOR_LTR },
		{ "&&", 11, LOP_OPERATOR_LTR },
		{ "||", 12, LOP_OPERATOR_LTR },

		{ "=", 13, LOP_OPERATOR_RTL },
		{ "+=", 13, LOP_OPERATOR_RTL },
		{ "-=", 13, LOP_OPERATOR_RTL },
		{ "*=", 13, LOP_OPERATOR_RTL },
		{ "/=", 13, LOP_OPERATOR_RTL },
		{ "%=", 13, LOP_OPERATOR_RTL },
		{ ">>=", 13, LOP_OPERATOR_RTL },
		{ "<<=", 13, LOP_OPERATOR_RTL },
		{ "~=", 13, LOP_OPERATOR_RTL },
		{ "&=", 13, LOP_OPERATOR_RTL },
		{ "|=", 13, LOP_OPERATOR_RTL },
		{ "^=", 13, LOP_OPERATOR_RTL },

		{ "..", 14, LOP_OPERATOR_RTL },
		{ "+..", 14, LOP_OPERATOR_RTL },
		{ "-..", 14, LOP_OPERATOR_RTL },
	};
	struct LOP_OperatorTable ot = {
		.size = ARRAY_SIZE(op),
		.data = op,
	};
	struct FileMap source;
	struct LOP_ASTNode *ast;
	int rc = 0;

	source = map_file(argv[1]);

	rc = LOP_getAST(&ast, argv[1], source.data, source.len, &ot);
	LOP_dump_ast(ast, true);
	LOP_delAST(ast);

	unmap_file(source);
	return rc;
}
