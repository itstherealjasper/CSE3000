#include "result-parser.h"

using namespace std;

// ./result-parser/result-parser "./cluster-results/" "J30_all_setup1.out"
int main(int argc, char* argv[])
{
	stringstream command_line_parameters;
	for (int i = 1; i < argc; ++i) {
		command_line_parameters << argv[i] << '\n';
	}

	string result_filepath;
	string result_filename;
	string binary_filename;
	command_line_parameters >> result_filepath >> result_filename >> binary_filename;

	ifstream result_file(result_filepath + result_filename);

	if (!result_file.is_open()) {
		throw runtime_error("Could not open lib file");
	};

	vector<Test_Result> results;

	string read_filename = "different";
	int optimal_makespan;
	int setup_time;
	bool solved_heuristically;
	int64_t makespan;
	bool sat_solved;
	bool optimality_proven;

	string read_filename_next;
	int optimal_makespan_next;
	int setup_time_next;
	bool solved_heuristically_next;
	int64_t makespan_next;
	bool sat_solved_next;
	bool optimality_proven_next;

	int i = 0;

	while (result_file >> read_filename_next) {
		read_single_result(result_file, optimal_makespan_next, setup_time_next, solved_heuristically_next, makespan_next, optimality_proven_next, sat_solved_next);
		if (read_filename == read_filename_next) {
			Test_Result test_result;
			test_result.optimal_makespan = optimal_makespan;
			test_result.setup_time = setup_time;
			test_result.heuristic_makespan = makespan;
			test_result.sat_makespan = makespan_next;
			test_result.sat_solved = sat_solved_next;
			test_result.optimality_proven = optimality_proven_next;
			results.push_back(test_result);
			//cout << ++i << '\t' << test_result.optimal_makespan << '\t' << test_result.setup_time << '\t' << test_result.heuristic_makespan << '\t' << test_result.sat_makespan << '\t' << test_result.sat_solved << '\t' << test_result.optimality_proven << '\n';
		}
		read_filename = read_filename_next;
		optimal_makespan = optimal_makespan_next;
		setup_time = setup_time_next;
		solved_heuristically = solved_heuristically_next;
		makespan = makespan_next;
		sat_solved = sat_solved_next;
		optimality_proven = optimality_proven_next;
	}

	result_file.close();

	int sat_solved_count = 0;
	int sat_solved_optimally_count = 0;

	double percentage_deviation_heuristic_cumulative = 0;
	double percentage_deviation_sat_cumulative = 0;

	int improved_by_sat_count = 0;
	int equal_by_sat_count = 0;

	int percentage_improved_optimal_heuristic_count = 0;
	int percentage_improved_optimal_sat_count = 0;

	double percentage_improvement_optimal_heuristic_cumulative = 0.0;
	double percentage_improvement_optimal_sat_cumulative = 0.0;

	for (int i = 0; i < results.size(); i++)
	{
		if (results[i].sat_solved)
		{
			sat_solved_count++;
			percentage_deviation_sat_cumulative += results[i].percentage_deviation_sat();
			if (results[i].optimality_proven)
			{
				sat_solved_optimally_count++;
			}

			if (results[i].sat_makespan < results[i].heuristic_makespan)
			{
				improved_by_sat_count++;
			}
			else if (results[i].sat_makespan == results[i].heuristic_makespan) {
				equal_by_sat_count++;
			}

			if (results[i].sat_makespan < results[i].optimal_makespan) {
				percentage_improved_optimal_sat_count++;
				percentage_improvement_optimal_sat_cumulative += (((double)results[i].sat_makespan - (double)results[i].optimal_makespan) / (double)results[i].optimal_makespan) * 100.0;
			}
		}

		if (results[i].heuristic_makespan < results[i].optimal_makespan) {
			percentage_improved_optimal_heuristic_count++;
			percentage_improvement_optimal_heuristic_cumulative += (((double)results[i].heuristic_makespan - (double)results[i].optimal_makespan) / (double)results[i].optimal_makespan) * 100.0;
		}
		percentage_deviation_heuristic_cumulative += results[i].percentage_deviation_heuristic();
	}

	double failed_instances_percentage = (480.0 - (double)results.size()) * 100.0 / 480.0;

	double sat_solved_percentage = (double)sat_solved_count * 100.0 / (double)results.size();
	double sat_solved_optimally_percentage = (double)sat_solved_optimally_count * 100.0 / (double)sat_solved_count;

	double percantage_deviation_heuristic_percentage = percentage_deviation_heuristic_cumulative / (double)results.size();
	double percantage_deviation_sat_percentage = percentage_deviation_sat_cumulative / (double)sat_solved_count;

	double improved_by_sat_percentage = (double)improved_by_sat_count * 100.0 / (double)sat_solved_count;
	double equal_by_sat_percentage = (double)equal_by_sat_count * 100.0 / (double)sat_solved_count;

	double percentage_improved_optimal_heuristic_percentage = (double)percentage_improved_optimal_heuristic_count * 100.0 / (double)results.size();
	double percentage_improved_optimal_sat_percentage = (double)percentage_improved_optimal_sat_count * 100.0 / (double)results.size();

	double percentage_improvement_optimal_heuristic_percentage = percentage_improvement_optimal_heuristic_cumulative / (double)results.size();
	double percentage_improvement_optimal_sat_percentage = percentage_improvement_optimal_sat_cumulative / (double)results.size();

	double percentage_improvement_optimal_heuristic_of_improved_instances = percentage_improvement_optimal_heuristic_cumulative / (double)percentage_improved_optimal_heuristic_count;
	double percentage_improvement_optimal_sat_of_improved_instances = percentage_improvement_optimal_sat_cumulative / (double)percentage_improved_optimal_sat_count;

	cout << "Filename read:\t\t\t\t" << result_filename << '\n';
	cout << "Failed instances %:\t\t\t" << failed_instances_percentage << '\n';
	cout << "SAT solved %:\t\t\t\t" << sat_solved_percentage << '\n';
	cout << "SAT solved optimally %:\t\t\t" << sat_solved_optimally_percentage << '\n';
	cout << "Dev% heuristic:\t\t\t\t" << percantage_deviation_heuristic_percentage << '\n';
	cout << "Dev% SAT:\t\t\t\t" << percantage_deviation_sat_percentage << '\n';
	cout << "% improved by SAT:\t\t\t" << improved_by_sat_percentage << '\n';
	cout << "% equal by SAT:\t\t\t\t" << equal_by_sat_percentage << '\n';
	cout << "Better solution found by heuristic %:\t" << percentage_improved_optimal_heuristic_percentage << '\n';
	cout << "Better solution found by SAT %:\t\t" << percentage_improved_optimal_sat_percentage << '\n';
	cout << "% Improved by heuristic:\t\t" << percentage_improvement_optimal_heuristic_percentage << '\n';
	cout << "% Improved by sat:\t\t\t" << percentage_improvement_optimal_sat_percentage << '\n';
	cout << "%Dev improvement by heuristic:\t\t" << percentage_improvement_optimal_heuristic_of_improved_instances << '\n';
	cout << "%Dev improvement by sat:\t\t" << percentage_improvement_optimal_sat_of_improved_instances << '\n' << '\n';


	return 0;
}

void read_single_result(ifstream& result_file, int& optimal_makespan, int& setup_time, bool& solved_heuristically, int64_t& makespan, bool& optimality_proven, bool& sat_solved)
{
	result_file >> optimal_makespan >> setup_time >> solved_heuristically >> makespan;
	if (!solved_heuristically)
	{
		result_file >> optimality_proven;
		if (makespan != -1 && makespan != INT64_MAX) {
			sat_solved = true;
		}
		else {
			sat_solved = false;
		}
	}
}
