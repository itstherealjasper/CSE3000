#include "encoder_totaliser.h"
#include "../Utilities/runtime_assert.h"

#include <iostream>
#include "encoder_cardinality_network.h"

namespace Pumpkin
{
std::vector<BooleanLiteral> EncoderTotaliser::SoftLessOrEqual(std::vector<BooleanLiteral>& input_literals, int right_hand_side, SolverState& state)
{
    runtime_assert(right_hand_side >= 0 && input_literals.size() >= right_hand_side);
    num_clauses_added_ = 0;

    if (input_literals.size() <= right_hand_side) { return std::vector<BooleanLiteral>(); }

    //the totaliser encoding can be visualised as a binary tree
    //  the leaf nodes are the input literals
    //  each node above the leafs represents the sum of two child nodes, where the sum is represented as a unary number using literals
    //  the final/root layer contains the sum of all input variables
    //  at the leaf layer, each node contains one literal
    //  each layer has half of the number of nodes as the previous layer, but each node is twice the size

    //  note that each layer has |input_literals| literals in total, distributed equally among the nodes
    //  for this reason, we represents all literals in a single vector (current_layer and next_layer are used to hold the literals)

    //  the input literals are used to initialise the current layer

    std::vector<BooleanLiteral>* current_layer = &helper1_, * next_layer = &helper2_;
    int num_literals_per_layer = input_literals.size();

    *current_layer = input_literals;
    next_layer->resize(num_literals_per_layer);
   
    //  in each iteration, the literals of the next_layer are created and appropriate clauses are added to capture the sum.
    //  The size of a node is the maximum number of literals in the node. 
    //  Initially the max_node_size is one, and in each iteration it doubles by merging two nodes.
    for (int max_node_size = 1; max_node_size < input_literals.size(); max_node_size *= 2)
    {
        int num_nodes = (num_literals_per_layer / max_node_size) + ((num_literals_per_layer % max_node_size) > 0);
        int size_of_unmerging_node = (num_nodes % 2 != 0)*(num_literals_per_layer - (num_nodes-1)*max_node_size); //if there is an odd number of nodes, the last one will not be considered in this iteration. Note that it is guaranteed that num_nodes-1 nodes will have max_node_size, and the last node may or may not have a different size
        int num_new_variables = num_literals_per_layer - size_of_unmerging_node;        
        
        //create new variables as appropriate
        for (int i = 0; i < num_new_variables; i++) 
        { 
            IntegerVariable integer_variable = state.CreateNewIntegerVariable(0, 1);
            BooleanLiteral literal = state.GetEqualityLiteral(integer_variable, 1);
            next_layer->at(i) = literal;
        }

        //  the unmerged nodes are simply kept for the next iteration
        for (int i = num_new_variables; i < num_literals_per_layer; i++) { next_layer->at(i) = current_layer->at(i); }

        //neighbouring nodes of the current layer are merged and their sum is represented in their parent node that is stored in the next layer
        //  we merge the first and the second node (merge_index = 0), then the third and the fourth node (merge_index = 1), and so on
        for (int merge_index = 0; merge_index < num_nodes / 2; merge_index++)
        {
            //node1_start and node2_start are the indicies of the starting locations of two nodes that are going to be merged
            int node1_start = (2 * merge_index) * max_node_size;
            int node2_start = (2 * merge_index + 1) * max_node_size;
            int sum_node_start = node1_start; //the sum_node is the node that is the result of merging two nodes (it stores the sum of the two nodes in unary form)
            //n1_i -> s_i
            for (int i = 1; i <= max_node_size; i++)
            {
                int node1_index = node1_start + i - 1;
                
                num_clauses_added_++;
                state.propagator_clausal_.AddBinaryClause(~current_layer->at(node1_index), next_layer->at(sum_node_start + i - 1));
            }
            //n2_j -> s_j
            for (int j = 1; j <= max_node_size; j++)
            {
                int node2_index = node2_start + j - 1;
                if (node2_index >= num_literals_per_layer) { break; } //in some cases the last node might not be of size 'max_node-size', e.g., if there are three input literals, the second iteration will need to merge a node of size two with a node of size one
                num_clauses_added_++;
                state.propagator_clausal_.AddBinaryClause(~current_layer->at(node2_index), next_layer->at(sum_node_start + j - 1));
            }
            //n1_i and n2_j -> s_{i+j}
            for (int i = 1; i <= max_node_size; i++)
            {
                int node1_index = node1_start + i - 1;
                for (int j = 1; j <= max_node_size; j++)
                {
                    int node2_index = node2_start + j - 1;
                    if (node2_index >= num_literals_per_layer) { break; } //in some cases the last node might not be of size 'max_node-size', e.g., if there are three input literals, the second iteration will need to merge a node of size two with a node of size one
                    num_clauses_added_++;
                    state.propagator_clausal_.AddTernaryClause(~current_layer->at(node1_index), ~current_layer->at(node2_index), next_layer->at(sum_node_start + i + j - 1));
                }
            }
            
            /*for (int i = 1; i <= max_node_size; i++)
            {
                int node1_index = node1_start + i - 1;
                int sum_node_start = node1_start; //the sum_node is the node that is the result of merging two nodes (it stores the sum of the two nodes in unary form)
                //n1_i -> s_i
                num_clauses_added_++;
                state.propagator_clausal_.AddBinaryClause(~current_layer->at(node1_index), next_layer->at(sum_node_start + i - 1));
                for (int j = 1; j <= max_node_size; j++)
                {
                    int node2_index = node2_start + j - 1;
                    if (node2_index >= num_literals_per_layer) { break; } //in some cases the last node might not be of size 'max_node-size', e.g., if there are three input literals, the second iteration will need to merge a node of size two with a node of size one
                    //n2_j -> s_j
                    num_clauses_added_++;
                    state.propagator_clausal_.AddBinaryClause(~current_layer->at(node2_index), next_layer->at(sum_node_start + j - 1));
                    //n1_i and n2_j -> s_{i+j}
                    num_clauses_added_++;
                    state.propagator_clausal_.AddTernaryClause(~current_layer->at(node1_index), ~current_layer->at(node2_index), next_layer->at(sum_node_start + i + j - 1));
                }
            }*/            
        }
        //the next layer has been successfully encoded, now use it as the current layer and iterate
        std::swap(current_layer, next_layer);
    }

    //current_layer now stores the final sum literals
    //  i.e., if i+1 input literals are set to true, then current_layer[i] is true
    //  note that the converse does not hold in the current version
    
    //the first 'right_hand_side' number of literals in current_layer are not restricted by the constraint
    //the remaining literals are the ones capture violations
    //  only these will be inserted into the output_literals vector

    std::vector<BooleanLiteral>& output_literals = *next_layer;
    output_literals.resize(input_literals.size() - right_hand_side);
    for (int i = 0; i < output_literals.size(); i++) { output_literals[i] = current_layer->at(i + right_hand_side); }    

    for (int i = 0; i < output_literals.size() - 1; i++)
    {
        state.propagator_clausal_.AddBinaryClause(output_literals[i], ~output_literals[i + 1]);
    }

    //store the result
    encoded_constraints_.push_back(EncodedConstraint());
    encoded_constraints_.back().input_literals = input_literals;
    encoded_constraints_.back().output_literals = output_literals;
    encoded_constraints_.back().right_hand_side = right_hand_side;

    return output_literals;
}

int64_t EncoderTotaliser::NumClausesAddedLastTime() const
{
    return num_clauses_added_;
}

bool EncoderTotaliser::DebugCheckSatisfactionOfEncodedConstraints(const IntegerAssignmentVector& solution, SolverState& state)
{
    int64_t sum_of_cores = 0;
    int64_t sum_of_falsified = 0;
    for (EncodedConstraint &encoded_constraint: encoded_constraints_)
    {
        sum_of_cores += encoded_constraint.input_literals.size();

        int num_falsified = 0;
        for (BooleanLiteral core_lit : encoded_constraint.input_literals)
        {
            auto& lit_info = state.GetLiteralInformation(core_lit);
            //if (core_lit.IsPositive())
            if (solution.IsSatisfied(lit_info.integer_variable, lit_info.operation, lit_info.right_hand_side))
            {
                //num_falsified += (solution[core_lit.VariableIndex()] == true);
                num_falsified++;
            }
            /*else
            {
                num_falsified += (solution[core_lit.VariableIndex()] == false);
            }*/
        }
        sum_of_falsified += num_falsified;
        runtime_assert(num_falsified >= 1); //has to be at least one unsat from the core

        for (int k = 0; k < num_falsified - encoded_constraint.right_hand_side; k++)
        {
            BooleanLiteral lit = encoded_constraint.output_literals[k];
            auto lit_info = state.GetLiteralInformation(lit);
            runtime_assert(solution.IsSatisfied(lit_info.integer_variable, lit_info.operation, lit_info.right_hand_side));
            //runtime_assert(solution[lit] == true)            
        }

        //actually not sure...true, but since assumptions are set to false, then it should not be the case that the literal is set to true if not necessary...but actually it could be the case if the next literal has been removed from this current iteration...todo explain this and test
        //this only makes sense if the constraint encodes the equivalence, but usually we do not
       // for (int k = num_falsified - encoded_constraint.right_hand_side; k < encoded_constraint.output_literals.size(); k++)
        //{
        //    BooleanLiteral lit = encoded_constraint.output_literals[k];
        //    runtime_assert(solution[lit] == false);
        //}
    }
    std::cout << "c avg core size: " << double(sum_of_cores) / encoded_constraints_.size() << "\n";
    std::cout << "c avg falsification: " << double(sum_of_falsified) / encoded_constraints_.size() << "\n";
    return true;
}

/*
bool LowerBoundSearch::DebugLiteralsCheck(SolverOutput& output)
{

}
*/

}