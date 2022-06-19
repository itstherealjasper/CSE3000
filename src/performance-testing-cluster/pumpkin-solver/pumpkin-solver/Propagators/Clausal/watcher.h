#pragma once

#include "clause.h"
#include "../../Utilities/boolean_literal.h"
#include "../../Engine/linear_clause_allocator.h"

namespace Pumpkin
{

class WatcherClause
{
public:
	WatcherClause() :clause_reference(NULL), cached_literal(BooleanLiteral()) {}
	WatcherClause(ClauseLinearReference watched_clause, BooleanLiteral blocking_literal) :clause_reference(watched_clause), cached_literal(blocking_literal) {}

	ClauseLinearReference clause_reference;
	BooleanLiteral cached_literal;
};

} //end Pumpkin namespace
