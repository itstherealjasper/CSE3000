#include "encoder_cardinality_network.h"
#include "../Utilities/stopwatch.h"

namespace Pumpkin
{
EncoderCardinalityNetwork::EncoderCardinalityNetwork() :
    num_clauses_added_last_time_(0),
    total_num_clauses_added_(0),
    total_time_spent_(0)
{
}
   
std::vector<BooleanLiteral> EncoderCardinalityNetwork::SoftLessOrEqual(std::vector<BooleanLiteral>& input_literals, int right_hand_side, SolverState& state)
{
    runtime_assert(right_hand_side >= 0 && input_literals.size() >= right_hand_side);
    num_clauses_added_last_time_ = 0;
    Stopwatch stopwatch;

    if (input_literals.size() <= right_hand_side) { return std::vector<BooleanLiteral>(); }

    std::vector<BooleanLiteral> sorted_literals = HSort(input_literals, state);
    
    std::vector<BooleanLiteral> output_literals(sorted_literals.begin()+right_hand_side, sorted_literals.end());

    //store the result
    encoded_constraints_.push_back(EncodedConstraint());
    encoded_constraints_.back().input_literals = input_literals;
    encoded_constraints_.back().output_literals = output_literals;
    encoded_constraints_.back().right_hand_side = right_hand_side;

    total_num_clauses_added_ += num_clauses_added_last_time_;
    total_time_spent_ += stopwatch.TimeElapsedInSeconds();

    return output_literals;
}

int64_t EncoderCardinalityNetwork::NumClausesAddedLastTime() const
{
    return num_clauses_added_last_time_;
}

int64_t EncoderCardinalityNetwork::TotalNumClausesAdded() const
{
    return total_num_clauses_added_;
}

int64_t EncoderCardinalityNetwork::TotalTimeSpent() const
{
    return total_time_spent_;
}

std::vector<BooleanLiteral> EncoderCardinalityNetwork::HSort(const std::vector<BooleanLiteral>& literals, SolverState& state)
{
    runtime_assert(!literals.empty());

    if (literals.size() == 1){ return literals; }
    
    if (literals.size() == 2) { return HMerge(literals[0], literals[1], state); }

    size_t size_half = literals.size()/2;
    std::vector<BooleanLiteral> sorted_first_half = HSort(std::vector<BooleanLiteral>(literals.begin(), literals.begin()+size_half), state);
    std::vector<BooleanLiteral> sorted_second_half = HSort(std::vector<BooleanLiteral>(literals.begin()+size_half, literals.end()), state);
    return HMerge(sorted_first_half, sorted_second_half, state);
}

std::vector<BooleanLiteral> EncoderCardinalityNetwork::HMerge(const std::vector<BooleanLiteral>& a, const std::vector<BooleanLiteral>& b, SolverState& state)
{
    runtime_assert(!b.empty());
    runtime_assert(b.size() >= a.size());
    runtime_assert(b.size() - a.size() <= 1);
    
    if (a.size() == 1 && b.size() == 1) { return HMerge(a[0], b[0], state); }
    
    if (a.empty()) { return b; }

    std::vector<BooleanLiteral> odd_sequence_a, odd_sequence_b, even_sequence_a, even_sequence_b;
    for (int i = 0; i < a.size(); i += 2) { odd_sequence_a.push_back(a[i]); }
    for (int i = 1; i < a.size(); i += 2) { even_sequence_a.push_back(a[i]); }
    for (int i = 0; i < b.size(); i += 2) { odd_sequence_b.push_back(b[i]); }
    for (int i = 1; i < b.size(); i += 2) { even_sequence_b.push_back(b[i]); }

    std::vector<BooleanLiteral> d = HMerge(odd_sequence_a, odd_sequence_b, state);
    std::vector<BooleanLiteral> e = HMerge(even_sequence_a, even_sequence_b, state);
    runtime_assert(d.size() + e.size() == a.size() + b.size());

    std::vector<BooleanLiteral> c(a.size() + b.size());
    for (size_t i = 1; i < c.size()-1; i++) 
    { 
        IntegerVariable integer_variable = state.CreateNewIntegerVariable(0, 1);
        c[i] = state.GetEqualityLiteral(integer_variable, 1); 
    }
    c[0] = d[0];
    
    if (2 * e.size() == c.size()) { c.back() = e.back(); }
    else 
    { 
        IntegerVariable integer_variable = state.CreateNewIntegerVariable(0, 1);
        c.back() = state.GetEqualityLiteral(integer_variable, 1);
    }    

    for (size_t i = 1; i < d.size(); i++) 
    { 
        num_clauses_added_last_time_++; 
        bool conflict_detected = state.propagator_clausal_.AddImplication(d[i + 1 - 1], c[2 * i - 1]);
        runtime_assert(!conflict_detected);
    }

    for (size_t i = 1; i < e.size(); i++)
    { 
        num_clauses_added_last_time_++; 
        bool conflict_detected = state.propagator_clausal_.AddImplication(e[i - 1], c[2 * i - 1]);
        runtime_assert(!conflict_detected);
    }

    if (2 * e.size() != c.size())
    {
        num_clauses_added_last_time_++;
        size_t m = e.size();
        bool conflict_detected = state.propagator_clausal_.AddImplication(e[m - 1], c[2 * m - 1]);
        runtime_assert(!conflict_detected);
    }

    size_t end_index = std::min(d.size(), e.size());
    for (size_t i = 1; i < end_index; i++)
    {
        num_clauses_added_last_time_++;
        bool conflict_detected = state.propagator_clausal_.AddTernaryClause(~d[i + 1 - 1], ~e[i - 1], c[2 * i + 1 - 1]);
        runtime_assert(!conflict_detected);
    }
    if (2 * e.size() != c.size())
    {
        num_clauses_added_last_time_++;
        size_t m = e.size();
        bool conflict_detected = state.propagator_clausal_.AddTernaryClause(~d[m + 1 - 1], ~e[m - 1], c[2 * m + 1 - 1]);
        runtime_assert(!conflict_detected);
    }

    return c;
}

std::vector<BooleanLiteral> EncoderCardinalityNetwork::HMerge(BooleanLiteral a, BooleanLiteral b, SolverState& state)
{
    IntegerVariable integer_variable1 = state.CreateNewIntegerVariable(0, 1);
    IntegerVariable integer_variable2 = state.CreateNewIntegerVariable(0, 1);

    BooleanLiteral sum_at_least_one(state.GetEqualityLiteral(integer_variable1, 1));
    BooleanLiteral sum_at_least_two(state.GetEqualityLiteral(integer_variable2, 1));

    bool conflict_detected = state.propagator_clausal_.AddImplication(a, sum_at_least_one);
    runtime_assert(!conflict_detected);
    conflict_detected = state.propagator_clausal_.AddImplication(b, sum_at_least_one);
    runtime_assert(!conflict_detected);
    conflict_detected = state.propagator_clausal_.AddTernaryClause(~a, ~b, sum_at_least_two);
    runtime_assert(!conflict_detected);
    num_clauses_added_last_time_ += 3;
    
    std::vector<BooleanLiteral> ret;
    ret.push_back(sum_at_least_one);
    ret.push_back(sum_at_least_two);    
    return ret;
}


