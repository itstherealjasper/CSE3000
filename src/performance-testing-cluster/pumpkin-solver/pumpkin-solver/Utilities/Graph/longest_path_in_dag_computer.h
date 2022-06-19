#pragma once

#include "compact_subgraph.h"
#include "adjacency_list_graph.h"
#include "topological_sort_computer.h"
#include "weakly_connected_components_computer.h"
#include "../pumpkin_assert.h"

#include <vector>
#include <algorithm>
#include <set>

namespace Pumpkin
{
namespace Graph
{
//currently only works for unweighted graphs, easy to extend for weighted though
//assumes the input graph is a dag...
class LongestPathInDAGComputer
{
public:
	std::vector<int> Solve(const AdjacencyListGraph& graph)
	{
		Initialise(graph);

		if (num_isolated_nodes_ == graph.NumNodes()) { return std::vector<int>(); }

		ComputeDynamicProgrammingTable();

		int max_distance = INT32_MIN;
		int max_node = UNDEFINED_NODE;
		for(int node_index = 0; node_index < graph.NumNodes(); node_index++)
		{
			if (table_[node_index].longest_distance > max_distance)
			{
				max_distance = table_[node_index].longest_distance;
				max_node = node_index;
			}
		}

		if (max_distance == 0) { return std::vector<int>(); } //trivial solutions ignored

		pumpkin_assert_simple(!table_[max_node].is_starting_node, "Nontrivial solutions cannot contain a starting node.");
		int current_node = max_node;
		std::vector<int> path;
		while (!table_[current_node].is_starting_node)
		{
			path.push_back(current_node);
			current_node = table_[current_node].previous_node;
		}
		path.push_back(current_node);
		std::reverse(path.begin(), path.end());

		pumpkin_assert_advanced(graph.DoesPathExist(path), "Sanity check.");
		return path;
	}

	//tries to extract multiple longest path lengths
	std::vector<std::vector<int> > SolveGreedyLongestPaths(const AdjacencyListGraph& graph)
	{
		Initialise(graph);

		if (num_isolated_nodes_ == graph.NumNodes()) { return std::vector<std::vector<int> >(); }

		ComputeDynamicProgrammingTable();

		int max_distance = INT32_MIN;
		int max_node = UNDEFINED_NODE;
		for (int node_index = 0; node_index < graph.NumNodes(); node_index++)
		{
			if (table_[node_index].longest_distance > max_distance)
			{
				max_distance = table_[node_index].longest_distance;
				max_node = node_index;
			}
		}

		if (max_distance == 0) { return std::vector<std::vector<int> >(); } //trivial solutions ignored

		static std::vector<int> max_distance_nodes;
		max_distance_nodes.clear();

		for (int node_index = 0; node_index < graph.NumNodes(); node_index++)
		{
			if (table_[node_index].longest_distance == max_distance)
			{
				max_distance_nodes.push_back(node_index);
			}		
		}

		//max_distance_nodes -> set of a nodes that all have distance of d

		static std::vector<bool> is_node_unused;
		is_node_unused.clear();
		is_node_unused.resize(graph_->NumNodes(), true);

		std::vector<std::vector<int> > all_paths;
		static std::vector<int> path;
		for (int max_node : max_distance_nodes)
		{
			pumpkin_assert_simple(!table_[max_node].is_starting_node, "Nontrivial solutions cannot contain a starting node.");

			int current_node = max_node;
			path.clear();
			while (!table_[current_node].is_starting_node && is_node_unused[current_node])
			{
				path.push_back(current_node);
				current_node = table_[current_node].previous_node;
			}

			if (is_node_unused[current_node])
			{
				path.push_back(current_node);
				pumpkin_assert_simple(path.size() == max_distance + 1, "Sanity check.");
				std::reverse(path.begin(), path.end());
				for (int node : path) { is_node_unused[node] = false; }
				pumpkin_assert_advanced(graph.DoesPathExist(path), "Path returned that does not exist in the graph.");
				all_paths.push_back(path);
			}			
		}	
		
		pumpkin_assert_advanced(graph.DoPathsExist(all_paths), "Sanity check.");
		pumpkin_assert_advanced(DebugPathsDoNotOverall(all_paths), "Sanity check.");
		return all_paths;
	}

