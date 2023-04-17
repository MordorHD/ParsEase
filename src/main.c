#include "parsease.h"

void
tests_print(short tests[16])
{
	int start;
	int count = 0;
	bool pc = false;
	for(int i = 0; i <= 0xff; i++) {
		if(TCHECK(tests, i)) {
			if(!count)
				start = i;
			count++;
		} else {
			if(!pc) {
				putchar('[');
				pc = true;
			}
			if(count > 3) {
				if(isprint(start) && isprint(start + count - 1))
					printf("%c-%c", start, start + count - 1);
				else
					printf("\\%x-\\%x", start, start + count - 1);
			} else if(count) {
				for(int c = start; c < start + count; c++) {
					if(isprint(c))
						putchar(c);
					else {
						int a = c;
						putchar('\\');
						if(a > 0xf) {
							putchar((a & 0xf) + ((a & 0xf) >= 0xa ? 'a' - 10 : '0'));
							a >>= 4;
						} else
							putchar('0');
						putchar((a & 0xf) + ((a & 0xf) >= 0xa ? 'a' - 10 : '0'));
					}
				}
			}
			count = 0;
		}
	}
	putchar(pc ? ']' : '.');

}

void
print_node_single(struct regex_node *node, int indent)
{
	if(node->flags & RXFLAG_CONTEXTMASK) {
		int f = node->flags >> RXFLAG_CONTEXTSHIFT;
		int i = 1;
		while(f) {
			if(f & 1)
				break;
			i++;
			f >>= 1;
		}
		printf("%d:", i);
		for(int i = 0; i < 2 * (indent - 1); i++)
			putchar(' ');
	} else
		for(int i = 0; i < 2 * indent; i++)
			putchar(' ');
	if(!(node->flags & RXFLAG_EMPTYGROUP))
		tests_print(node->tests);
	else
		printf("[]");
	if((node->flags & (RXFLAG_TRANSIENT | RXFLAG_REPEAT)) == (RXFLAG_TRANSIENT | RXFLAG_REPEAT))
		printf("*");
	else if(node->flags & RXFLAG_TRANSIENT)
		printf("?");
	else if(node->flags & RXFLAG_REPEAT)
		printf("+");
}

void
print_node(struct regex_node *node, int indent)
{
	for(node_t *s = node->nodes, *e = s + node->nNodes; s != e; s++) {
		const node_t n = *s;
		struct regex_node *const node = nodes + n;
		if(node->flags & RXFLAG_VISITED) {
			if(node->flags & RXFLAG_CONTEXTMASK) {
				for(int i = 0; i < 2 * indent; i++)
					putchar(' ');
				int f = node->flags >> RXFLAG_CONTEXTSHIFT;
				int i = 1;
				while(f) {
					if(f & 1)
						break;
					i++;
					f >>= 1;
				}
				printf("%d.\n", i);
				continue;
			} else if(node->nNodes) {
				for(int i = 0; i < 2 * (indent + 1); i++)
					putchar(' ');
				printf("*\n");
			}
			continue;
		}
		print_node_single(node, indent);
		printf("\n");
		node->flags |= RXFLAG_VISITED;
		print_node(node, indent + 1);
		node->flags ^= RXFLAG_VISITED;
	}
}

void
print_node_logical(struct regex_node *node, int indent)
{
	print_node_single(node, indent);
	printf("\n");
	if(node->flags & RXFLAG_VISITED) {
		if(node->nNodes) {
			for(int i = 0; i < 2 * (indent + 1); i++)
				putchar(' ');
			printf("*\n");
		}
		return;
	}
	if(!(node->flags & (RXFLAG_TRANSIENT | RXFLAG_REPEAT)))
		return;
	node->flags |= RXFLAG_VISITED;
	for(node_t *s = node->nodes, *e = s + node->nNodes; s != e; s++) {
		const node_t n = *s;
		struct regex_node *const node = nodes + n;
		print_node_logical(node, indent + 1);
	}
	node->flags ^= RXFLAG_VISITED;
}

int
main(int argc, char **argv)
{
	struct parser parser;

	if(argc != 2 && argc != 3) {
		fprintf(stderr, "Bad usage! Right usage: %s FILENAME.rgs [FILENAME]\n", argv[1]);
		return 1;
	}
	if(init_parser(&parser)) {
		perror("init_parser");
		return -1;
	}
	if(bopen(&parser.buf, argv[1])) {
		perror("bopen");
		return -1;
	}
	if(parse(&parser)) {
		bclose(&parser.buf);
		return -1;
	}
	print_node(nodes + parser.root, 0);
	bclose(&parser.buf);
	if(argc > 2) {
		if(bopen(&parser.buf, argv[2])) {
			perror("bopen");
			return -1;
		}
		struct regex_path path;
		rx_begin(&path, parser.root);
		int ch;
		while((ch = bgetch(&parser.buf)) != EOF) {
			if(rx_follow(&path, ch) < 0) {
				const int atChar = ch;
				uint32_t pos = btell(&parser.buf) - 1;
				uint32_t line = 1, col = 1;
				brewind(&parser.buf);
				while(pos) {
					ch = bgetch(&parser.buf);
					if(ch == '\n') {
						line++;
						col = 1;
					} else
						col++;
					pos--;
				}
				fprintf(stderr, "error: invalid token '%c' at %u:%u; failed at node:\n", atChar, line, col);
				print_node_logical(nodes + path.nodes[path.nNodes - 1], 0);
				break;
			}
		}
		rx_close(&path);
		bclose(&parser.buf);
	}
	return 0;
}

