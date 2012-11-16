#include "common.h"
#include "commit.h"
#include "oidmap.h"

GIT__USE_OIDMAP;

struct git_graph {
	git_oidmap *commits;
	git_pool commit_pool;
};

struct git_graph_node {
	git_oid oid;
	uint32_t time;
	unsigned int seen:1,
			 uninteresting:1,
			 topo_delay:1,
			 parsed:1,
			 flags : 4;

	unsigned short in_degree;
	unsigned short out_degree;

	struct git_graph_node **parents;
};

#define PARENTS_PER_COMMIT	2
#define COMMIT_ALLOC \
	(sizeof(git_graph_node) + PARENTS_PER_COMMIT * sizeof(git_graph_node *))


int git_graph_new(git_graph **graph_out)
{
	git_graph *graph;

	graph = git__calloc(1, sizeof(git_graph));
	GITERR_CHECK_ALLOC(graph);

	graph->commits = git_oidmap_alloc();
	GITERR_CHECK_ALLOC(graph->commits);

	git_pool_init(&graph->commit_pool, 1,
		git_pool__suggest_items_per_page(COMMIT_ALLOC) * COMMIT_ALLOC);


	*graph_out = graph;

	return 0;
}

void git_graph_free(git_graph *graph)
{
	if (graph == NULL)
		return;

	git_oidmap_free(graph->commits);
	git_pool_clear(&graph->commit_pool);
	git__free(graph);
}

size_t git_graph_node_num_parents(const git_graph_node *commit)
{
	return commit->out_degree;
}

int git_graph_lookup(git_graph_node **commit, git_graph *graph, const git_oid *oid)
{
	//return commit from hash table if already found, otherwise return NULL

	return 0;
}

int git_graph_push_node(git_graph_node **new_node, git_graph *graph,
	const git_oid *oid, size_t num_parents, const git_oid **parents)
{
	return 0;
}

int git_graph_node_parent(git_graph_node **parent, git_graph_node *commit, size_t index)
{
	*parent = commit->parents[index];
	return 0;
}


int git_graph_merge_base_many(git_graph_node **out, git_graph *graph, size_t num_heads, git_graph_node **heads)
{
	//find merge base for some commit objects
	return 0;
}


int git_graph_node_add_parent(git_graph_node *child, git_graph_node *parent)
{
	//put parent into next slot in commit object
	return 0;
}
