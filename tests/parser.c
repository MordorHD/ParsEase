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

int main(int argc, char **argv)
{
	Parser parser;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <file name>\n", argv[0]);
		return -1;
	}

	parser_init(&parser, argv[1]);

	parser_run(&parser);

	parser_cleanup(&parser);
	return 0;
}
