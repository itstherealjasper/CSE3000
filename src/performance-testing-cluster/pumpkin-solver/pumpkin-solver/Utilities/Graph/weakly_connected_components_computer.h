#pragma once

#include "adjacency_list_graph.h"
#include "../union_find_data_structure.h"
#include "../pumpkin_assert.h"

#include <vector>

namespace Pumpkin
{
namespace Graph
{
class WeaklyConnectedComponentsComputer
{
public:

	//returns a vector of vectors v, where v[i] is the vector of nodes that compose a weakly connected component
	//each component has size greater than one (non-trivial)
	std::vector<std::vector<int> > Solve(const AdjacencyListGraph& graph)
	{
		UnionFindDataStructure union_find(graph.NumNodes());
		//merge nodes with their neighbours
		for (int node_index = 0; node_index < graph.NumNodes(); node_index++)
		{
			for (int neighbour_index : graph.GetNeighbours(node_index))
			{
				union_find.Merge(node_index, neighbour_index);
			}
		}
		//create a mapping from the representation id to its new id
		//	the new id ranges from [0, num_components), whereas representative ids can take any value from [0, num_nodes)
		//	note that we only consider non-trivial components, i.e., components with at least two nodes
		std::vector<int> representative_id_to_new_id(graph.NumNodes(), -1);
		int num_nontrivial_components = 0;
		for (int node_index = 0; node_index < graph.NumNodes(); node_index++)
		{
			if (union_find.IsRepresentative(node_index) && union_find.NumEquivalentIDs(node_index) > 1) 
			{ 
				representative_id_to_new_id[node_index] = num_nontrivial_components;
				++num_nontrivial_components;
			}
		}
		//extract the components
		//	each node will go into its own component, which is determined by the new id of its representative
		std::vector<std::vector<int> > weakly_connected_components(num_nontrivial_components);
		for (int node_index = 0; node_index < graph.NumNodes(); node_index++)
		{
			int rep_id = union_find.GetRepresentative(node_index);
			int new_rep_id = representative_id_to_new_id[rep_id];
			if (new_rep_id != -1)
			{
				weakly_connected_components[new_rep_id].push_back(node_index);
			}
		}
		return weakly_connected_components;
	}
};
}
}
