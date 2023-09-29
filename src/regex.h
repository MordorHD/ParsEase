#define TCHECK(t, b) ((t)[(uint8_t)(b)>>4]&(1<<((uint8_t)(b)&0xf)))
#define TSET(t, b) ((t)[(uint8_t)(b)>>4]|=(1<<((uint8_t)(b)&0xf)))
#define TTOGGLE(t, b) ((t)[(uint8_t)(b)>>4]^=(1<<((uint8_t)(b)&0xf)))

typedef struct regex_group {
	uint16_t tests[16];
} RegexGroup;

#define RXFLAG_UNCONDITIONAL 0x01

struct regex_node;

typedef struct regex_branch {
	uint64_t flags;
	RegexGroup condition;
	struct regex_node *to;
} RegexBranch;

typedef struct regex_node {
	RegexBranch *branches;
	size_t nBranches;
} RegexNode;

RegexNode *rx_alloc(void);
RegexBranch *rx_connect(RegexNode *from, uint64_t flags, const RegexGroup *cond,
		RegexNode *to);
