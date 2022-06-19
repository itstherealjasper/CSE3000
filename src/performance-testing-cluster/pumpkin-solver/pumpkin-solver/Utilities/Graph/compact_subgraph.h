#pragma once

#include "adjacency_list_graph.h"

#include <vector>

namespace Pumpkin
{
namespace Graph
{
struct CompactSubGraph
{
	CompactSubGraph(int num_nodes):graph(num_nodes), mapping_subgraph_node_to_old_node(num_nodes, -1) {}

	AdjacencyListGraph graph;
	std::vector<int> mapping_subgraph_node_to_old_node;
};
}
}