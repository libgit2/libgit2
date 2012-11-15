#include "common.h"
#include "commit.h"
#include "oidmap.h"

struct git_graph {
	git_oidmap *commits;
	git_pool commit_pool;
};

struct git_commit_object {
	git_oid oid;
	uint32_t time;
	unsigned int seen:1,
			 uninteresting:1,
			 topo_delay:1,
			 parsed:1,
			 flags : 4;

	unsigned short in_degree;
	unsigned short out_degree;

	struct commit_object **parents;
};


int git_graph_new(git_graph **graph_out)
{
	git_graph *graph;

	graph = git__calloc(1, sizeof(git_revwalk));
	GITERR_CHECK_ALLOC(graph);

	graph->commits = git_oidmap_alloc();
	GITERR_CHECK_ALLOC(graph->commits);

	git_pool_init(&graph->commit_pool, 1,
		git_pool__suggest_items_per_page(COMMIT_ALLOC) * COMMIT_ALLOC) < 0)


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

int git_graph_commit_get(git_graph *graph, git_oid *oid, git_commit_object **commit)
{
	//return commit from hash table if already found, otherwise return a new commit

	return 0;
}

int git_graph_commit_alloc_parents(git_commit_object *commit, unsigned short num_parents)
{
	//allocate parents array in commit object
	return 0;
}

int git_graph_commit_add_parent(git_commit_object *commit, git_commit_object *parent)
{
	//put parent into next slot in commit object
	return 0;
}

int git_graph_merge_base_many(git_commit_object *out, const git_commit_object input_array[], size_t length)
{
	//find merge base for some commit objects
}
