#pragma once

#include "adjacency_list_graph.h"
#include "../pumpkin_assert.h"

#include <vector>
#include <algorithm>
#include <set>

namespace Pumpkin
{
namespace Graph
{
class CliqueComputer
{
public:
	//computes a set of disjoint cliques from the graph
	//	there algorithm tries to find larger cliques but no guarantees are given, the algorithm is greedy with no backtracking
	//	there may be better ways to find cliques, todo
	//	the type of algorithm implemented below was used in the MaxSAT solver RC2 (see code of "RC2: an Efficient MaxSAT Solver" by A. Ignatiev, A. Morgado, and J. Marques-Silva) 
	std::vector<std::vector<int> > SolveManyCliquesGreedy(AdjacencyListGraph& graph)
	{
		struct PairNodeDegree { int node_index; int node_degree; PairNodeDegree() :node_index(-1), node_degree(-1) {}; PairNodeDegree(int index, int degree) :node_index(index), node_degree(degree) {} };
		std::vector<PairNodeDegree> sorted_nodes(graph.NumNodes()); //nodes will be sorted based on their degree, smaller degrees first
		for (int node_index = 0; node_index < graph.NumNodes(); node_index++)
		{
			sorted_nodes[node_index].node_index = node_index;
			sorted_nodes[node_index].node_degree = graph.NumNeighbours(node_index);
		}
		std::sort(sorted_nodes.begin(), sorted_nodes.end(), [](PairNodeDegree& p1, PairNodeDegree& p2)->bool { return p1.node_degree < p2.node_degree; });

		std::vector<bool> is_node_available(graph.NumNodes(), true);
		std::vector<PairNodeDegree> neighbouring_nodes;
		std::vector<std::vector<int> > cliques;
		std::vector<int> current_clique;
		//iterate through the nodes in increasing node degree
		//	and try to form cliques

		for (PairNodeDegree& p_initial : sorted_nodes)
		{
			if (!is_node_available[p_initial.node_index]) { continue; }

			//create a vector of neighbouring nodes sorted by degree
			neighbouring_nodes.clear();
			for (int neighbour_index : graph.GetNeighbours(p_initial.node_index))
			{
				if (is_node_available[neighbour_index])
				{
					neighbouring_nodes.emplace_back(neighbour_index, graph.NumNeighbours(neighbour_index));
				}
			}
			//skip this node if no neighbours are available
			if (neighbouring_nodes.empty()) { continue; }

			std::sort(neighbouring_nodes.begin(), neighbouring_nodes.end(), [](PairNodeDegree& p1, PairNodeDegree& p2)->bool { return p1.node_degree < p2.node_degree; });
			current_clique.clear();
			current_clique.push_back(p_initial.node_index);
			//the clique initially contain a single node
			//	try to add as many neighbours as possible while still remaining a clique
			for (PairNodeDegree p_candidate : neighbouring_nodes)
			{
				if (!is_node_available[p_candidate.node_index]) { continue; }

				//a node can be added to the clique if it has connection to all nodes in the clique and vice versa
				//here we implicitly assume that the graph is correctly given as an undirected graph, i.e., (a, b), then (b, a)
				bool can_be_added_to_clique = true;
				for (int i = 1; 1 < current_clique.size(); i++)
				{
					int clique_node = current_clique[i];									
					if (!graph.DoesEdgeExist(clique_node, p_candidate.node_index))
					{
						can_be_added_to_clique = false;
						break;
					}
				}
				
				if (can_be_added_to_clique)
				{
					current_clique.push_back(p_candidate.node_index);
				}
			}
			//register clique if it is non-trivial
			if (current_clique.size() >= 2)
			{
				for (int clique_node : current_clique)
				{
					pumpkin_assert_simple(is_node_available[clique_node], "Sanity check.");
					is_node_available[clique_node] = false;
				}
				cliques.push_back(current_clique);
			}
		}
		pumpkin_assert_simple(CheckCliques(cliques, graph), "Sanity check.");
		return cliques;
	}

private:
	bool CheckCliques(std::vector<std::vector<int> >& cliques, AdjacencyListGraph& graph) 
	{ 
		std::cout << "c checking if cliques are really correct...\n";
		for (auto& clique : cliques)
		{
			for (int i = 0; i < clique.size(); i++)
			{
				int node1 = clique[i];
				for (int j = i+1; j < clique.size(); j++)
				{
					int node2 = clique[j];
					if (!graph.DoesEdgeExist(node1, node2)) { return false; }
				}
			}
		}

		//check if cliques are nonoverlapping
		std::vector<bool> has_been_seen(graph.NumNodes(), false);
		for (auto& clique : cliques)
		{
			for (int clique_lit: clique)
			{
				if (has_been_seen[clique_lit]) { return false; }

				has_been_seen[clique_lit] = true;
			}
		}
		return true; 
	}
};
}
}