	static std::vector<std::vector<int> > SolveGreedyAllLongestPaths_BruteForce(const AdjacencyListGraph& graph)
	{
		AdjacencyListGraph copy_graph = graph;

		std::vector<std::vector<int> > paths;
		LongestPathInDAGComputer computer;
		
		while (1 == 1)
		{
			std::vector<int> path = computer.Solve(copy_graph);
			if (path.size() <= 1) { break; }

			paths.push_back(path);
			copy_graph.RemoveEdgesWithNodes(path);
		}
		pumpkin_assert_advanced(graph.DoPathsExist(paths), "Sanity check.");
		return paths;
	}

	static std::vector<std::vector<int> > SolveGreedyAllLongestPaths_AllMaxPathsMethods(const AdjacencyListGraph& graph)
	{
		AdjacencyListGraph copy_graph = graph;

		std::vector<std::vector<int> > all_paths;
		std::vector<int> nodes_to_remove;
		LongestPathInDAGComputer computer;

		while (1 == 1)
		{
			std::vector<std::vector<int> > paths = computer.SolveGreedyLongestPaths(copy_graph);

			if (paths.empty()) { break; }

			if (paths[0].size() <= 1) { break; } //I think paths of length 1 are never returned, it would then return empty

			nodes_to_remove.clear();
			for (auto& p : paths)
			{
				all_paths.push_back(p);
				for (int n : p) { nodes_to_remove.push_back(n); }
			}
			copy_graph.RemoveEdgesWithNodes(nodes_to_remove);
		}
		pumpkin_assert_advanced(graph.DoPathsExist(all_paths), "Sanity check.");
		return all_paths;
	}

	static std::vector<std::vector<int> > SolveGreedyAllLongestPaths_DecomposeConnectedComponentsBruteForce(const AdjacencyListGraph& graph)
	{
		//decompose the graph into its non-trivial connected components
		//	then we can enumerate the longest paths for each component independently
		//	a positive side effect of this method is that it effectively removes isolated nodes from the graph
		//	currently graphs are copied, but creating a view of the graph would be better, todo...

		std::vector<std::vector<int> > all_paths;
		WeaklyConnectedComponentsComputer wcc_computer;
		std::vector<std::vector<int> > weakly_connected_components = wcc_computer.Solve(graph);

		for (std::vector<int>& component : weakly_connected_components)
		{
			CompactSubGraph compact_subgraph = graph.CreateCompactSubGraph(component);
			std::vector<std::vector<int> > paths = SolveGreedyAllLongestPaths_BruteForce(compact_subgraph.graph);
			//	in case it takes too long, we could prioritise the order in which we search, but for now we just go in order assuming the time is negligible
			
			auto rename_ids = [](std::vector<int>& path, std::vector<int>& new_to_old)
			{
				for (int i = 0; i < path.size(); i++)
				{
					path[i] = new_to_old[path[i]];
				}
			};

			for (int i = 0; i < paths.size(); i++)
			{
				rename_ids(paths[i], compact_subgraph.mapping_subgraph_node_to_old_node);
				all_paths.push_back(paths[i]);
			}
		}

		pumpkin_assert_advanced(graph.DoPathsExist(all_paths), "Sanity check.");
		return all_paths;
	}


	static std::vector<std::vector<int> > SolveGreedyAllLongestPaths_DecomposeConnectedComponents_AllPathsMethod(const AdjacencyListGraph& graph)
	{
		//decompose the graph into its non-trivial connected components
		//	then we can enumerate the longest paths for each component independently
		//	a positive side effect of this method is that it effectively removes isolated nodes from the graph
		//	currently graphs are copied, but creating a view of the graph would be better, todo...

		std::vector<std::vector<int> > all_paths;
		WeaklyConnectedComponentsComputer wcc_computer;
		std::vector<std::vector<int> > weakly_connected_components = wcc_computer.Solve(graph);

		for (std::vector<int>& component : weakly_connected_components)
		{
			CompactSubGraph compact_subgraph = graph.CreateCompactSubGraph(component);
			std::vector<std::vector<int> > paths = SolveGreedyAllLongestPaths_AllMaxPathsMethods(compact_subgraph.graph);
			//	in case it takes too long, we could prioritise the order in which we search, but for now we just go in order assuming the time is negligible

			auto rename_ids = [](std::vector<int>& path, std::vector<int>& new_to_old)
			{
				for (int i = 0; i < path.size(); i++)
				{
					path[i] = new_to_old[path[i]];
				}
			};

			for (int i = 0; i < paths.size(); i++)
			{
				rename_ids(paths[i], compact_subgraph.mapping_subgraph_node_to_old_node);
				all_paths.push_back(paths[i]);
			}
		}
		pumpkin_assert_advanced(graph.DoPathsExist(all_paths), "Sanity check.");
		return all_paths;
	}

private:

