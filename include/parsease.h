#ifndef INCLUDED_PARSEASE_H
#define INCLUDED_PARSEASE_H

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define ARRLEN(a) (sizeof(a)/sizeof*(a))

#define ASSERT(expr, msg) do { \
	if(expr) \
		break; \
	fprintf(stderr, "%s:%s:%d - %s\n", __FILE__, __FUNCTION__, __LINE__, (msg)); \
	exit(1); \
} while(0)

typedef struct iobuffer {
	int fd;
	char buf[0x800];
	int32_t iRead, iWrite;
} IOBUFFER;

int bopen(IOBUFFER *buf, const char *fileName);
uint32_t btell(IOBUFFER *buf);
int brewind(IOBUFFER *buf);
int bgetch(IOBUFFER *buf);
int bungetch(IOBUFFER *buf, int ch);
int bclose(IOBUFFER *buf);

#define TCHECK(t, b) ((t)[(uint8_t)(b)>>4]&(1<<((uint16_t)(b)&0xf)))
#define TSET(t, b) ((t)[(uint8_t)(b)>>4]|=(1<<((uint16_t)(b)&0xf)))
#define TTOGGLE(t, b) ((t)[(uint8_t)(b)>>4]^=(1<<((uint16_t)(b)&0xf)))

enum {
	RXFLAG_TRANSIENT = 1 << 0,
	RXFLAG_NOCONSUME = 1 << 1,
	RXFLAG_EMPTYGROUP = 1 << 2,
	RXFLAG_REPEAT = 1 << 3,
	RXFLAG_VISITED = 1 << 4,
};

typedef uint32_t node_t;

struct regex_node {
	uint32_t flags;
	// this hard limit of 8 (+ null terminator) nodes will get removed soon,
	// this is just easier for memory management right now
	node_t nodes[9];
	short tests[256 / 8 / sizeof(short)];
	// debugging variables
	uint32_t line;
};

extern struct regex_node *nodes;
extern node_t nNodes;

node_t rx_alloc(const struct regex_node *node);
node_t rx_allocempty(void);
node_t rx_duplicate(node_t node);
void rx_getdeepestnodes(node_t node, node_t **depestNodes, uint32_t *nDeepestNodes);
int rx_addchild(node_t parent, node_t child);

struct regex_path {
	uint32_t nNodes;
	node_t *nodes;
};

void rx_begin(struct regex_path *path, node_t root);
int rx_follow(struct regex_path *path, int ch);
void rx_close(struct regex_path *path);

enum {
	TNULL,
	TINDENT,
	TNEWLINE,
	TINDEX,
	TACCINDEX,
	TVARIABLE,
	TACCVARIABLE,
	TNODE,
};
struct token {
	uint32_t type;
	uint32_t pos;
	union {
		uint32_t indent;
		int index;
		node_t node;
		struct {
			char name[60];
			uint32_t flags;
		};
	};
};

int next_token(IOBUFFER *buf, struct token *token);

struct request {
	node_t from;
	uint32_t index;
};

struct parser {
	IOBUFFER buf;
	// indexed nodes; defined with [0-9]:
	node_t indexNodes[9];
	// bit field, toggled bit means index is defined
	uint32_t hasIndex;
	// root node of the parsing process
	node_t root;
	// current level on indentation
	uint32_t indent;
	// maximum valid level of indentation
	uint32_t maxIndent;
	// nodes on different level of indentation
	node_t *indentNodes;
	struct parser_variable {
		char name[60];
		node_t node;
	} *variables;
	uint32_t nVariables;
	uint32_t nRequests;
	struct request *requests;
	bool inVariable;
	bool onVariable;
	char cacheName[64];
	node_t cacheNode;
	int index;
};

int init_parser(struct parser *parser);
int parse(struct parser *parser);

#endif
