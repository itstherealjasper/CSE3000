#pragma once

#include "boolean_literal.h"
#include "boolean_assignment_vector.h"
#include "pair_weight_literal.h"

#include <vector>
#include <string>

namespace Pumpkin
{
class ProblemSpecification
{
public:
	static std::string GetFileType(std::string file_location);
	static ProblemSpecification ReadSATFormula(std::string file_location);
	static ProblemSpecification ReadMaxSATFormula(std::string file_location);
		
	void AddClause(std::vector<int32_t>& clause); //a clause that contain both polarities of a variable will not be added (e.g. 1 v -1). Furthermore, unit clauses that are already present will not be be added
	void AddSoftClause(std::vector<int32_t>& clause, int64_t weight);

	int NumBooleanVariables() const;

	//constraints
	std::vector<std::vector<int32_t> > hard_clauses_;
	
	//objective function
	struct PairWeightClause { std::vector<int32_t> soft_clause; int64_t weight; };
	std::vector<PairWeightClause> soft_clauses_;

	int64_t num_Boolean_variables_;
	int64_t num_unit_clauses_;
	int64_t num_binary_clauses_;
	int64_t num_ternary_clauses_;
	int64_t num_other_clauses_;
	int64_t num_literals_in_other_clauses_;
	
	bool IsSatisfyingAssignment(const BooleanAssignmentVector& solution); //solution[i] is the truth assignment for the variable with index i
	int ComputeCost(const BooleanAssignmentVector& solution);
	static bool IsClauseSatisfied(std::vector<int32_t>& clause, const BooleanAssignmentVector& solution); //returns whether the clause is satisfied by the given solution
																					  /*
	void TriviallyRefineSolution(std::vector<bool> &solution);
	
	void ConvertClausesIntoPseudoBoolean();
	bool ArePseudoBooleanConstraintsSatisfied(std::vector<bool> &solution);
	
	
	void PrintToFile(std::string file_location);
	void AddUnaryClause(BooleanLiteral literal);
	void AddBinaryClause(BooleanLiteral l1, BooleanLiteral l2);
	void AddTernaryClause(BooleanLiteral l1, BooleanLiteral l2, BooleanLiteral l3);
	
	*/

private:
	ProblemSpecification();
};

} //end Pumpkin namespace