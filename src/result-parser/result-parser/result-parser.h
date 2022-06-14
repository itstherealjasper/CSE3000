#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cassert>
#include <random>
#include <ctime>
#include <tuple>
#include <filesystem>
#include <iterator>

class Test_Result
{
public:
	int optimal_makespan;
	int setup_time;
	int heuristic_makespan;
	int sat_makespan;
	bool sat_solved = false;
	bool optimality_proven = false;

	double percentage_deviation_heuristic() {
		return (((double)heuristic_makespan - (double)optimal_makespan) / (double)optimal_makespan) * 100.0;
	}

	double percentage_deviation_sat() {
		return (((double)sat_makespan - (double)optimal_makespan) / (double)optimal_makespan) * 100.0;
	}
};

void read_single_result(std::ifstream& result_file, int& optimal_makespan, int& setup_time, bool& solved_heuristically, int64_t& makespan, bool& optimality_proven, bool& sat_solved);
