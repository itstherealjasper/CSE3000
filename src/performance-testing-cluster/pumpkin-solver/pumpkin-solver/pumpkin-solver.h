#pragma once

#include "Engine/constraint_optimisation_solver.h"
#include "Utilities/solver_output_checker.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <time.h>
#include <cstdlib>
#include <signal.h>
#include <string>

int solve(std::string wncf_filename, bool& optimum_found);