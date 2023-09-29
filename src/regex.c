#include "parsease.h"

RegexNode *rx_alloc(void)
{
	RegexNode *node;

	if ((node = malloc(sizeof(*node))) == NULL)
		return NULL;
	memset(node, 0, sizeof(*node));
	return node;
}

RegexBranch *rx_connect(RegexNode *from, uint64_t flags, const RegexGroup *cond,
		RegexNode *to)
{
	RegexBranch *newBranches;

	newBranches = realloc(from->branches, sizeof(*from->branches) *
			(from->nBranches + 1));
	if (newBranches == NULL)
		return NULL;
	from->branches = newBranches;
	newBranches += from->nBranches;
	from->nBranches++;
	newBranches->flags = flags;
	newBranches->condition = *cond;
	newBranches->to = to;
	return newBranches;
}
