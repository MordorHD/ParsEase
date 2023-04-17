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
	nodes[iNode] = (struct regex_node) { .flags = RXFLAG_EMPTYGROUP, .nNodes = 0, .nodes = NULL };
	return iNode;
}

__attribute__((destructor)) static void
delete_nodes(void)
{
	for(uint32_t i = 0; i < nNodes; i++)
		free(nodes[i].nodes);
	free(nodes);
}

void
rx_addchild(node_t parent, node_t child)
{
	struct regex_node *const p = nodes + parent;
	p->nodes = realloc(p->nodes, sizeof(*p->nodes) * (p->nNodes + 1));
	p->nodes[p->nNodes++] = child;
}

node_t
rx_duplicate(node_t node)
{
	node_t dup;
	
	dup = rx_alloc(nodes + node);
	nodes[dup].nodes = nodes[node].nNodes ? malloc(sizeof(*nodes[dup].nodes) * nodes[node].nNodes) : NULL;
	nodes[node].flags |= RXFLAG_VISITED;
	for(uint32_t i = 0; i < nodes[node].nNodes; i++) {
		if(!(nodes[nodes[node].nodes[i]].flags & RXFLAG_VISITED))
			nodes[dup].nodes[i] = rx_duplicate(nodes[node].nodes[i]);
	}
	nodes[node].flags ^= RXFLAG_VISITED;
	return dup;
}

void
rx_getdeepestnodes(node_t node, node_t **deepestNodes, uint32_t *nDeepestNodes)
{
	struct regex_node *const root = nodes + node;
	node_t *ns = *deepestNodes;
	uint32_t n = *nDeepestNodes;
	
	if(!root->nNodes) {
		ns = realloc(ns, sizeof(*ns) * (n + 1));
		ns[n++] = node;
	} else {
		root->flags |= RXFLAG_VISITED;
		for(uint32_t i = 0; i < root->nNodes; i++)
			if(!(nodes[root->nodes[i]].flags & RXFLAG_VISITED))
				rx_getdeepestnodes(root->nodes[i], &ns, &n);
		root->flags ^= RXFLAG_VISITED;
	}
	*deepestNodes = ns;
	*nDeepestNodes = n;
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
	if(ch == EOF)
		return 1;
	// -1: no node matched
	//  0: one node matched
	//  1: one node matched but one that has `no consume` enabled
	int
	recursive_check(struct regex_node *node)
	{
		if(node->flags & RXFLAG_REPEAT) {
			if(TCHECK(node->tests, ch)) {
				return 0;
			}
		}
		for(node_t *s = node->nodes, *e = s + node->nNodes; s != e; s++) {
			const node_t n = *s;
			struct regex_node *const child = nodes + n;
			if((child->flags & RXFLAG_EMPTYGROUP)
			|| TCHECK(child->tests, ch)) {
				path->nodes = realloc(path->nodes, sizeof(*path->nodes) * (path->nNodes + 1));
				path->nodes[path->nNodes++] = n;
				if(child->flags & (RXFLAG_NOCONSUME | RXFLAG_EMPTYGROUP))
					return 1;
				return 0;
			}
			if(child->flags & RXFLAG_TRANSIENT) {
				const int r = recursive_check(child);
				if(r != -1)
					return r;
			}
		}
		return -1;
	}
	while(1) {
		const int r = recursive_check(nodes + path->nodes[path->nNodes - 1]);
		switch(r) {
		case -1:
			return -1;
		case 0:
			return 0;
		case 1:
			break;
		}
	}
}

void
rx_close(struct regex_path *path)
{
	free(path->nodes);
}

