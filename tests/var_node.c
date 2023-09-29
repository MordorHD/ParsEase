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

void print_regexnode(const RegexNode *node, int depth)
{
	if (depth == 12) {
		fprintf(stderr, "...");
		return;
	}
	fprintf(stderr, "[]");
	for (size_t i = 0; i < node->nBranches; i++) {
		const RegexBranch *const branch = node->branches + i;
		fprintf(stderr, "\n");
		for (int i = 0; i <= depth; i++)
			fprintf(stderr, "  ");
		print_regexgroup(&branch->condition);
		fprintf(stderr, " - ");
		print_regexnode(branch->to, depth + 1);
	}
}

void skip_to_eol(Parser *parser)
{
	while (parser->c != '\n' && parser->c != EOF)
		parser_getc(parser);
}

ParserVariable *get_variable(Parser *parser, const char *name)
{
	for (size_t i = 0; i < parser->nVariables; i++)
		if (!strcmp(parser->variables[i].name, name))
			return parser->variables + i;
	return NULL;
}

ParserVariable *add_variable(Parser *parser, const ParserVariable *var)
{
	ParserVariable *newVars;

	if ((newVars = get_variable(parser, var->name)) != NULL)
		parser_report(parser, "overwriting existing variable '%s'\n",
				var->name);
	if (newVars == NULL) {
		newVars = realloc(parser->variables,
				sizeof(*parser->variables) *
				(parser->nVariables + 1));
		if (newVars == NULL) {
			parser_report(parser, "ran out of memory trying to add variable '%s'\n", var->name);
			return NULL;
		}
		parser->variables = newVars;
		newVars += parser->nVariables;
		parser->nVariables++;
	}
	*newVars = *var;
	return newVars;
}


int main(void)
{
	Parser parser;
	ParserVariable *var = NULL;

	memset(&parser, 0, sizeof(parser));
	parser.fp = stdin;
	parser.path = strdup("stdin");
	parser.c = fgetc(stdin);

	while (parser_parsespace(&parser), parser.c != EOF) {
		if (parser.c == '[') {
			RegexNode *prev;

			parser_getc(&parser);
			parser_parseregexgroup(&parser);
			fprintf(stderr, "Parsed(indent=%d): [", parser.indent);
			print_regexgroup(&parser.group);
			fprintf(stderr, "]\n");
			parser_getc(&parser);

			if (var != NULL) {
				prev = var->tail;
				var->tail = rx_alloc();
				rx_connect(prev, 0, &parser.group, var->tail);
			}
		} else if (isalpha(parser.c)) {
			ParserVariable v;

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
			strcpy(v.name, parser.word);
			v.head = rx_alloc();
			v.tail = v.head;
			var = add_variable(&parser, &v);
		} else if (parser.c == '\\') {
			ParserVariable *v;

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
			if ((v = get_variable(&parser, parser.word)) == NULL) {
				parser_report(&parser, "variable '%s' does not exist\n", parser.word);
				continue;
			}
			fprintf(stderr, "Variable %s(indent=%d): ",
					v->name, parser.indent);
			print_regexnode(v->head, 0);
			fprintf(stderr, "\n");
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
			parser_report(&parser, "not sure what token that is: '");
			print_char(parser.c);
			fprintf(stderr, "'\n");
			skip_to_eol(&parser);
		}
	}

	parser_cleanup(&parser);
	return 0;
}
