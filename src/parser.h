#define PARSER_MAX_WORD 64

typedef struct parser_variable {
	char name[PARSER_MAX_WORD];
	RegexNode *head, *tail;
} ParserVariable;

typedef struct parser {
	FILE *fp;
	char *path;
	int c, prevc;
	ParserVariable *variables;
	size_t nVariables;
	int col, line;

	int indent;
	char word[PARSER_MAX_WORD];
	RegexGroup group;
} Parser;

int parser_init(Parser *parser, const char *path);
int parser_getc(Parser *parser);
int parser_ungetc(Parser *parser);
void parser_report(Parser *parser, const char *fmt, ...);
ParserVariable *parser_getvariable(Parser *parser, const char *name);
ParserVariable *parser_addvariable(Parser *parser, const ParserVariable *var);
void parser_cleanup(Parser *parser);

/**
 * This includes comments new lines and blank space.
 *
 * This sets the indent value to the indent of the current line.
 */
void parser_parsespace(Parser *parser);

/**
 * This includes letters (a-zA-Z_) and digits (0-9).
 *
 * The word is stored in word.
 * Note that a word has a maximum length.
 */
int parser_parseword(Parser *parser);

/**
 * This includes a regex group, here are examples:
 * a-z]
 * 0-9_]
 * abc-]
 * []
 * \]]
 * \\\n\r\t\0]
 *
 * Note that the first '[' is always missing. This function expects that
 * this character was already consumed by a getc call.
 *
 * This fills the group struct.
 */
int parser_parseregexgroup(Parser *parser);

int parser_run(Parser *parser);
