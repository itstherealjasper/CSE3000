#include "problem_specification.h"
#include "gz_file_reader.h"
#include "pumpkin_assert.h"

#include <iostream>
#include <algorithm>

namespace Pumpkin
{
std::string ProblemSpecification::GetFileType(std::string file_location)
{
	GzFileReader file_reader(file_location.c_str());
	std::string s;
	char c;
	int64_t n;

	//skip comments
	file_reader >> c;
	while (c == 'c')
	{
		file_reader.SkipLine();
		file_reader >> c;
	}
	pumpkin_assert_permanent(c == 'p', "Error reading file: expected 'p' at the beginning of the header.");
	file_reader >> s;

	if (s == "wcnf") { return s; }
	else if (s == "cnf") { return s; }
	else { return "unknown"; }
}

ProblemSpecification ProblemSpecification::ReadSATFormula(std::string file_location)
{
	ProblemSpecification problem_specification;

	GzFileReader file_reader(file_location.c_str());
	std::string s;
	char c;
	int64_t n;

	//skip comments
	file_reader >> c;
	while (c == 'c')
	{
		file_reader.SkipLine();
		file_reader >> c;
	}

	int64_t num_clauses;
	//read the header, i.e. number of variables and clauses
	pumpkin_assert_permanent(c == 'p', "Error reading file: expected 'p' at the beginning of the header.");
	file_reader >> s;
	pumpkin_assert_permanent(s == "wcnf" || s == "cnf", "Error reading file: the header does not specify file type cnf nor wcnf.");

	file_reader >> problem_specification.num_Boolean_variables_ >> num_clauses;

	//read the clauses
	for (int i = 0; i < num_clauses; i++)
	{
		std::vector<int32_t> clause;
		while (file_reader >> n)
		{
			pumpkin_assert_permanent(abs(n) <= problem_specification.num_Boolean_variables_, "Error reading the file: the variable index is larger than the number of specified variables.");

			if (n == 0) { break; }; //go until you read a zero, which denotes the end of the clause

			int32_t lit = n;
			clause.push_back(lit);
		}
		problem_specification.AddClause(clause);			
	}
	return problem_specification;
}

ProblemSpecification ProblemSpecification::ReadMaxSATFormula(std::string file_location)
{
	ProblemSpecification problem_specification;

	GzFileReader file_reader(file_location.c_str());
	std::string s;
	char c;
	int64_t n;

	//skip comments
	file_reader >> c;
	while (c == 'c')
	{
		file_reader.SkipLine();
		file_reader >> c;
	}

	int64_t num_clauses, hard_clause_weight;
	//read the header, i.e. number of variables and clauses
	pumpkin_assert_permanent(c == 'p', "Error reading file: expected 'p' at the beginning of the header.");
	file_reader >> s;
	pumpkin_assert_permanent(s == "wcnf" || s == "cnf", "Error reading file: the header does not specify file type cnf nor wcnf.");

	file_reader >> problem_specification.num_Boolean_variables_ >> num_clauses >> hard_clause_weight;
	
	//read the clauses 
	for (int i = 0; i < num_clauses; i++)
	{
		std::vector<int32_t> clause;
		while (file_reader >> n)
		{
			pumpkin_assert_permanent(n == hard_clause_weight || clause.empty() || abs(n) <= problem_specification.num_Boolean_variables_, "Error reading the file: the variable index is larger than the number of specified variables.");

			if (n == 0) { break; }; //go until you read a zero, which denotes the end of the clause

			int32_t lit = n;
			clause.push_back(lit);
		}
		pumpkin_assert_simple(clause.size() >= 2, "Sanity check."); //recall that clauses must be of size two since the zeros literal is a weight

		if (clause[0] == hard_clause_weight) //is a hard clause
		{
			clause.erase(clause.begin()); //remove the weight
			problem_specification.AddClause(clause);
		} 
		else //is a soft clause
		{
			int64_t weight = clause[0]; //the 0th literal is the weight
			clause.erase(clause.begin()); //remove the weight
			problem_specification.AddSoftClause(clause, weight);
		}		
	}
	// printf("c done reading the file\n");
	return problem_specification;
}

bool ProblemSpecification::IsSatisfyingAssignment(const BooleanAssignmentVector& solution)
{
	for (std::vector<int32_t>& clause : hard_clauses_)
	{
		if (!IsClauseSatisfied(clause, solution)) 
		{ 
			return false; 
		}
	}
	
	return true;
}

int ProblemSpecification::ComputeCost(const BooleanAssignmentVector& solution)
{
	int64_t cost = 0;
	for (auto &p : soft_clauses_) 
	{
		cost += ((!IsClauseSatisfied(p.soft_clause, solution))* p.weight); 
	}
	return cost;
}

bool ProblemSpecification::IsClauseSatisfied(std::vector<int32_t>& clause, const BooleanAssignmentVector& solution)
{
	for (int32_t literal : clause)
	{
		int32_t var_index = abs(literal);
		if (solution[var_index] && literal > 0 || solution[var_index] == 0 && literal < 0) { return true; }
	}
	return false;
}

void ProblemSpecification::AddClause(std::vector<int32_t>& clause)
{	
	hard_clauses_.push_back(clause);
	num_unit_clauses_ += (clause.size() == 1);
	num_binary_clauses_ += (clause.size() == 2);
	num_ternary_clauses_ += (clause.size() == 3);
	num_other_clauses_ += (clause.size() > 3);
	num_literals_in_other_clauses_ += (clause.size() > 3) * clause.size();
}

void ProblemSpecification::AddSoftClause(std::vector<int32_t>& clause, int64_t weight)
{
	PairWeightClause p;
	p.soft_clause = clause;
	p.weight = weight;
	soft_clauses_.push_back(p);
}

int ProblemSpecification::NumBooleanVariables() const
{
	return num_Boolean_variables_;
}

ProblemSpecification::ProblemSpecification():
	num_Boolean_variables_(0),
	num_unit_clauses_(0),
	num_binary_clauses_(0),
	num_ternary_clauses_(0),
	num_other_clauses_(0),
	num_literals_in_other_clauses_(0)
{
}

} //end Pumpkin namespace