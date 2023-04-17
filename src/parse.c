#include "parsease.h"

int
init_parser(struct parser *parser)
{
	memset(parser, 0, sizeof(*parser));
	parser->root = rx_allocempty();
	parser->index = -1;
	return 0;
}

static void
parser_addrequest(struct parser *parser, struct request *request)
{
	parser->requests = realloc(parser->requests, sizeof(*parser->requests) * (parser->nRequests + 1));
	parser->requests[parser->nRequests++] = *request;
}

static void
parser_setindexnode(struct parser *parser, node_t node)
{
	struct request *r;
	uint32_t n;

	if(parser->index < 0)
		return;
	parser->hasIndex |= 1 << parser->index;
	parser->indexNodes[parser->index] = node;
	for(r = parser->requests, n = parser->nRequests; n; r++) {
		n--;
		if((int) r->index == parser->index) {
			rx_addchild(r->from, node);
			memmove(r, r + 1, sizeof(*r) * n);
			parser->nRequests--;
		}
	}
	parser->index = -1;
}

static int
parser_setvariable(struct parser *parser, const char *name, node_t node)
{
	for(uint32_t i = 0; i < parser->nVariables; i++)
		if(!strcmp(parser->variables[i].name, name))
			return -1;
	parser->variables = realloc(parser->variables, sizeof(*parser->variables) * (parser->nVariables + 1));
	strcpy(parser->variables[parser->nVariables].name, name);
	parser->variables[parser->nVariables++].node = node;
	return 0;
}

static struct parser_variable *
parser_getvariable(struct parser *parser, const char *name)
{
	for(uint32_t i = 0; i < parser->nVariables; i++)
		if(!strcmp(parser->variables[i].name, name))
			return parser->variables + i;
	return 0;
}

static inline int 
parse_group(IOBUFFER *buf, struct regex_node *node)
{
	int ch;

	memset(node, 0, sizeof(*node));
	ch = bgetch(buf);
	if(ch == ']') {
		node->flags = RXFLAG_EMPTYGROUP;
	} else {
		int prevCh = 0;
		const bool negate = ch == '^';
		if(negate)
			ch = bgetch(buf);
		do {
			if(ch == EOF) {
				fprintf(stderr, "'[' expects a closing bracket ']'\n");
				return -1;
			}
			if(ch == '\\') {
				ch = bgetch(buf);
				switch(ch) {
				case 'a': ch = '\a'; break;
				case 'b': ch = '\b'; break;
				case 'f': ch = '\f'; break;
				case 'n': ch = '\n'; break;
				case 'r': ch = '\r'; break;
				case 't': ch = '\t'; break;
				case 'v': ch = '\v'; break;
				}
			} else if(ch == '-') {
				ch = bgetch(buf);
				if(ch == EOF) {
					fprintf(stderr, "'-' expects a second operand\n'");
					return -1;
				}
				for(char i = prevCh; i != (char) ch; i++)
					TSET(node->tests, i);
			}
			TSET(node->tests, ch);
			prevCh = ch;
			ch = bgetch(buf);
		} while(ch != ']');
		if(negate) {
			for(int i = 0; i < (int) ARRLEN(node->tests); i++)
				node->tests[i] ^= 0xffffffff;
		}
	}
	return 0;
}

static int
parse_regex(IOBUFFER *buf, node_t *pEntryNode, node_t *pExitNode)
{
	int ch;
	struct regex_node rawNode;
	node_t entryNode = 0, exitNode = 0;
	node_t *deepestNodes = NULL;
	uint32_t nDeepestNodes = 0;

	bool
	check_condition(void)
	{
		if(entryNode == exitNode && (nodes[exitNode].flags & RXFLAG_EMPTYGROUP)) {
			fprintf(stderr, "operator '%c' cannot come after an empty group\n", ch);
			free(deepestNodes);
			return false;
		}
		return true;
	}

	while(ch = bgetch(buf), ch != EOF) {
		switch(ch) {
		case ' ':
		case '\f':
		case '\v':
		case '\t':
			break;
		case '.':
			rawNode.flags = 0;
			rawNode.nodes[0] = 0;
			memset(rawNode.tests, 0xff, sizeof(rawNode.tests));
			TTOGGLE(rawNode.tests, '\n');
			goto add_node;
		case '[':
			if(parse_group(buf, &rawNode) < 0)
				return -1;
		add_node:
			if(entryNode) {
				const node_t node = rx_alloc(&rawNode);
				rx_addchild(exitNode, node);
				exitNode = node;
			} else {
				entryNode = exitNode = rx_alloc(&rawNode);
			}
			switch(ch = bgetch(buf)) {
			case '*':
				nodes[exitNode].flags |= RXFLAG_TRANSIENT | RXFLAG_REPEAT;
				rx_addchild(exitNode, exitNode);
				break;
			case '+': 
				nodes[exitNode].flags |= RXFLAG_REPEAT;
				rx_addchild(exitNode, exitNode);
				break;
			case '?':
				nodes[exitNode].flags |= RXFLAG_TRANSIENT;
				break;
			case '|': {
				deepestNodes = realloc(deepestNodes, sizeof(*deepestNodes) * (nDeepestNodes + 1));
				deepestNodes[nDeepestNodes++] = exitNode;
				const node_t node = rx_allocempty();
				rx_addchild(node, entryNode);
				entryNode = exitNode = node;
				break;
			}
			default:
				bungetch(buf, ch);
			}
			break;
		case '(': {
			node_t n1, n2;
			if(parse_regex(buf, &n1, &n2))
				return -1;
			if(entryNode) {
				rx_addchild(exitNode, n1);
				exitNode = n2;
			} else {
				entryNode = n1;
				exitNode = n2;
			}
			switch(ch = bgetch(buf)) {
			case '*':
				nodes[n2].flags |= RXFLAG_TRANSIENT | RXFLAG_REPEAT;
				rx_addchild(n2, n1);
				break;
			case '+': 
				nodes[n2].flags |= RXFLAG_REPEAT;
				rx_addchild(n2, n1);
				break;
			case '?':
				nodes[n2].flags |= RXFLAG_TRANSIENT;
				break;
			case '|': {
				deepestNodes = realloc(deepestNodes, sizeof(*deepestNodes) * (nDeepestNodes + 1));
				deepestNodes[nDeepestNodes++] = n2;
				const node_t node = rx_allocempty();
				rx_addchild(node, n1);
				entryNode = exitNode = node;
				break;
			}
			default:
				bungetch(buf, ch);
			}
			break;
		}
		case ')':
			goto end;
		default:
			bungetch(buf, ch);
			goto end;
		}
	}
end:
	if(deepestNodes) {
		const node_t node = rx_allocempty();
		for(uint32_t i = 0; i < nDeepestNodes; i++)
			rx_addchild(deepestNodes[i], node);
		rx_addchild(exitNode, node);
		exitNode = node;
		free(deepestNodes);
	}
	*pEntryNode = entryNode;
	*pExitNode = exitNode;
	return 0;
}

