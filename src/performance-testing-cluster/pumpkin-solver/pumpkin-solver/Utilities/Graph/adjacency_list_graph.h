#pragma once

#include "../pumpkin_assert.h"

#include <vector>
#include <string>
#include <fstream>
#include <set>
#include <algorithm>

namespace Pumpkin
{
namespace Graph
{
class CompactSubGraph;

class AdjacencyListGraph
{
public:
	AdjacencyListGraph(int num_nodes):adjacency_list_(num_nodes){}
	
	//todo: explain the format
	//	currently the input is transfered via this transform_index method, but this should be done cleaner
	AdjacencyListGraph(std::string file_name)
	{
		std::ifstream input_file(file_name);
		pumpkin_assert_permanent(input_file, "Error: input file could not be found!\n");
		auto transform_index = [](int a)
		{
			if (a > 0) { return 2 * a; }
			else if (a < 0) { return 2 * abs(a) + 1; }
			else { pumpkin_assert_permanent(0, "Error: zero index not expected!\n"); return 0; } //no 0 indices expected			
		};

		int n1, n2;
		while (input_file >> n1)
		{
			input_file >> n2;
			pumpkin_assert_simple(input_file, "Error: end of file reached unexpectedly.\n");

			/*int other_n1 = -n1;
			int other_n2 = -n2;

			n1 = transform_index(n1);
			n2 = transform_index(n2);

			other_n1 = transform_index(other_n1);
			other_n2 = transform_index(other_n2);*/

			AddNode(n1);
			AddNode(n2);
			
			AddNeighbour(n1, n2);

			/*bool implication_addition = true;
			if (implication_addition)
			{
				AddNode(other_n1);
				AddNode(other_n2);
				AddNeighbour(other_n2, other_n1);
			}*/
		}
	}
	
	void AddNode(int node_index)
	{
		if (node_index < adjacency_list_.size()) { return; }
		adjacency_list_.resize(node_index+1);
	}

	//bool AddNeighbour(int node1, int node2) { return adjacency_list_[node1].insert(node2).second; } //returns true if the edge was added, false if the edge already existed
	void AddNeighbour(int node1, int node2) { adjacency_list_[node1].push_back(node2); }
	
	int NumNodes() const { return adjacency_list_.size(); }

	int NumNeighbours(int node) const { return adjacency_list_[node].size(); }
	
	//const std::set<int>& GetNeighbours(int node_index) const { return adjacency_list_[node_index]; }
	const std::vector<int>& GetNeighbours(int node_index) const { return adjacency_list_[node_index]; }

	//also makes the neighbour indices sorted
	void RemoveDuplicateAndLoopEdgesForNode(int node_index)
	{
		std::vector<int>& v = adjacency_list_[node_index];

		if (v.empty()) { return; }

		std::sort(v.begin(), v.end());

		size_t i = 0;
		//move the 'i' to the first position that is different from the node index
		//	need to do this to avoid adding self-loops
		while (i < v.size() && v[i] == node_index) { ++i; }

		//if all elements are self-loops
		if (i == v.size())
		{
			v.clear();
			return;
		}
		//at this point, i is at an edge that is not a self-loop
		//we know that for sure this edge should be kept
		v[0] = v[i];
		++i;
		size_t new_size = 1;
		//iterate through the sorted list to add non-duplicates and non-self-loops
		while (i < v.size())
		{
			bool not_a_duplicate = v[i] != v[new_size - 1];
			bool not_a_self_loop = v[i] != node_index;
			if (not_a_duplicate && not_a_self_loop)
			{
				v[new_size] = v[i];
				++new_size;
			}
			++i;
		}
		v.resize(new_size);
	}

	//also makes the neighbour indices sorted
	void RemoveDuplicateAndLoopEdges()
	{
		//for (std::vector<int>& v : adjacency_list_)
		for(int node_index = 0; node_index < adjacency_list_.size(); node_index++)
		{
			RemoveDuplicateAndLoopEdgesForNode(node_index);
		}
	}

	//removes forbidden nodes from the graph, both outgoing and incoming edges to those nodes
	void RemoveEdgesWithNodes(std::vector<int>& forbidden_nodes)
	{
		std::vector<bool> is_forbidden_node(NumNodes(), false);
		for (int forbidden_node : forbidden_nodes) { is_forbidden_node[forbidden_node] = true; }

		//remove outgoing edges for the nodes
		for (int forbidden_node : forbidden_nodes) { adjacency_list_[forbidden_node].clear(); }
		//remove incoming edges for the nodes
		for (int node_index = 0; node_index < NumNodes(); node_index++)
		{
			int new_end = 0;
			for (int j = 0; j < NumNeighbours(node_index); j++)
			{
				int neighbour_node = adjacency_list_[node_index][j];
				if (!is_forbidden_node[neighbour_node])
				{
					adjacency_list_[node_index][new_end] = adjacency_list_[node_index][j];
					++new_end;
				}
			}
			adjacency_list_[node_index].resize(new_end);
		}
	}

	void RemoveOutgoingEdgesForNode(int node_index)
	{
		adjacency_list_[node_index].clear();
	}

	int NumIsolatedNodes() const
	{
		std::vector<bool> has_incoming_edge(NumNodes(), false);
		for (int node_index = 0; node_index < NumNodes(); node_index++)
		{
			for (int neighbour_index : GetNeighbours(node_index))
			{
				has_incoming_edge[neighbour_index] = true;
			}
		}
		int num_isolated_nodes = 0;
		for (int node_index = 0; node_index < NumNodes(); node_index++)
		{
			bool no_incoming_edges = !has_incoming_edge[node_index];
			bool no_neighbours = NumNeighbours(node_index) == 0;
			bool is_isolated = (no_incoming_edges && no_neighbours);
			num_isolated_nodes += is_isolated;
		}
		return num_isolated_nodes;
	}

	void PrintSubgraphToFile(std::set<int> allowed_nodes, std::string file_location)
	{
		std::ofstream output(file_location.c_str());
		for (int i = 0; i < adjacency_list_.size(); i++)
		{
			if (allowed_nodes.count(i) == 0) { continue; }
			for (int m : GetNeighbours(i))
			{
				if (allowed_nodes.count(m) == 0) { continue; }
				output << i << " " << m << '\n';
			}
		}
	}
	
	CompactSubGraph CreateCompactSubGraph(std::vector<int>& nodes) const;

	//slow method
	bool DoesPathExist(std::vector<int>& path) const
	{
		pumpkin_assert_simple(path.size() >= 2, "Error: paths are assumed to be at least two nodes.");

		for (int i = 0; i < path.size() - 1; i++)
		{
			if (!DoesEdgeExist(path[i], path[i+1])) { return false;}
		}
		return true;
	}

	//slow method
	bool DoesEdgeExist(int node1, int node2) const
	{
		for (int neighbour_id : GetNeighbours(node1))
		{
			if (neighbour_id == node2) { return true; }
		}
		return false;
	}

	//slow method
	bool DoPathsExist(std::vector<std::vector<int> >& paths) const
	{
		for (auto& p : paths)
		{
			if (!DoesPathExist(p))
			{
				return false;
			}
		}
		return true;
	}

private:
	static const int UNDEFINED_NODE = -1;

	//the set usage is a bit too slow
	//std::vector<std::set<int> > adjacency_list_;
	std::vector<std::vector<int> > adjacency_list_;
};
}
}