    /*std::vector<BooleanLiteral> EncoderCardinalityNetwork::SoftLessOrEqual(std::vector<BooleanLiteral>& input_literals, int right_hand_side, SolverState& state)
{
    runtime_assert(right_hand_side >= 0 && input_literals.size() >= right_hand_side);

    if (input_literals.size() <= right_hand_side) { return std::vector<BooleanLiteral>(); }

    //the cardinality network encoding done here can be visualised as a binary tree
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
        int size_of_unmerging_node = (num_nodes % 2 != 0) * (num_literals_per_layer - (num_nodes - 1) * max_node_size); //if there is an odd number of nodes, the last one will not be considered in this iteration. Note that it is guaranteed that num_nodes-1 nodes will have max_node_size, and the last node may or may not have a different size
        int num_new_variables = num_literals_per_layer - size_of_unmerging_node;

        //create new variables as appropriate
        for (int i = 0; i < num_new_variables; i++) { next_layer->at(i) = BooleanLiteral(state.CreateNewVariable(), true); }
        //  the unmerged nodes are simply kept for the next iteration
        for (int i = num_new_variables; i < num_literals_per_layer; i++) { next_layer->at(i) = current_layer->at(i); }

        //neighbouring nodes of the current layer are merged and their sum is represented in their parent node that is stored in the next layer
        //  we merge the first and the second node (merge_index = 0), then the third and the fourth node (merge_index = 1), and so on
        for (int merge_index = 0; merge_index < num_nodes / 2; merge_index++)
        {
            //node1_start and node2_start are the indicies of the starting locations of two nodes that are going to be merged
            int node1_start = (2 * merge_index) * max_node_size;
            int node2_start = (2 * merge_index + 1) * max_node_size;
            for (int i = 1; i <= max_node_size; i++)
            {
                EncoderCardinalityNetwork aux_even_encoder, aux_odd_encoder;
                std::vector<BooleanLiteral> even1, even2, odd1, odd2;
                for (int k = 1; k <= max_node_size / 2; k += 2)
                {
                    if (node2_start + k - 1 >= num_literals_per_layer) { break; } //in some cases the last node might not be of size 'max_node-size', e.g., if there are three input literals, the second iteration will need to merge a node of size two with a node of size one
                    if (node2_start + k >= num_literals_per_layer) { break; } //in some cases the last node might not be of size 'max_node-size', e.g., if there are three input literals, the second iteration will need to merge a node of size two with a node of size one

                    odd1.push_back(current_layer->at(node1_start + k - 1));
                    odd2.push_back(current_layer->at(node2_start + k - 1));
                    
                    even1.push_back(current_layer->at(node1_start + k));
                    even2.push_back(current_layer->at(node2_start + k));
                }

                //(std::vector<BooleanLiteral>&input_literals, int right_hand_side, SolverState & state)


            }
            
            runtime_assert(1 == 2);
            for (int i = 1; i <= max_node_size; i++)
            {
                int node1_index = node1_start + i - 1;
                int sum_node_start = node1_start; //the sum_node is the node that is the result of merging two nodes (it stores the sum of the two nodes in unary form)
                //n1_i -> s_i
                state.AddBinaryClause(~current_layer->at(node1_index), next_layer->at(sum_node_start + i - 1));
                for (int j = 1; j <= max_node_size; j++)
                {
                    int node2_index = node2_start + j - 1;
                    if (node2_index >= num_literals_per_layer) { break; } //in some cases the last node might not be of size 'max_node-size', e.g., if there are three input literals, the second iteration will need to merge a node of size two with a node of size one
                    //n2_j -> s_j
                    state.AddBinaryClause(~current_layer->at(node2_index), next_layer->at(sum_node_start + j - 1));
                    //n1_i and n2_j -> s_{i+j}
                    state.AddTernaryClause(~current_layer->at(node1_index), ~current_layer->at(node2_index), next_layer->at(sum_node_start + i + j - 1));
                }
            }
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

    //store the result
    encoded_constraints_.push_back(EncodedConstraint());
    encoded_constraints_.back().input_literals = input_literals;
    encoded_constraints_.back().output_literals = output_literals;
    encoded_constraints_.back().right_hand_side = right_hand_side;

    return output_literals;
}*/
}