int
parse(struct parser *parser)
{
	// testing parse_regex function
	node_t en, ex;
	if(parse_regex(&parser->buf, &en, &ex) < 0)
		return -1;
	rx_addchild(parser->root, en);
	return 0;
	// the below code was before parse_regex was there, it is old code and will be replaced soon
	int code = 0;
	struct token tok;
	// generally the last node received
	node_t lNode = 0;
	// last node on current line
	node_t clNode = 0;
	struct parser_variable *var;

	while(!next_token(&parser->buf, &tok)) {
		switch(tok.type) {
		case TINDENT:
			if(clNode || parser->onVariable)
				break;
			if((tok.indent % 2) || (tok.indent /= 2) > parser->maxIndent + parser->inVariable) {
				fprintf(stderr, "inconsistent indentation\n");
				code = -1;
				goto end;
			}
			parser->indent += tok.indent;
			break;
		case TNEWLINE:
			if(parser->onVariable) {
				parser->cacheNode = rx_allocempty();
				lNode = parser->cacheNode;
			}
			parser->onVariable = false;
			parser->indent = 0;
			lNode = clNode;
			clNode = 0;
			parser->index = -1;
			break;
		case TINDEX:
			parser->index = tok.index;
			break;
		case TACCINDEX:
			if(!clNode) {
				fprintf(stderr, "reference must come after node\n");
				code = -1;
				goto end;
			}
			if(parser->hasIndex & (1 << tok.index))
				rx_addchild(clNode, parser->indexNodes[tok.index]);
			else
				parser_addrequest(parser,
					&(struct request) {
						.from = clNode,
						.index = tok.index
					});
			break;
		case TVARIABLE:
			if(clNode) {
				fprintf(stderr, "variable must be at start of line\n");
				code = -1;
				goto end;
			}
			if(parser->inVariable) {
				if(parser_setvariable(parser, parser->cacheName, parser->cacheNode)) {
					fprintf(stderr, "variable '%s' exists already\n", parser->cacheName);
					code = -1;
					goto end;
				}
			}
			strcpy(parser->cacheName, tok.name);
			parser->inVariable = true;
			parser->onVariable = true;
			break;
		case TACCVARIABLE:
			var = parser_getvariable(parser, tok.name);
			if(!var) {
				fprintf(stderr, "variable '%s' doesn't exist\n", tok.name);
				code = -1;
				goto end;
			}
			tok.node = rx_duplicate(var->node);
			nodes[tok.node].flags |= tok.flags; // WATCH OUT!!
			/* fall through */
		case TNODE:
			if(parser->inVariable) {
				if(parser->onVariable) {
					if(!clNode)
						parser->cacheNode = tok.node;
					clNode = tok.node;
					parser->onVariable = false;
					parser->indent = 1;
					break;
				} else {
					if(!parser->indent) {
						if(parser_setvariable(parser, parser->cacheName, parser->cacheNode)) {
							fprintf(stderr, "variable '%s' exists already\n", parser->cacheName);
							code = -1;
							goto end;
						}
						clNode = parser->root;
						parser->inVariable = false;
					} else
						parser->indent--;
				}
			}
			parser_setindexnode(parser, tok.node);
			if(clNode) {
				rx_addchild(clNode, tok.node);	
			} else if(parser->indent == parser->maxIndent) {
				rx_addchild(lNode, tok.node);
				parser->indentNodes = realloc(parser->indentNodes, sizeof(*parser->indentNodes) * (parser->maxIndent + 1));
				parser->indentNodes[parser->maxIndent++] = lNode;
			} else {
				parser->maxIndent = parser->indent + 1;
				rx_addchild(parser->indentNodes[parser->indent], tok.node);
			}
			clNode = tok.node;
			if(tok.type == TACCVARIABLE) {
				while(nodes[clNode].nodes[0])
					clNode = nodes[clNode].nodes[0];
			}
			break;
		}
	}
end:
	free(parser->indentNodes);
	free(parser->variables);
	free(parser->requests);
	return code;
}
