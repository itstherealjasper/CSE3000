#pragma once

#include "adjacency_list_graph.h"
#include "../pumpkin_assert.h"

#include <vector>

namespace Pumpkin
{
namespace Graph
{
class StronglyConnectedComponentsComputer
{
public:

	//returns a vector of vectors v, where v[i] is the vector of nodes that compose a strongly connected component
	//each component has size greater than one (non-trivial)
	std::vector<std::vector<int> > Solve(const AdjacencyListGraph& graph)
	{
		Initialise(graph);
		for (int node_index = 0; node_index < graph.NumNodes(); node_index++)
		{
			Visit(node_index);
		}
		pumpkin_assert_simple(stack_.empty() && num_nodes_visited_ == graph.NumNodes(), "Sanity check.");
		return strongly_connected_components_;
	}

private:
	void Initialise(const AdjacencyListGraph& graph)
	{
		pumpkin_assert_simple(graph.NumNodes() != INT32_MAX, "Sanity check.");

		graph_ = &graph;
		num_nodes_visited_ = 0;
		stack_.clear();
		strongly_connected_components_.clear();

		info_.resize(graph.NumNodes());
		for (int node_index = 0; node_index < graph.NumNodes(); node_index++)
		{
			info_[node_index].already_visited = false;
			info_[node_index].colour = UNDEFINED_COLOUR;
			info_[node_index].is_on_stack = false;
			info_[node_index].lowlink = INT32_MAX;
		}
	}

	void Visit(int node_index)
	{
		if (info_[node_index].already_visited) { return; }

		pumpkin_assert_simple(!info_[node_index].is_on_stack, "Sanity check.");

		info_[node_index].already_visited = true;
		info_[node_index].colour = num_nodes_visited_;
		info_[node_index].lowlink = num_nodes_visited_;
		++num_nodes_visited_;

		stack_.push_back(node_index);
		info_[node_index].is_on_stack = true;

		for (int neighbour_index : graph_->GetNeighbours(node_index))
		{
			Visit(neighbour_index);

			//if the neighbour is still on the stack, update the lowlink of the node
			//	recall that if the neighbour is _not_ on the stack, the neighbour was already processed as part of some other component and should be ignored
			if (info_[neighbour_index].is_on_stack)
			{
				info_[node_index].lowlink = std::min(info_[neighbour_index].lowlink, info_[node_index].lowlink);
			}
		}

		//if the colour and lowlink are the same
		//	then this node is the first node that was visited from the strongly connected component
		//	and all other nodes from the same component were already visited and are currently on the stack
		//	now record the strongly connected component by taking the nodes off the stack
		if (info_[node_index].colour == info_[node_index].lowlink)
		{
			int stack_node = stack_.back();

			//if a component of size greater than one is detected, register it
			if (stack_node != node_index)
			{
				//prepare space for the new component
				strongly_connected_components_.emplace_back();

				//nodes in the component are located on the stack, collect them
				while (stack_node != node_index)
				{
					pumpkin_assert_simple(info_[stack_node].is_on_stack, "Sanity check.");

					strongly_connected_components_.back().push_back(stack_node);

					stack_.pop_back();
					info_[stack_node].is_on_stack = false;

					pumpkin_assert_simple(!stack_.empty(), "Sanity check."); //there always has to be at least the current node on stack
					stack_node = stack_.back();
				}
				strongly_connected_components_.back().push_back(node_index);
			}
			stack_.pop_back(); //remove the node_index from the stack
			info_[node_index].is_on_stack = false;
		}
	}

	static const int UNDEFINED_COLOUR = -1;

	const AdjacencyListGraph* graph_;
	int num_nodes_visited_;
	std::vector<int> stack_;
	struct NodeInformation { bool already_visited; int colour; int lowlink; bool is_on_stack; };
	std::vector<NodeInformation> info_;
	std::vector<std::vector<int> > strongly_connected_components_;
};
}
}