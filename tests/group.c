#include "parsease.h"

void print_char(char ch)
{
	if (ch < 0) {
		printf("\x1b[32m\\%x\x1b[0m", (uint8_t) ch);
	} else if (ch >= 0 && ch < 32) {
		printf("\x1b[34m^%c\x1b[0m", ch + '@');
	} else {
		printf("%c", ch);
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
					printf("\x1b[31m-\x1b[0m");
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

int main(void)
{
	Parser parser;

	memset(&parser, 0, sizeof(parser));
	parser.fp = stdin;
	parser.path = strdup("stdin");
	parser.c = fgetc(stdin);

	while (parser_parsespace(&parser), parser.c == '[') {
		parser_getc(&parser);
		parser_parseregexgroup(&parser);
		printf("Parsed: [");
		print_regexgroup(&parser.group);
		printf("]\n");
		parser_getc(&parser);
	}

	parser_cleanup(&parser);
	return 0;
}
