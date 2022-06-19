#pragma once

#include "adjacency_list_graph.h"
#include "../pumpkin_assert.h"

#include <vector>
#include <algorithm>

namespace Pumpkin
{
namespace Graph
{
class TopologicalSortComputer
{
public:
	std::vector<int> Solve(const AdjacencyListGraph& graph)
	{
        Initialise(graph);
        for (int node_index = 0; node_index < graph.NumNodes(); node_index++)
        {
            Visit(node_index);
        }
        std::reverse(topologically_sorted_nodes_.begin(), topologically_sorted_nodes_.end());
        pumpkin_assert_advanced(DebugCheckTopologicalSort(), "Error: something went wrong with the topological sort.");
        return topologically_sorted_nodes_;
	}

private:

    void Initialise(const AdjacencyListGraph& graph)
    {
        graph_ = &graph;
        info_.resize(graph.NumNodes());
        for (int i = 0; i < graph.NumNodes(); i++)
        {
            info_[i].already_processed = false;
            info_[i].is_being_processed = false;
        }
        topologically_sorted_nodes_.clear();
    }

    void Visit(int node_index)
    {
        if (info_[node_index].already_processed) { return; }

        if (info_[node_index].is_being_processed) { pumpkin_assert_permanent(0, "Error: topological sort is not possible for graphs with cycles!\n"); return; }

        info_[node_index].is_being_processed = true;

        for (int neighbour_index : graph_->GetNeighbours(node_index))
        {
            Visit(neighbour_index);
        }

        topologically_sorted_nodes_.push_back(node_index);

        info_[node_index].is_being_processed = false;
        info_[node_index].already_processed = true;
    }

    bool DebugCheckTopologicalSort() const
    {//here we check that for each edge (a, b), 'a' appears before 'b' in the topological sort
        if (topologically_sorted_nodes_.size() != graph_->NumNodes()) { return false; }
        std::vector<int> node_to_position(graph_->NumNodes());
        for (int i = 0; i < graph_->NumNodes(); i++)
        {
            int node_index = topologically_sorted_nodes_[i];
            node_to_position[node_index] = i;
        }

        for (int node_index = 0; node_index < graph_->NumNodes(); node_index++)
        {
            for (int neighbour_index : graph_->GetNeighbours(node_index))
            {
                if (node_to_position[node_index] >= node_to_position[neighbour_index])
                {
                    return false;
                }
            }
        }
        return true;
    }

    const AdjacencyListGraph* graph_;
    struct NodeInformation { bool already_processed, is_being_processed; };
    std::vector<NodeInformation> info_;
    std::vector<int> topologically_sorted_nodes_;
};
}
}