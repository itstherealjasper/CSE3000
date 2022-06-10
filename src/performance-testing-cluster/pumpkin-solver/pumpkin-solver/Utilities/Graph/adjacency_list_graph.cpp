#include "adjacency_list_graph.h"
#include "compact_subgraph.h"

namespace Pumpkin
{
namespace Graph
{

const int AdjacencyListGraph::UNDEFINED_NODE;

CompactSubGraph Pumpkin::Graph::AdjacencyListGraph::CreateCompactSubGraph(std::vector<int>& nodes) const
{
	CompactSubGraph subgraph(nodes.size());
	if (nodes.empty()) { return subgraph; }

	static std::vector<int> old_id_to_subgraph_id;
	old_id_to_subgraph_id.resize(NumNodes(), UNDEFINED_NODE);

	for (int i = 0; i < nodes.size(); i++)
	{
		subgraph.mapping_subgraph_node_to_old_node[i] = nodes[i];
		old_id_to_subgraph_id[nodes[i]] = i;
	}

	for (int new_node_id = 0; new_node_id < nodes.size(); new_node_id++)
	{
		int old_node_id = subgraph.mapping_subgraph_node_to_old_node[new_node_id];
		pumpkin_assert_simple(old_id_to_subgraph_id[old_node_id] == new_node_id, "Sanity check.");
		for (int old_neighbour : GetNeighbours(old_node_id))
		{
			int new_neighbour_id = old_id_to_subgraph_id[old_neighbour];
			if (new_neighbour_id == UNDEFINED_NODE) { continue; }

			subgraph.graph.AddNeighbour(new_node_id, new_neighbour_id);
		}
	}

	//small optimisation to ensure that the contents of the static vector old_id_to_subgraph are always undefined nodes
	for (int old_node_id : nodes)
	{
		pumpkin_assert_simple(old_id_to_subgraph_id[old_node_id] != UNDEFINED_NODE, "Sanity check.");
		old_id_to_subgraph_id[old_node_id] = UNDEFINED_NODE;
	}

	return subgraph;
}
}
}
