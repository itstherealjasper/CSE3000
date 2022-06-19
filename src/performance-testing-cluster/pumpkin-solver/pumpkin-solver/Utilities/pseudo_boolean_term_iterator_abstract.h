#ifndef  PSEUDO_BOOLEAN_TERM_ITERATOR_ABSTRACT_H
#define  PSEUDO_BOOLEAN_TERM_ITERATOR_ABSTRACT_H

#include "boolean_literal.h"

namespace Pumpkin
{

//This is an abstract class.
//It is designed to provide a common interface among different but similar data structures that represent pseudo boolean linear constraints.
//This allows us to use the same code for propagation for these different algorithms and avoid certain implicit assumptions
//todo refine the description!
//todo check, I think this is a simple input iterator

class PseudoBooleanTermIteratorAbstract
{
public:
	virtual void operator++() = 0;
	virtual uint32_t Coefficient() = 0;
	virtual BooleanLiteral Literal() = 0;
	virtual BooleanVariableInternal Variable() = 0;
	virtual bool IsValid()= 0;
	//virtual bool operator==(const PseudoBooleanTermIteratorAbstract &rhs) = 0;
	//bool operator!=(const PseudoBooleanTermIteratorAbstract &rhs) { return !this->operator==(rhs); }

//protected: todo make this private
	//virtual uint64_t MemoryLocationPointer() const = 0;
};

} //end Pumpkin namespace

#endif // ! PSEUDO_BOOLEAN_TERM_ITERATOR_ABSTRACT_H