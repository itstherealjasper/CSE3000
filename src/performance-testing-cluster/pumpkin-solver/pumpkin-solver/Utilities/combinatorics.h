#pragma once

#include <vector>
#include <cstddef>  //includes size_t

namespace Pumpkin
{
class Combinatorics
{
public:
	static std::vector<std::vector<bool> > GenerateAllBooleanVectors(int vector_length);

private:
	static void GenerateAllBooleanVectorsInternal(size_t index, std::vector<bool>& current_solution, std::vector<std::vector<bool> >& solutions);
};
}