	void Initialise(const AdjacencyListGraph& graph)
	{
		graph_ = &graph;
		table_.resize(graph.NumNodes());
		for (int i = 0; i < graph.NumNodes(); i++)
		{
			table_[i].longest_distance = INT32_MIN;
			table_[i].previous_node = UNDEFINED_NODE;
			table_[i].is_starting_node = false;
			table_[i].is_isolated_node = false;
		}
		InitialiseNodesWithNoIncomingEdgeAndIsolatedNodeInfo();
	}

	void ComputeDynamicProgrammingTable()
	{
		for (int starting_nodes : nonisolated_nodes_with_no_incoming_edge_)
		{
			table_[starting_nodes].longest_distance = 0;
			table_[starting_nodes].previous_node = UNDEFINED_NODE;
			table_[starting_nodes].is_starting_node = true;
		}
		
		std::vector<int> sorted_nodes = topological_sorter_.Solve(*graph_);
		//isolated nodes participate in the solving, but are not useful, could consider removing these nodes from the graph
		
		for (int node_index : sorted_nodes)
		{
			if (table_[node_index].is_isolated_node) { continue; }

			pumpkin_assert_simple(table_[node_index].longest_distance != INT32_MIN, "Cannot be that the node is being visited but it has infinite distance.");
			int new_distance = table_[node_index].longest_distance + 1; //distance to the neighbours will be plus one since we only consider unweighted graphs
			
			for (int neighbour_index : graph_->GetNeighbours(node_index))
			{
				auto& neighbour_info = table_[neighbour_index];

				int current_distance = neighbour_info.longest_distance;
				
				if (new_distance > current_distance)
				{
					neighbour_info.longest_distance = new_distance;
					neighbour_info.previous_node = node_index;
				}
			}
		}		
	}

	void InitialiseNodesWithNoIncomingEdgeAndIsolatedNodeInfo()
	{
		num_isolated_nodes_ = 0;
		has_incoming_edge_ = std::vector<bool>(graph_->NumNodes(), false);
		
		for (int node_index = 0; node_index < graph_->NumNodes(); node_index++)
		{
			for (int neighbour_index : graph_->GetNeighbours(node_index))
			{
				has_incoming_edge_[neighbour_index] = true;
			}
		}
		nonisolated_nodes_with_no_incoming_edge_.clear();
		for (int node_index = 0; node_index < graph_->NumNodes(); node_index++)
		{
			bool no_incoming_edges = !has_incoming_edge_[node_index];
			bool has_neighbours = graph_->NumNeighbours(node_index) > 0;

			if (no_incoming_edges && has_neighbours)
			{
				nonisolated_nodes_with_no_incoming_edge_.push_back(node_index);
			}			
			else if (no_incoming_edges && !has_neighbours)
			{
				table_[node_index].is_isolated_node = true;
				++num_isolated_nodes_;
			}
		}
	}

	bool DebugPathsDoNotOverall(std::vector<std::vector<int> >& paths) const
	{
		std::set<int> nodes;
		for (auto& p : paths)
		{
			for (int node : p)
			{
				if (nodes.count(node) != 0) { return false; }
				nodes.insert(node);
			}
		}
		return true;
	}

	static const int UNDEFINED_NODE = -1;

	const AdjacencyListGraph* graph_;
	TopologicalSortComputer topological_sorter_;
	std::vector<bool> has_incoming_edge_;
	std::vector<int> nonisolated_nodes_with_no_incoming_edge_;
	int num_isolated_nodes_;
	struct Entry { int longest_distance; int previous_node; bool is_starting_node; bool is_isolated_node; };
	std::vector<Entry> table_;
};
}
}