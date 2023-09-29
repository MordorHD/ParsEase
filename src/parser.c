#include "parsease.h"

int parser_init(Parser *parser, const char *path)
{
	FILE *fp;

	fp = fopen(path, "r");
	if (fp == NULL) {
		fprintf(stderr, "could not open file '%s': %s\n",
				path, strerror(errno));
		return -1;
	}
	memset(parser, 0, sizeof(*parser));
	parser->fp = fp;
	parser->path = strdup(path);
	parser->c = fgetc(fp);
	return 0;
}

int parser_getc(Parser *parser)
{
	const int c = fgetc(parser->fp);
	if (c != EOF) {
		if (c == '\n') {
			parser->col = 0;
			parser->line++;
		} else {
			parser->col++;
		}
		parser->prevc = parser->c;
	}
	return parser->c = c;
}

ParserVariable *parser_getvariable(Parser *parser, const char *name)
{
	for (size_t i = 0; i < parser->nVariables; i++)
		if (!strcmp(parser->variables[i].name, name))
			return parser->variables + i;
	return NULL;
}

ParserVariable *parser_addvariable(Parser *parser, const ParserVariable *var)
{
	ParserVariable *newVars;

	if ((newVars = parser_getvariable(parser, var->name)) != NULL)
		parser_report(parser, "overwriting existing variable '%s'\n",
				var->name);
	if (newVars == NULL) {
		newVars = realloc(parser->variables,
				sizeof(*parser->variables) *
				(parser->nVariables + 1));
		if (newVars == NULL) {
			parser_report(parser, "ran out of memory trying to "
					"add variable '%s'\n", var->name);
			return NULL;
		}
		parser->variables = newVars;
		newVars += parser->nVariables;
		parser->nVariables++;
	}
	*newVars = *var;
	return newVars;
}

int parser_ungetc(Parser *parser)
{
	ungetc(parser->c, parser->fp);
	return parser->c = parser->prevc;
}

void parser_report(Parser *parser, const char *fmt, ...)
{
	va_list l;
	int line;

	line = parser->line;
	if (parser->c != '\n')
		line++;
	fprintf(stderr, "%s:%d:%d: ", parser->path, line, parser->col);

	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);
}

void parser_cleanup(Parser *parser)
{
	fclose(parser->fp);
	free(parser->path);
}

void parser_parsespace(Parser *parser)
{
	size_t indent;
	bool elig = false;

again:
	indent = 0;
	while (isblank(parser->c)) {
		indent += parser->c == ' ' ? 1 : 2 - indent % 2;
		parser_getc(parser);
	}
	if (parser->c == '\n') {
		elig = true;
		parser_getc(parser);
		goto again;
	}
	if (parser->c == '#') {
		while (parser->c != '\n' && parser->c != EOF)
			parser_getc(parser);
		parser_getc(parser);
		goto again;
	}
	if (elig) {
		if (indent % 2 == 1)
			parser_report(parser, "inconsistent indentation\n");
		indent /= 2;
		parser->indent = indent;
	} else {
		parser->indent = -1;
	}
}

int parser_parseword(Parser *parser)
{
	size_t nWord = 0;

	while  (isalpha(parser->c)) {
		if  (nWord + 1 == PARSER_MAX_WORD) {
			parser_report(parser,
				"variable name exceeds %u bytes\n",
				PARSER_MAX_WORD);
			return -1;
		}
		parser->word[nWord++] = parser->c;
		parser_getc(parser);
	}
	parser->word[nWord] = 0;
	return 0;
}

int parser_parseregexgroup(Parser *parser)
{
	int pt = -1, t = -1;
	bool invert;

	memset(&parser->group, 0, sizeof(parser->group));
	invert = parser->c == '^';
	if (invert && parser_getc(parser) == ']') {
		TSET(parser->group.tests, '^');
		return 0;
	}
	while (parser->c != ']') {
		if (parser->c == EOF) {
			parser_report(parser, "'[' expects a closing bracket ']'\n");
			return -1;
		}
		if (parser->c == '\\') {
			switch (parser_getc(parser)) {
			case '0': t = '\0'; break;
			case 'a': t = '\a'; break;
			case 'b': t = '\b'; break;
			case 'f': t = '\f'; break;
			case 'n': t = '\n'; break;
			case 'r': t = '\r'; break;
			case 't': t = '\t'; break;
			case 'v': t = '\v'; break;
			default:
				t = parser->c;
			}
			parser_getc(parser);
		} else if (parser->c == '-') {
			parser_getc(parser);
			if (t != -1 && parser->c != ']') {
				pt = t;
				continue;
			}
			t = '-';
		} else {
			t = parser->c;
			parser_getc(parser);
		}
		if (pt == -1)
			pt = t;
		for (; pt <= t; pt++)
			TSET(parser->group.tests, pt);
		pt = -1;
	}
	if (invert)
		for (int i = 0; i < (int) ARRLEN(parser->group.tests); i++)
			parser->group.tests[i] ^= ~0;
	return 0;
}

static void beauty_printchar(char ch)
{
	if (ch < 0) {
		fprintf(stderr, "\x1b[32m\\%x\x1b[0m", (uint8_t) ch);
	} else if (ch >= 0 && ch < 32) {
		fprintf(stderr, "\x1b[34m^%c\x1b[0m", ch + '@');
	} else {
		fprintf(stderr, "%c", ch);
	}
}

void parser_skiptoeol(Parser *parser)
{
	while (parser->c != '\n' && parser->c != EOF)
		parser_getc(parser);
}

int parser_run(Parser *parser)
{
	ParserVariable *var = NULL;

	while (parser_parsespace(parser), parser->c != EOF) {
		if (parser->c == '[') {
			RegexNode *prev;

			parser_getc(parser);
			if (parser_parseregexgroup(parser) < 0)
				continue;
			parser_getc(parser);

			if (var != NULL) {
				prev = var->tail;
				var->tail = rx_alloc();
				rx_connect(prev, 0, &parser->group, var->tail);
			}
		} else if (isalpha(parser->c)) {
			ParserVariable v;

			if (parser_parseword(parser) < 0) {
				parser_skiptoeol(parser);
				continue;
			}
			if (parser->c != ':') {
				parser_report(parser, "expected ':' after %s "
						"(variable declaration\n",
						parser->word);
				parser_skiptoeol(parser);
				continue;
			}
			parser_getc(parser);
			strcpy(v.name, parser->word);
			v.head = rx_alloc();
			v.tail = v.head;
			var = parser_addvariable(parser, &v);
		} else if (parser->c == '\\') {
			ParserVariable *v;

			parser_getc(parser);
			if (!isalpha(parser->c)) {
				parser_report(parser, "expected variable name "
						"after '\\'\n");
				parser_skiptoeol(parser);
				continue;
			}
			if (parser_parseword(parser) < 0) {
				parser_skiptoeol(parser);
				continue;
			}
			v = parser_getvariable(parser, parser->word);
			if (v == NULL) {
				parser_report(parser,
					"variable '%s' does not exist\n",
					parser->word);
				continue;
			}
			if (strcmp(v->name, var->name)) {
				RegexGroup cond;

				memset(&cond, 0, sizeof(cond));
				rx_connect(var->tail, 0, &cond, v->head);
				/* troublesome spot: what if the variable
				 * is used twice?
				 */
				var->tail = v->tail;
			}
		} else {
			parser_report(parser, "not sure what token that is: '");
			beauty_printchar(parser->c);
			fprintf(stderr, "'\n");
			parser_skiptoeol(parser);
		}
	}
	return 0;
}
