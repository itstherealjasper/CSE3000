#include "encoder_tester.h"
#include "encoder_tester.h"
#include "encoder_totaliser.h"
#include "encoder_generalised_totaliser.h"
#include "encoder_cardinality_network.h"
#include "../Engine/constraint_satisfaction_solver.h"
#include "../Utilities/combinatorics.h"
#include "../Utilities/runtime_assert.h"

namespace Pumpkin 
{
void EncoderTester::TestTotaliserEncoding(ParameterHandler &parameters)
{
	int max_num_literals = 20;
	for (int num_literals = 1; num_literals <= max_num_literals; num_literals++)
	{
		std::cout << "n = " << num_literals << "\n";
		for (int cardinality = 0; cardinality <= num_literals; cardinality++)
		{
			std::cout << "\tk = " << cardinality << "\n";
			ConstraintSatisfactionSolver solver(parameters);
			
			//create the variables which will be used for the cardinality encoding
			std::vector<BooleanLiteral> literals;
			for (int i = 0; i < num_literals; i++) 
			{ 
				IntegerVariable integer_variable = solver.state_.CreateNewIntegerVariable(0, 1);
				BooleanLiteral literal = solver.state_.GetEqualityLiteral(integer_variable, 1);
				literals.push_back(literal);
			}
			
			//create the encoding
			EncoderTotaliser totaliser_encoding;
			std::vector<BooleanLiteral> soft_literals = totaliser_encoding.SoftLessOrEqual(literals, cardinality, solver.state_);
			std::cout << cardinality << " out of " << num_literals << ": " << totaliser_encoding.NumClausesAddedLastTime() << "\n";

			//get assumptions that fix the soft literals to false
			std::vector<BooleanLiteral> assumptions_force_cardinality_encoding;
			for (BooleanLiteral soft_literal : soft_literals) { assumptions_force_cardinality_encoding.push_back(~soft_literal); }
			
			//generate all possible assignments and use them to test whether the encoding rules them as SAT or UNSAT
			std::vector<std::vector<bool> > possible_solutions = Combinatorics::GenerateAllBooleanVectors(num_literals);
			std::cout << "\n...testing for " << possible_solutions.size() << " solutions with cardinality " << cardinality << "\n";

			for (std::vector<bool>& solution : possible_solutions)
			{	
				//determine if the solution is satisfying or not
				int sum = 0;
				for (bool val : solution) { sum += val; }
				bool should_be_SAT = sum <= cardinality;

				//get assumptions that fix the solution entirely
				std::vector<BooleanLiteral> assumptions = assumptions_force_cardinality_encoding;
				for (int i = 0; i < num_literals; i++)
				{
					if (solution[i] == true) { assumptions.push_back(literals[i]); }
					else					  { assumptions.push_back(~literals[i]); }
				}
				//solve and test if the output is correct
				SolverOutput output = solver.Solve(assumptions);

				if (output.HasSolution() && !should_be_SAT)
				{
					std::cout << "Problem: SAT for UNSAT!\n";
					std::cout << "\tn = " << num_literals << ", k = " << cardinality << "\n";
				}
				
				if (output.ProvenInfeasible() && should_be_SAT)
				{
					std::cout << "Problem: UNSAT for SAT!\n";
					std::cout << "\tn = " << num_literals << ", k = " << cardinality << "\n";
				}
			}
		}
	}
}

void EncoderTester::TestCardinalityNetworkEncoding(ParameterHandler& parameters)
{
	int max_num_literals = 20;
	for (int num_literals = 1; num_literals <= max_num_literals; num_literals++)
	{
		std::cout << "n = " << num_literals << "\n";
		for (int cardinality = 0; cardinality <= num_literals; cardinality++)
		{
			std::cout << "\tk = " << cardinality << "\n";
			ConstraintSatisfactionSolver solver(parameters);

			//create the variables which will be used for the cardinality encoding
			std::vector<BooleanLiteral> literals;
			for (int i = 0; i < num_literals; i++)
			{
				IntegerVariable integer_variable = solver.state_.CreateNewIntegerVariable(0, 1);
				BooleanLiteral literal = solver.state_.GetEqualityLiteral(integer_variable, 1);
				literals.push_back(literal);
			}

			//create the encoding
			EncoderCardinalityNetwork cardinality_network_encoding;
			std::vector<BooleanLiteral> soft_literals = cardinality_network_encoding.SoftLessOrEqual(literals, cardinality, solver.state_);
			std::cout << cardinality << " out of " << num_literals << ": " << cardinality_network_encoding.NumClausesAddedLastTime() << "\n";

			//get assumptions that fix the soft literals to false
			std::vector<BooleanLiteral> assumptions_force_cardinality_encoding;
			for (BooleanLiteral soft_literal : soft_literals) { assumptions_force_cardinality_encoding.push_back(~soft_literal); }

			//generate all possible assignments and use them to test whether the encoding rules them as SAT or UNSAT
			std::vector<std::vector<bool> > possible_solutions = Combinatorics::GenerateAllBooleanVectors(num_literals);
			std::cout << "\n...testing for " << possible_solutions.size() << " solutions with cardinality " << cardinality << "\n";

			for (std::vector<bool>& solution : possible_solutions)
			{
				//determine if the solution is satisfying or not
				int sum = 0;
				for (bool val : solution) { sum += val; }
				bool should_be_SAT = sum <= cardinality;

				//get assumptions that fix the solution entirely
				std::vector<BooleanLiteral> assumptions = assumptions_force_cardinality_encoding;
				for (int i = 0; i < num_literals; i++)
				{
					if (solution[i] == true) { assumptions.push_back(literals[i]); }
					else { assumptions.push_back(~literals[i]); }
				}
				//solve and test if the output is correct
				SolverOutput output = solver.Solve(assumptions);

				if (output.HasSolution() && !should_be_SAT)
				{
					std::cout << "Problem: SAT for UNSAT!\n";
					std::cout << "\tn = " << num_literals << ", k = " << cardinality << "\n";
				}

				if (output.ProvenInfeasible() && should_be_SAT)
				{
					std::cout << "Problem: UNSAT for SAT!\n";
					std::cout << "\tn = " << num_literals << ", k = " << cardinality << "\n";
				}
			}
		}
	}
}

void EncoderTester::TestGeneralisedTotaliserAsCardinalityConstraintEncoding(ParameterHandler& parameters)
{
	int max_num_literals = 20;
	for (int num_literals = 1; num_literals <= max_num_literals; num_literals++)
	{
		std::cout << "n = " << num_literals << "\n";
		for (int cardinality = 0; cardinality <= num_literals; cardinality++)
		{
			std::cout << "\tk = " << cardinality << "\n";
			ConstraintSatisfactionSolver solver(parameters);

			//create the variables which will be used for the cardinality encoding
			std::vector<PairWeightLiteral> literals;
			for (int i = 0; i < num_literals; i++)
			{
				IntegerVariable integer_variable = solver.state_.CreateNewIntegerVariable(0, 1);
				BooleanLiteral literal = solver.state_.GetEqualityLiteral(integer_variable, 1);
				literals.push_back(PairWeightLiteral(literal, 1));
			}

			//create the encoding
			EncoderGeneralisedTotaliser totaliser_encoding;
			std::vector<PairWeightLiteral> soft_literals = totaliser_encoding.SoftLessOrEqual(literals, cardinality, solver.state_);

			//get assumptions that fix the soft literals to false
			std::vector<BooleanLiteral> assumptions_force_cardinality_encoding;
			for (PairWeightLiteral p : soft_literals) { assumptions_force_cardinality_encoding.push_back(~p.literal); }

			//generate all possible assignments and use them to test whether the encoding rules them as SAT or UNSAT
			std::vector<std::vector<bool> > possible_solutions = Combinatorics::GenerateAllBooleanVectors(num_literals);
			std::cout << "\n...testing for " << possible_solutions.size() << " solutions with cardinality " << cardinality << "\n";

			for (std::vector<bool>& solution : possible_solutions)
			{
				//determine if the solution is satisfying or not
				int sum = 0;
				for (bool val : solution) { sum += val; }
				bool should_be_SAT = sum <= cardinality;

				//get assumptions that fix the solution entirely
				std::vector<BooleanLiteral> assumptions = assumptions_force_cardinality_encoding;
				for (int i = 0; i < num_literals; i++)
				{
					if (solution[i] == true) { assumptions.push_back(literals[i].literal); }
					else { assumptions.push_back(~literals[i].literal); }
				}
				//solve and test if the output is correct
				SolverOutput output = solver.Solve(assumptions);

				if (output.HasSolution() && !should_be_SAT)
				{
					std::cout << "Problem: SAT for UNSAT!\n";
					std::cout << "\tn = " << num_literals << ", k = " << cardinality << "\n";
				}

				if (output.ProvenInfeasible() && should_be_SAT)
				{
					std::cout << "Problem: UNSAT for SAT!\n";
					std::cout << "\tn = " << num_literals << ", k = " << cardinality << "\n";
				}
			}
		}
	}
}

void EncoderTester::TestGeneralisedTotaliser(ParameterHandler& parameters, int64_t max_weight)
{
	int max_num_literals = 20;
	for (int num_literals = 1; num_literals <= max_num_literals; num_literals++)
	{
		std::cout << "n = " << num_literals << "\n";
		for (int iter_num = 0; iter_num < 100; iter_num++)
		{
			std::cout << "\titer: " << iter_num << "\n";
			std::vector<int64_t> weights;
			int64_t total_weight_sum = 0;
			for (int i = 0; i < num_literals; i++)
			{
				weights.push_back((rand() % max_weight)+1);
				total_weight_sum += weights.back();
			}			
			for (int right_hand_side = 0; right_hand_side <= total_weight_sum; right_hand_side++)
			{
				std::cout << "\trhs = " << right_hand_side << "\n";
				ConstraintSatisfactionSolver solver(parameters);

				//create the variables which will be used for the encoding
				std::vector<PairWeightLiteral> literals;
				for (int i = 0; i < num_literals; i++)
				{
					IntegerVariable integer_variable = solver.state_.CreateNewIntegerVariable(0, 1);
					BooleanLiteral literal = solver.state_.GetEqualityLiteral(integer_variable, 1);
					literals.push_back(PairWeightLiteral(literal, 1));
				}

				//create the encoding
				EncoderGeneralisedTotaliser totaliser_encoding;
				std::vector<PairWeightLiteral> soft_literals = totaliser_encoding.SoftLessOrEqual(literals, right_hand_side, solver.state_);

				//get assumptions that fix the soft literals to false
				std::vector<BooleanLiteral> assumptions_force_cardinality_encoding;
				for (PairWeightLiteral p : soft_literals) { assumptions_force_cardinality_encoding.push_back(~p.literal); }

				//generate all possible assignments and use them to test whether the encoding rules them as SAT or UNSAT
				std::vector<std::vector<bool> > possible_solutions = Combinatorics::GenerateAllBooleanVectors(num_literals);
				std::cout << "\n...testing for " << possible_solutions.size() << " solutions with right hand side " << right_hand_side << "\n";

				for (std::vector<bool>& solution : possible_solutions)
				{
					//determine if the solution is satisfying or not
					int64_t sum = 0;
					for (int i = 0; i < num_literals; i++)
					{
						sum += literals[i].weight * solution[i];
					}					
					bool should_be_SAT = (sum <= right_hand_side);

					//get assumptions that fix the solution entirely
					std::vector<BooleanLiteral> assumptions = assumptions_force_cardinality_encoding;
					for (int i = 0; i < num_literals; i++)
					{
						if (solution[i] == true) { assumptions.push_back(literals[i].literal); }
						else { assumptions.push_back(~literals[i].literal); }
					}
					//solve and test if the output is correct
					SolverOutput output = solver.Solve(assumptions);

					if (output.HasSolution() && !should_be_SAT)
					{
						std::cout << "Problem: SAT for UNSAT!\n";
						std::cout << "\tn = " << num_literals << ", rhs = " << right_hand_side << "\n";
						system("pause");
					}

					if (output.ProvenInfeasible() && should_be_SAT)
					{
						std::cout << "Problem: UNSAT for SAT!\n";
						std::cout << "\tn = " << num_literals << ", rhs = " << right_hand_side << "\n";
						system("pause");
					}
				}
			}
		}
	}
}

void EncoderTester::TestGeneralisedTotaliserAsCardinalityConstraintEncodingHard(ParameterHandler& parameters)
{
	int max_num_literals = 20;
	for (int num_literals = 1; num_literals <= max_num_literals; num_literals++)
	{
		std::cout << "n = " << num_literals << "\n";
		for (int cardinality = 0; cardinality <= num_literals; cardinality++)
		{
			std::cout << "\tk = " << cardinality << "\n";
			ConstraintSatisfactionSolver solver(parameters);

			//create the variables which will be used for the cardinality encoding
			std::vector<PairWeightLiteral> literals;
			for (int i = 0; i < num_literals; i++)
			{
				IntegerVariable integer_variable = solver.state_.CreateNewIntegerVariable(0, 1);
				BooleanLiteral literal = solver.state_.GetEqualityLiteral(integer_variable, 1);
				literals.push_back(PairWeightLiteral(literal, 1));
			}

			//create the encoding
			EncoderGeneralisedTotaliser totaliser_encoding;
			totaliser_encoding.HardLessOrEqual(literals, cardinality, solver.state_);

			//generate all possible assignments and use them to test whether the encoding rules them as SAT or UNSAT
			std::vector<std::vector<bool> > possible_solutions = Combinatorics::GenerateAllBooleanVectors(num_literals);
			std::cout << "\n...testing for " << possible_solutions.size() << " solutions with cardinality " << cardinality << "\n";

			for (std::vector<bool>& solution : possible_solutions)
			{
				//determine if the solution is satisfying or not
				int sum = 0;
				for (bool val : solution) { sum += val; }
				bool should_be_SAT = sum <= cardinality;

				//get assumptions that fix the solution entirely
				std::vector<BooleanLiteral> assumptions;
				for (int i = 0; i < num_literals; i++)
				{
					if (solution[i] == true) { assumptions.push_back(literals[i].literal); }
					else { assumptions.push_back(~literals[i].literal); }
				}
				//solve and test if the output is correct
				SolverOutput output = solver.Solve(assumptions);

				if (output.HasSolution() && !should_be_SAT)
				{
					std::cout << "Problem: SAT for UNSAT!\n";
					std::cout << "\tn = " << num_literals << ", k = " << cardinality << "\n";
					system("pause");
				}

				if (output.ProvenInfeasible() && should_be_SAT)
				{
					std::cout << "Problem: UNSAT for SAT!\n";
					std::cout << "\tn = " << num_literals << ", k = " << cardinality << "\n";
					system("pause");
				}
			}
		}
	}
}

void EncoderTester::TestGeneralisedTotaliserHard(ParameterHandler& parameters, int64_t max_weight)
{
	int max_num_literals = 20;
	for (int num_literals = 1; num_literals <= max_num_literals; num_literals++)
	{
		std::cout << "n = " << num_literals << "\n";
		for (int iter_num = 0; iter_num < 100; iter_num++)
		{
			std::cout << "\titer: " << iter_num << "\n";
			std::vector<int64_t> weights;
			int64_t total_weight_sum = 0;
			for (int i = 0; i < num_literals; i++)
			{
				weights.push_back((rand() % max_weight) + 1);
				total_weight_sum += weights.back();
			}
			for (int right_hand_side = 0; right_hand_side <= total_weight_sum; right_hand_side++)
			{
				std::cout << "\trhs = " << right_hand_side << "\n";
				ConstraintSatisfactionSolver solver(parameters);

				//create the variables which will be used for the encoding
				std::vector<PairWeightLiteral> literals;
				for (int i = 0; i < num_literals; i++)
				{
					IntegerVariable integer_variable = solver.state_.CreateNewIntegerVariable(0, 1);
					BooleanLiteral literal = solver.state_.GetEqualityLiteral(integer_variable, 1);
					literals.push_back(PairWeightLiteral(literal, 1));
				}

				//create the encoding
				EncoderGeneralisedTotaliser totaliser_encoding;
				totaliser_encoding.HardLessOrEqual(literals, right_hand_side, solver.state_);

				//generate all possible assignments and use them to test whether the encoding rules them as SAT or UNSAT
				std::vector<std::vector<bool> > possible_solutions = Combinatorics::GenerateAllBooleanVectors(num_literals);
				std::cout << "\n...testing for " << possible_solutions.size() << " solutions with right hand side " << right_hand_side << "\n";

				for (std::vector<bool>& solution : possible_solutions)
				{
					//determine if the solution is satisfying or not
					int64_t sum = 0;
					for (int i = 0; i < num_literals; i++)
					{
						sum += literals[i].weight * solution[i];
					}
					bool should_be_SAT = (sum <= right_hand_side);

					//get assumptions that fix the solution entirely
					std::vector<BooleanLiteral> assumptions;
					for (int i = 0; i < num_literals; i++)
					{
						if (solution[i] == true) { assumptions.push_back(literals[i].literal); }
						else { assumptions.push_back(~literals[i].literal); }
					}
					//solve and test if the output is correct
					SolverOutput output = solver.Solve(assumptions);

					if (output.HasSolution())
					{
						runtime_assert(totaliser_encoding.DebugCheckSatisfactionOfEncodedConstraints(output.solution));
					}

					totaliser_encoding.Clear();

					if (output.HasSolution() && !should_be_SAT)
					{
						std::cout << "Problem: SAT for UNSAT!\n";
						std::cout << "\tn = " << num_literals << ", rhs = " << right_hand_side << "\n";
						system("pause");
					}

					if (output.ProvenInfeasible() && should_be_SAT)
					{
						std::cout << "Problem: UNSAT for SAT!\n";
						std::cout << "\tn = " << num_literals << ", rhs = " << right_hand_side << "\n";
						system("pause");
					}
				}
			}
		}
	}
}
}