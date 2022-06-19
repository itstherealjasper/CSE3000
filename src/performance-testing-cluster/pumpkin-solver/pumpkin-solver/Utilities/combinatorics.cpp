#include "combinatorics.h"

namespace Pumpkin
{
std::vector<std::vector<bool>> Combinatorics::GenerateAllBooleanVectors(int vector_length)
{
    std::vector<std::vector<bool> > solutions;
    std::vector<bool> auxiliary_vector(vector_length);
    GenerateAllBooleanVectorsInternal(size_t(0), auxiliary_vector, solutions); //this methods fill in solutions
    return solutions;
}

void Combinatorics::GenerateAllBooleanVectorsInternal(size_t index, std::vector<bool>& current_solution, std::vector<std::vector<bool>>& solutions)
{
    if (index >= current_solution.size())
    {
        solutions.push_back(current_solution);
    }
    else
    {
        current_solution[index] = false;
        GenerateAllBooleanVectorsInternal(index + 1, current_solution, solutions);
        current_solution[index] = true;
        GenerateAllBooleanVectorsInternal(index + 1, current_solution, solutions);
    }
}

}