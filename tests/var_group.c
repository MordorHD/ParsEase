#include "parsease.h"

void print_char(char ch)
{
	if (ch < 0) {
		fprintf(stderr, "\x1b[32m\\%x\x1b[0m", (uint8_t) ch);
	} else if (ch >= 0 && ch < 32) {
		fprintf(stderr, "\x1b[34m^%c\x1b[0m", ch + '@');
	} else {
		fprintf(stderr, "%c", ch);
	}
}

void print_regexgroup(const RegexGroup *group)
{
	const int uint16_bits = sizeof(uint16_t) * CHAR_BIT;
	int succ = 0;

	for (int i = 0; i < (int) ARRLEN(group->tests); i++) {
		uint16_t t;

		t = group->tests[i];
		for (int j = 0, p; j < uint16_bits; j++) {
			p = i * uint16_bits + j;
			if (t & 1)
				succ++;
			if (!(t & 1) || (p == 255 && ++p)) {
				if (succ > 3) {
					print_char(p - succ);
					fprintf(stderr, "\x1b[31m-\x1b[0m");
					print_char(p - 1);
					succ = 0;
				} else {
					for (; succ > 0; succ--)
						print_char(p - succ);
				}
			}
			t >>= 1;
		}
	}
}

void skip_to_eol(Parser *parser)
{
	while (parser->c != '\n' && parser->c != EOF)
		parser_getc(parser);
}

int main(void)
{
	Parser parser;
	struct var {
		char name[PARSER_MAX_WORD];
		RegexGroup group;
	} *vars = NULL, *newVars = NULL, *var = NULL;
	size_t nVars = 0;

	memset(&parser, 0, sizeof(parser));
	parser.fp = stdin;
	parser.path = strdup("stdin");
	parser.c = fgetc(stdin);

	while (parser_parsespace(&parser), parser.c != EOF) {
		if (parser.c == '[') {
			parser_getc(&parser);
			parser_parseregexgroup(&parser);
			if (var == NULL)
				fprintf(stderr, "Parsed(indent=%d): [", parser.indent);
			else
				fprintf(stderr, "Variable %s: [", var->name);
			print_regexgroup(&parser.group);
			fprintf(stderr, "]\n");
			parser_getc(&parser);
			if (var != NULL) {
				var->group = parser.group;
				var = NULL;
			}
		} else if (isalpha(parser.c)) {
			if (parser_parseword(&parser) < 0) {
				skip_to_eol(&parser);
				continue;
			}
			if (parser.c != ':') {
				parser_report(&parser, "expected ':' after %s (variable declaration\n", parser.word);
				skip_to_eol(&parser);
				continue;
			}
			parser_getc(&parser);
			if (var != NULL)
				parser_report(&parser, "missing value for variable '%s'\n", var->name);
			var = NULL;
			for (size_t i = 0; i < nVars; i++)
				if (!strcmp(parser.word, vars[i].name)) {
					var = vars + i;
					parser_report(&parser, "overwriting existing variable '%s'\n", parser.word);
					break;
				}
			if (var == NULL) {
				newVars = realloc(vars, sizeof(*vars) * (nVars + 1));
				if (newVars == NULL) {
					parser_report(&parser, "was not able to add variable: %s\n", strerror(errno));
					continue;
				}
				vars = newVars;
				var = vars + nVars;
				nVars++;
				strcpy(var->name, parser.word);
				memset(&var->group, 0, sizeof(var->group));
			}
		} else if (parser.c == '\\') {
			size_t varIndex;

			parser_getc(&parser);
			if (!isalpha(parser.c)) {
				parser_report(&parser, "expected variable name after '\\'\n");
				skip_to_eol(&parser);
				continue;
			}
			if (parser_parseword(&parser) < 0) {
				skip_to_eol(&parser);
				continue;
			}
			for (varIndex = 0; varIndex < nVars; varIndex++)
				if (!strcmp(parser.word, vars[varIndex].name))
					break;
			if (varIndex == nVars) {
				parser_report(&parser, "variable '%s' does not exist\n", parser.word);
				continue;
			}
			parser.group = vars[varIndex].group;
			if (var == NULL)
				fprintf(stderr, "Parsed(indent=%d): [", parser.indent);
			else
				fprintf(stderr, "Variable %s: [", var->name);
			print_regexgroup(&parser.group);
			fprintf(stderr, "]\n");
			if (var != NULL) {
				var->group = parser.group;
				var = NULL;
			}
		} else {
			parser_report(&parser, "not sure what token that is: '");
			print_char(parser.c);
			fprintf(stderr, "'\n");
			skip_to_eol(&parser);
		}
	}

	parser_cleanup(&parser);
	return 0;
}
