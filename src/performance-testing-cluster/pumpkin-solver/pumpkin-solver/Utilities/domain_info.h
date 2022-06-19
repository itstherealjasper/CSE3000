#pragma once

#include "../Utilities/pumpkin_assert.h"
#include "../Utilities/smart_bitvector.h"

namespace Pumpkin
{
struct DomainInfo
{
	int lower_bound, upper_bound;
	SmartBitvector is_value_in_domain;

	void Initialise(int lb, int ub)
	{
		pumpkin_assert_simple(lb <= ub, "Sanity check.");
		lower_bound = lb;
		upper_bound = ub;
		is_value_in_domain.Resize(ub + 1);
		for (int i = 0; i < lb; i++) { is_value_in_domain.ClearBit(i); }
		for (int i = lb; i <= ub; i++) { is_value_in_domain.SetBit(i); }
	}
};
}
