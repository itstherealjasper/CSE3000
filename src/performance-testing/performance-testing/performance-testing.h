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
#include "../pblib/pblib/pb2cnf.h"
#include "../pumpkin-solver/pumpkin-solver/pumpkin-solver.h"

using namespace std;

// Parameters to tune the algorithm
const int setup_time = 1; // time_units
const int cpu_time_deadline = 20; // seconds

// Global variables for the specific problem instance used
const string project_lib_folder = "C:/Projects/cse3000/src/parsed-datasets/j30.sm/";
const string project_lib_file = "j301_0.RCP";

const string cnf_file_type = "wcnf";
static string extract_filename_without_extention(string file_path);

class Heuristic_Solver
{
private:
	struct Id
	{
	private:
		int id = 0;
	public:
		int get_max_id() {
			return id;
		}

		int get_next_id() {
			int result;
			result = id;
			id = id + 1;
			assert(id < INT_MAX);
			return result;
		}
	};

	struct Task
	{
		int task_id;
		int split_task_id;
		vector<int> successors;
		int duration;
		vector<int> resource_requirements; // default value required
		double rur; // resource utility rate

		void calculate_rur(vector<int>& resource_availabilities)
		{
			rur = 0;
			for (int i = 0; i < resource_availabilities.size(); i++)
			{
				rur += (double)resource_requirements[i] / (double)resource_availabilities[i];
			}
			rur *= duration;
		}

		bool operator > (const Task& o) const
		{
			return (rur > o.rur);
		}
	};

	// Parameters to tune the algorithm
	int destruction_count = 0;

	// Project
	int horizon;
	int optimal_solution;
	int initial_task_count;
	vector<Task> initial_tasks;
	int resource_count;
	vector<int> resource_availabilities;

	// Global task counter to prevent duplicates
	Id job_id;

	void parse_activities(string filename);
	void extend_initial_successors(Task& task);
	void calculate_rurs();
	void fix_presedence_constraint(vector<Task>& task_list);
	template <typename t> void move(vector<t>& v, size_t oldIndex, size_t newIndex);
	bool check_presedence_violation(vector<Task>& task_list, vector<int>& task_in_violation_at_index);
	void construct_initial_task_list(vector<Task>& task_list);
	int calculate_makespan(vector<Task>& task_list, int task_count);
	int find_earliest_start_time(int duration, vector<int> resource_requirements, vector<vector<int>>& remaining_resources, int earliest_start_time);
	int construct_schedule(vector<Task>& task_list, vector<int>& start_times, int task_count);
	int optimize_task_list(int makespan, vector<Task>& job_list, bool preempt_tasks, int setup_time);
	void extend_successors(Task& task, vector<Task>& task_list);

public:
	int solve(string project_lib_folder, string project_lib_file, int setup_time, int cpu_time_deadline);
};

class SAT_encoder
{
private:
	struct Task
	{
		int id;
		int segment;
		vector<pair<int, int>> successors;
		int duration;
		vector<int> resource_requirements;

		// Heuristic bound variable
		double rur; // resource utility rate
		int earliest_start_time = 0;

		// CNF variables
		int early_start = -INT_MAX / 2;
		int early_finish = INT_MAX / 2;
		int late_start = -INT_MAX / 2;
		int late_finish = INT_MAX / 2;

		vector<int32_t> start_variables;
		vector<int32_t> process_variables;
	};

	struct Task_Id
	{
	private:
		int next_unused_id = 0;
		vector<int> reusable_ids;
	public:
		int get_unused_id()
		{
			if (reusable_ids.size() > 0) {
				int unused_id = reusable_ids[0];
				reusable_ids.erase(reusable_ids.begin());
				return unused_id;
			}
			return next_unused_id++;
		}

		void set_reusable_id(int reusable_id)
		{
			if (reusable_id >= next_unused_id) {
				throw new exception("reusable id is outside of the used id range");
			}
			reusable_ids.push_back(reusable_id);
		}
	};

	struct CNF_variable
	{
	private:
		int32_t last_used_variable = 0;
	public:
		int32_t get_variable()
		{
			assert(last_used_variable < INT32_MAX);
			return ++last_used_variable;
		}

		int32_t get_variable_count()
		{
			return last_used_variable;
		}

		void set_last_used_variable(int32_t new_last_used_variable) {
			assert(new_last_used_variable >= last_used_variable);
			last_used_variable = new_last_used_variable;
		}
	};

	void parse_input_file(string filename);
	void critical_path();
	void forward_pass(pair<int, int> task_segment_id, int early_start);
	void backward_pass(pair<int, int> task_segment_id, int late_finish);
	void preempt_tasks();
	void preempt_task(Task task);
	void set_start_variables();
	void set_process_variables();
	void remove_duplicate_segments(vector<Task>& task_list);
	void extend_initial_successors(Task& task, vector<Task>& task_list);
	void calculate_rurs(vector<Task>& task_list);
	void fix_presedence_constraint(vector<Task>& task_list);
	bool check_presedence_violation(vector<Task>& task_list, vector<int>& task_in_violation_at_index);
	template <typename t> void move(vector<t>& v, size_t oldIndex, size_t newIndex);
	void construct_upper_bound_task_list(vector<Task>& task_list);
	int calculate_sgs_makespan(vector<Task>& task_list);
	int find_earliest_start_time(int duration, vector<int> resource_requirements, vector<vector<int>>& remaining_resources, int earliest_start_time);
	void write_cnf_file(vector<Task>& task_list, string project_lib_file);
	void build_consistency_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
	void build_precedence_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
	void build_completion_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
	void build_resource_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
	void build_objective_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
	int get_cnf_hard_clause_weight();

	Task_Id unique_task_id;

	int horizon;
	int optimal_solution;
	vector<Task> parsed_tasks;
	vector<int> resource_availabilities;
	vector<Task> preempted_tasks;

	// Global CNF construction variables
	int upper_bound_makespan;
	PB2CNF pb2cnf;
	CNF_variable cnf_variable;

public:
	string encode(string project_lib_folder, string project_lib_file, int setup_time, int cpu_time_deadline);
};