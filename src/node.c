#include "parsease.h"

struct regex_node *nodes;
uint32_t nNodes;

node_t
rx_alloc(const struct regex_node *node)
{
	const struct regex_node n = *node;
	const node_t iNode = nNodes++;
	nodes = realloc(nodes, sizeof(*nodes) * nNodes);
	nodes[iNode] = n;
	return iNode;
}

node_t
rx_allocempty(void)
{
	const node_t iNode = nNodes++;
	nodes = realloc(nodes, sizeof(*nodes) * nNodes);
	memset(&nodes[iNode], 0, sizeof(nodes[iNode]));
	nodes[iNode].flags = RXFLAG_EMPTYGROUP;
	return iNode;
}

__attribute__((destructor)) static void
delete_nodes(void)
{
	free(nodes);
}

int
rx_addchild(node_t parent, node_t child)
{
	struct regex_node *const p = nodes + parent;
	node_t *n;
	for(n = p->nodes; *n; n++);
	if(p->nodes + (ARRLEN(p->nodes) - 1) == n)
		return -1;
	*n = child;
	return 0;
}

node_t
rx_duplicate(node_t node)
{
	node_t dup;
	
	dup = rx_alloc(nodes + node);
	nodes[node].flags |= RXFLAG_VISITED;
	for(uint32_t i = 0; i < ARRLEN(nodes[node].nodes); i++) {
		if(nodes[node].nodes[i] && !(nodes[nodes[node].nodes[i]].flags & RXFLAG_VISITED))
			nodes[dup].nodes[i] = rx_duplicate(nodes[node].nodes[i]);
	}
	nodes[node].flags ^= RXFLAG_VISITED;
	return dup;
}

void
rx_begin(struct regex_path *path, node_t root)
{
	path->nodes = malloc(sizeof(*path->nodes));
	path->nodes[0] = root;
	path->nNodes = 1;
}

int
rx_follow(struct regex_path *path, int ch)
{
	node_t emergency = 0;
	if(ch == EOF)
		return 1;
again:
	for(node_t n, *s = nodes[path->nodes[path->nNodes - 1]].nodes; n = *s; s++) {
		struct regex_node *const child = nodes + n;
		if(child->flags & RXFLAG_EMPTYGROUP)
			emergency = n;
		else if(TCHECK(child->tests, ch)) {
			path->nodes = realloc(path->nodes, sizeof(*path->nodes) * (path->nNodes + 1));
			path->nodes[path->nNodes++] = n;
			if(child->flags & (RXFLAG_NOCONSUME | RXFLAG_EMPTYGROUP))
				goto again;
			return 0;
		}
	}
	if(emergency) {
		path->nodes = realloc(path->nodes, sizeof(*path->nodes) * (path->nNodes + 1));
		path->nodes[path->nNodes++] = emergency;
		goto again;
	}
	return -1;
}

void
rx_close(struct regex_path *path)
{
	free(path->nodes);
}

