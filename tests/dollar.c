#include "parsease.h"

int main(int argc, char **argv)
{
	Parser parser;
	int c;
	size_t dollars = 0;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <file name>\n", argv[0]);
		return -1;
	}

	if (parser_init(&parser, argv[1]) < 0)
		return -1;

	while ((c = parser_getc(&parser)) != EOF) {
		if (c == '$') {
			parser_report(&parser, "Found a dollar!\n");
			dollars++;
		}
	}
	fprintf(stderr, "found %zu dollar(s) in '%s'\n",
			dollars, argv[1]);

	parser_cleanup(&parser);
	return 0;
}
