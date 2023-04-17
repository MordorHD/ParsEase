#include "parsease.h"

int
next_token(IOBUFFER *buf, struct token *token)
{
	int ch, prevCh;
	struct regex_node node;
	uint32_t nName;

	token->pos = btell(buf);
	memset(&node, 0, sizeof(node));
	ch = bgetch(buf);
	switch(ch) {
	case '1' ... '9':
		token->index = ch - '1';
		switch(bgetch(buf)) {
		case ':':
			token->type = TINDEX;
			break;
		case '.':
			token->type = TACCINDEX;
			break;
		default:
			return -1;
		}
		break;
	case 'a' ... 'z':
	case 'A' ... 'Z':
	case '_':
		token->name[0] = ch;
		nName = 1;
		while(isalnum(ch = bgetch(buf)) || ch == '_') {
			if(nName + 1 >= sizeof(token->name))
				return -1;
			token->name[nName++] = ch;
		}
		if(ch != ':')
			return -1;
		token->name[nName] = 0;
		token->type = TVARIABLE;
		break;
	case '\\':
		nName = 0;
		while(isalnum(ch = bgetch(buf)) || ch == '_') {
			if(nName + 1 >= sizeof(token->name))
				return -1;
			token->name[nName++] = ch;
		}
		if(!nName)
			return -1;
		bungetch(buf, ch);
		token->name[nName] = 0;
		token->type = TACCVARIABLE;
        token->flags = 0;
		switch((ch = bgetch(buf))) {
		case '*':
			token->flags |= RXFLAG_TRANSIENT;
			/* fall through */
		case '+': 
			token->flags |= RXFLAG_REPEAT;
			break;
		case '?':
			token->flags |= RXFLAG_TRANSIENT;
			break;
		default:
			bungetch(buf, ch);
		}
		break;
	case ' ':
		token->type = TINDENT;
		token->indent = 1;
		while((ch = bgetch(buf)) == ' ')
			token->indent++;
		if(ch == '\n')
			token->type = TNEWLINE;
		else
			bungetch(buf, ch);
		break;
	case '\n':
		token->type = TNEWLINE;
		break;
	case '.':
		memset(node.tests, 0xff, sizeof(node.tests));
		goto add_node;
	case '[':
		token->type = TNODE;
		ch = bgetch(buf);
		if(ch == ']') {
			node.flags = RXFLAG_EMPTYGROUP;
			token->node = rx_alloc(&node);
			return 0;
		}
		prevCh = 0;
		do {
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
					TSET(node.tests, i);
			}
			TSET(node.tests, ch);
			prevCh = ch;
			ch = bgetch(buf);
			if(ch == EOF) {
				fprintf(stderr, "'[' expects a closing bracket ']'\n");
				return -1;
			}
		} while(ch != ']');
	add_node:
		switch((ch = bgetch(buf))) {
		case '*':
			node.flags |= RXFLAG_TRANSIENT;
			/* fall through */
		case '+': 
			node.flags |= RXFLAG_REPEAT;
			break;
		case '?':
			node.flags |= RXFLAG_TRANSIENT;
			break;
		default:
			bungetch(buf, ch);
		}
		token->node = rx_alloc(&node);
		break;
	default:
		return 1;
	}
	return 0;
}



