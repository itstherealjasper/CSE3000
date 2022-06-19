#include "performance-testing.h"

using namespace std;
namespace fs = std::filesystem;

static void SIGINT_exit(int signum)
{
	//cout << "SIGINT exit detected";
	if (solve_heuristically) {
		cout << solve_heuristically << '\t' << heuristic_solver.get_best_makespan() << '\n' << '\n';
	}
	else {
		if (done_encoding) {
			external_exit(solve_heuristically);
		}
		else {
			cout << solve_heuristically << '\t' << -1 << '\t' << 0 << '\n' << '\n';
		}
		fs::path file_path_to_remove("./");
		string file_name_to_remove = extract_filename_without_extention(project_file_name) + '.' + cnf_file_type;
		remove(file_path_to_remove / file_name_to_remove);
	}
	exit(1);
}

int main(int argc, char* argv[])
{
	signal(SIGINT, SIGINT_exit);
	signal(SIGTERM, SIGINT_exit);

	stringstream command_line_parameters;
	for (int i = 1; i < argc; ++i) {
		command_line_parameters << argv[i] << '\n';
	}

	string project_lib_subfolder;
	int setup_time;
	command_line_parameters >> project_lib_subfolder >> project_file_name >> setup_time >> solve_heuristically;

	ifstream project_lib(project_lib_subfolder + project_file_name);

	if (!project_lib.is_open()) {
		throw runtime_error("Could not open lib file");
	};

	string tmp;
	int optimal_solution;
	project_lib >> tmp >> tmp >> tmp >> optimal_solution;

	cout << extract_filename_without_extention(project_file_name) << '\t' << optimal_solution << '\t' << setup_time << '\n';

	if (solve_heuristically) {
		heuristic_solver.solve(project_lib_subfolder, project_file_name, setup_time);
	}
	else {
		sat_encoder.encode(project_lib_subfolder, project_file_name, setup_time);
		done_encoding = true;

		bool optimum_found;
		int SAT_makespan = solve(extract_filename_without_extention(project_file_name) + '.' + cnf_file_type, optimum_found);
		cout << solve_heuristically << '\t' << SAT_makespan << '\t' << optimum_found << '\n' << '\n';
		fs::path file_path_to_remove("./");
		string file_name_to_remove = extract_filename_without_extention(project_file_name) + '.' + cnf_file_type;
		remove(file_path_to_remove / file_name_to_remove);
	}

	return 0;
}

int Heuristic_Solver::solve(string project_lib_folder, string project_lib_file, int setup_time)
{
#pragma region setup
	parse_activities(project_lib_folder + project_lib_file);
	calculate_rurs();
#pragma endregion

#pragma region initial schedule
	vector<Task> initial_task_list = initial_tasks;
	vector<int> initial_start_times;
	int initial_makespan;

	sort(initial_task_list.begin() + 1, initial_task_list.end() - 1, greater<Task>());
	fix_presedence_constraint(initial_task_list);
	construct_initial_task_list(initial_task_list);
	initial_makespan = construct_schedule(initial_task_list, initial_start_times, initial_task_count);
#pragma endregion
	// from here on all (global) variables with the initial_ preamble will not be modified

#pragma region algorithm
	vector<Task> task_list = initial_task_list;
	makespan = initial_makespan;

	int no_better_solution_found = 0;
	while (no_better_solution_found > 20) {
		int new_makespan = optimize_task_list(makespan, task_list, false, setup_time);
		if (new_makespan == makespan) {
			no_better_solution_found++;
		}
		else {
			no_better_solution_found = 0;
		}
		makespan = new_makespan;
	}

	vector<int> start_times = vector<int>(job_id.get_max_id(), 0);
	construct_schedule(task_list, start_times, job_id.get_max_id());

	while (true) {
		makespan = optimize_task_list(makespan, task_list, true, setup_time);
	}

#pragma endregion

	return makespan;
}

int Heuristic_Solver::get_best_makespan()
{
	return makespan;
}

void Heuristic_Solver::parse_activities(string filename)
{
	string tmp;

	ifstream project_lib(filename);

	if (!project_lib.is_open()) {
		throw runtime_error("Could not open lib file");
	};

	project_lib >> horizon >> initial_task_count >> resource_count >> optimal_solution;

	resource_availabilities = vector<int>(resource_count);
	for (int i = 0; i < resource_count; i++)
	{
		project_lib >> resource_availabilities[i];
	}

	initial_tasks = vector<Task>(initial_task_count);
	for (int i = 0; i < initial_task_count; i++)
	{
		int parsed_task_id;
		project_lib >> parsed_task_id;
		initial_tasks[i].task_id = job_id.get_next_id();
		initial_tasks[i].split_task_id = initial_tasks[i].task_id;

		initial_tasks[i].resource_requirements = vector<int>(resource_count);
		for (int j = 0; j < resource_count; j++)
		{
			project_lib >> initial_tasks[i].resource_requirements[j];
		}

		project_lib >> initial_tasks[i].duration;

		int successor_count;
		project_lib >> successor_count;
		initial_tasks[i].successors = vector<int>(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> initial_tasks[i].successors[j];
		}
	}

	destruction_count = ceil((double)initial_task_count / (double)4);

	// Close the file
	project_lib.close();
}

void Heuristic_Solver::calculate_rurs()
{
	for (int i = 0; i < initial_task_count; i++)
	{
		initial_tasks[i].calculate_rur(resource_availabilities);
	}
}

void Heuristic_Solver::fix_presedence_constraint(vector<Task>& task_list)
{
	vector<int> task_in_violation_at_index;

	bool presedence_violated = check_presedence_violation(task_list, task_in_violation_at_index, task_list);
	while (presedence_violated) {
		move(task_list, task_in_violation_at_index[0], task_in_violation_at_index[1]);
		presedence_violated = check_presedence_violation(task_list, task_in_violation_at_index, task_list);
	}
}

// Found on StackOverflow answer: https://stackoverflow.com/a/57399634
template <typename t> void Heuristic_Solver::move(vector<t>& v, size_t oldIndex, size_t newIndex)
{
	if (oldIndex > newIndex)
	{
		rotate(v.rend() - oldIndex - 1, v.rend() - oldIndex, v.rend() - newIndex);
	}
	else {
		rotate(v.begin() + oldIndex, v.begin() + oldIndex + 1, v.begin() + newIndex + 1);
	}
}

template<typename T> vector<T> Heuristic_Solver::flatten(vector<vector<T>> const& vec)
{
	vector<T> flattened;
	for (auto const& v : vec) {
		flattened.insert(flattened.end(), v.begin(), v.end());
	}
	return flattened;
}

bool Heuristic_Solver::check_presedence_violation(vector<Task>& task_list, vector<int>& task_in_violation_at_index, vector<Task>& complete_task_list)
{
	for (int i = 1; i < task_list.size(); i++)
	{
		vector<int> successors = task_list[i].successors;
		while (successors.size() > 0)
		{
			int successor = successors[0];
			successors.erase(successors.begin());
			for (int j = 0; j < i; j++)
			{
				if (task_list[j].split_task_id == successor) {
					task_in_violation_at_index = { i,j };
					return true;
				}
			}
			for (int j = 0; j < complete_task_list.size(); j++)
			{
				if (complete_task_list[j].split_task_id == successor)
				{
					for (int k = 0; k < complete_task_list[j].successors.size(); k++)
					{
						successors.push_back(complete_task_list[j].successors[k]);
					}
					break;
				}
			}
		}
	}

	return false;
	//for (int i = 1; i < task_list.size(); i++)
	//{
	//	for (int j = 0; j < i; j++)
	//	{
	//		int sucessor_index = find(task_list[i].successors.begin(), task_list[i].successors.end(), task_list[j].split_task_id) - task_list[i].successors.begin();
	//		if (sucessor_index != task_list[i].successors.end() - task_list[i].successors.begin())
	//		{
	//			task_in_violation_at_index = { i, j };
	//			return true;
	//		}
	//	}
	//}

	//return false;
}

void Heuristic_Solver::construct_initial_task_list(vector<Task>& task_list)
{
	vector<Task> new_task_list;
	vector<Task> remaining_task_list;
	new_task_list.push_back(task_list[0]);
	new_task_list.push_back(task_list[1]);

	for (int i = 2; i < initial_task_count; i++)
	{
		remaining_task_list.push_back(task_list[i]);
	}

	while (remaining_task_list.size() > 0) {
		int minimum_makespan = horizon;
		int optimal_index = -1;
		Task to_schedule_task = remaining_task_list[0];
		remaining_task_list.erase(remaining_task_list.begin());
		for (int i = 0; i <= new_task_list.size(); i++)
		{
			new_task_list.insert(new_task_list.begin() + i, to_schedule_task);
			vector<int>tmp(2);
			if (!check_presedence_violation(new_task_list, tmp, task_list)) {
				int makespan_test = calculate_makespan(new_task_list, initial_task_count);
				if (makespan_test < minimum_makespan) {
					minimum_makespan = makespan_test;
					optimal_index = i;
				}
			}
			new_task_list.erase(new_task_list.begin() + i);
		}
		assert(optimal_index >= 0);
		new_task_list.insert(new_task_list.begin() + optimal_index, to_schedule_task);
	}

	task_list = new_task_list;
}

int Heuristic_Solver::calculate_makespan(vector<Task>& task_list, int task_count)
{
	vector<vector<int>> remaining_resources;
	for (int i = 0; i < horizon; i++)
	{
		remaining_resources.push_back(resource_availabilities);
	}

	vector<int> start_times(task_count, 0);

	for (int i = 0; i < task_list.size(); i++)
	{
		int earliest_start_time = find_earliest_start_time(task_list[i].duration, task_list[i].resource_requirements, remaining_resources, start_times[task_list[i].split_task_id]);
		start_times[task_list[i].split_task_id] = earliest_start_time;
		for (int j = 0; j < task_list[i].duration; j++)
		{
			for (int k = 0; k < task_list[i].resource_requirements.size(); k++)
			{
				remaining_resources[earliest_start_time + j][k] = remaining_resources[earliest_start_time + j][k] - task_list[i].resource_requirements[k];
			}
		}

		for (int j = 0; j < task_list[i].successors.size(); j++)
		{
			if (earliest_start_time + task_list[i].duration > start_times[task_list[i].successors[j]])
			{
				start_times[task_list[i].successors[j]] = earliest_start_time + task_list[i].duration;
			}
		}
	}

	return start_times[initial_task_count - 1];
}

int Heuristic_Solver::construct_schedule(vector<Task>& task_list, vector<int>& start_times, int task_count)
{
	vector<vector<int>> remaining_resources;
	for (int i = 0; i < horizon; i++)
	{
		remaining_resources.push_back(resource_availabilities);
	}

	vector<int> new_start_times(task_count, 0);

	for (int i = 0; i < task_list.size(); i++)
	{
		int earliest_start_time = find_earliest_start_time(task_list[i].duration, task_list[i].resource_requirements, remaining_resources, new_start_times[task_list[i].split_task_id]);
		new_start_times[task_list[i].split_task_id] = earliest_start_time;
		for (int j = 0; j < task_list[i].duration; j++)
		{
			for (int k = 0; k < task_list[i].resource_requirements.size(); k++)
			{
				remaining_resources[earliest_start_time + j][k] = remaining_resources[earliest_start_time + j][k] - task_list[i].resource_requirements[k];
			}
		}

		for (int j = 0; j < task_list[i].successors.size(); j++)
		{
			if (earliest_start_time + task_list[i].duration > new_start_times[task_list[i].successors[j]])
			{
				new_start_times[task_list[i].successors[j]] = earliest_start_time + task_list[i].duration;
			}
		}
	}

	start_times = new_start_times;
	return start_times.back();
}

int Heuristic_Solver::find_earliest_start_time(int duration, vector<int> resource_requirements, vector<vector<int>>& remaining_resources, int earliest_start_time)
{
	bool found_possible_timeslot = false;
	while (!found_possible_timeslot) {
		bool requirements_to_high = false;
		for (int i = 0; i < duration; i++)
		{
			for (int j = 0; j < resource_requirements.size(); j++)
			{
				if (resource_requirements[j] > remaining_resources[earliest_start_time + i][j])
				{
					requirements_to_high = true;
				}
			}
		}
		if (requirements_to_high) {
			earliest_start_time++;
		}
		else {
			found_possible_timeslot = true;
		}
	}

	return earliest_start_time;
}

int Heuristic_Solver::optimize_task_list(int makespan, vector<Task>& task_list, bool preempt_tasks, int setup_time)
{
	vector<Task> new_task_list = task_list;

	vector<int> destruction_ids;
	vector<vector<vector<int>>> destruction_split_ids;
	assert(destruction_count > 0);
	for (int i = 0; i < destruction_count; i++)
	{

		int random_task_index = (rand() % (task_list.size() - 1)) + 1;
		int random_task_id = task_list[random_task_index].split_task_id;
		while (random_task_id == initial_task_count - 1 || find(destruction_ids.begin(), destruction_ids.end(), random_task_id) != destruction_ids.end()) {
			random_task_index = (rand() % (task_list.size() - 1)) + 1;
			random_task_id = task_list[random_task_index].split_task_id;
		}

		destruction_ids.push_back(random_task_id);

		Task to_be_destroyed;
		for (int j = 0; j < task_list.size(); j++)
		{
			if (task_list[j].split_task_id == random_task_id)
			{
				to_be_destroyed = task_list[j];
			}
		}

		if (!preempt_tasks || to_be_destroyed.duration <= 1)
		{
			destruction_split_ids.push_back(vector<vector<int>> {vector<int>{to_be_destroyed.split_task_id}});
		}
		else
		{
			int split_index = (rand() % (to_be_destroyed.duration - 1)) + 1;

			Task to_be_destroyed_begin = to_be_destroyed;
			to_be_destroyed_begin.split_task_id = job_id.get_next_id();
			to_be_destroyed_begin.duration = split_index;

			Task to_be_destroyed_end = to_be_destroyed;
			to_be_destroyed_end.split_task_id = job_id.get_next_id();
			to_be_destroyed_end.duration = to_be_destroyed.duration - split_index + setup_time;

			to_be_destroyed_begin.successors.push_back(to_be_destroyed_end.split_task_id);

			new_task_list.push_back(to_be_destroyed_begin);
			new_task_list.push_back(to_be_destroyed_end);

			for (int j = 0; j < new_task_list.size(); j++)
			{
				if (find(new_task_list[j].successors.begin(), new_task_list[j].successors.end(), to_be_destroyed.split_task_id) != new_task_list[j].successors.end())
				{
					new_task_list[j].successors.push_back(to_be_destroyed_begin.split_task_id);
					new_task_list[j].successors.push_back(to_be_destroyed_end.split_task_id);
				}
			}

			destruction_split_ids.push_back(vector<vector<int>> {vector<int>{to_be_destroyed.split_task_id}, vector<int>{to_be_destroyed_begin.split_task_id, to_be_destroyed_end.split_task_id}});
		}
	}

	vector<Task> remaining_tasks = new_task_list;
	vector<vector<vector<Task>>> destroyed_tasks;
	for (int i = 0; i < destruction_split_ids.size(); i++)
	{
		vector<vector<Task>> destroyed_task;
		for (int j = 0; j < destruction_split_ids[i].size(); j++)
		{
			vector<Task> destroyed_task_parts;
			for (int k = 0; k < destruction_split_ids[i][j].size(); k++)
			{
				Task destroyed_task_part;
				for (int l = 0; l < remaining_tasks.size(); l++)
				{
					if (remaining_tasks[l].split_task_id == destruction_split_ids[i][j][k])
					{
						destroyed_task_part = remaining_tasks[l];
						remaining_tasks.erase(remaining_tasks.begin() + l);
					}
				}
				destroyed_task_parts.push_back(destroyed_task_part);
			}
			destroyed_task.push_back(destroyed_task_parts);
		}
		destroyed_tasks.push_back(destroyed_task);
	}

	if (preempt_tasks) {
		for (int i = 0; i < destroyed_tasks.size(); i++)
		{
			if (destroyed_tasks[i].size() < 2)
			{
				continue;
			}
			for (int j = 0; j < destroyed_tasks.size(); j++)
			{
				if (j == i)
				{
					continue;
				}
				for (int k = 0; k < destroyed_tasks[j].size(); k++)
				{
					for (int l = 0; l < destroyed_tasks[j][k].size(); l++)
					{
						for (int m = 0; m < destroyed_tasks[j][k][l].successors.size(); m++)
						{
							if (destroyed_tasks[i][1][0].task_id == destroyed_tasks[j][k][l].successors[m])
							{
								destroyed_tasks[j][k][l].successors.push_back(destroyed_tasks[i][1][0].split_task_id);
							}
						}
					}
				}
			}
		}
	}

	vector<int>tmp(2);
	vector<Task> successor_test_list = remaining_tasks;
	for (vector<vector<Task>> destroyed_task : destroyed_tasks) {
		vector<Task> flattened = flatten(destroyed_task);
		successor_test_list.insert(successor_test_list.end(), flattened.begin(), flattened.end());
	}

	vector<int> cleanup_task_ids;

	while (destroyed_tasks.size() > 0)
	{
		vector<vector<Task>> destroyed_task = destroyed_tasks[0];
		destroyed_tasks.erase(destroyed_tasks.begin());

		pair<int, vector<Task>> best_possible_task_list = pair<int, vector<Task>>{ horizon, vector<Task>{} };
		vector<int> previous_best_parts;

		for (int i = 0; i < destroyed_task.size(); i++)
		{
			vector<Task> possible_task_list = remaining_tasks;
			for (int j = 0; j < destroyed_task[i].size(); j++)
			{
				int minimum_makespan = horizon;
				int optimal_insertion_index = -1;
				for (int k = 0; k < possible_task_list.size(); k++)
				{
					possible_task_list.insert(possible_task_list.begin() + k, destroyed_task[i][j]);
					if (!check_presedence_violation(possible_task_list, tmp, successor_test_list)) {
						int makespan_test = calculate_makespan(possible_task_list, job_id.get_max_id());
						if (makespan_test < minimum_makespan) {
							minimum_makespan = makespan_test;
							optimal_insertion_index = k;
						}
					}
					possible_task_list.erase(possible_task_list.begin() + k);
				}
				assert(optimal_insertion_index >= 0);
				possible_task_list.insert(possible_task_list.begin() + optimal_insertion_index, destroyed_task[i][j]);
			}
			int possible_task_list_makespan = calculate_makespan(possible_task_list, job_id.get_max_id());
			if (possible_task_list_makespan < best_possible_task_list.first) {
				best_possible_task_list = pair<int, vector<Task>>{ possible_task_list_makespan , possible_task_list };
				for (int j = 0; j < previous_best_parts.size(); j++)
				{
					cleanup_task_ids.push_back(previous_best_parts[j]);
				}
				previous_best_parts = vector<int>{};
				for (int j = 0; j < destroyed_task[i].size(); j++)
				{
					previous_best_parts.push_back(destroyed_task[i][j].split_task_id);
				}
			}
			else {
				for (int j = 0; j < destroyed_task[i].size(); j++)
				{
					cleanup_task_ids.push_back(destroyed_task[i][j].split_task_id);
				}
			}
		}
		assert(best_possible_task_list.first < horizon);
		remaining_tasks = best_possible_task_list.second;
	}

	int makespan_test = calculate_makespan(remaining_tasks, job_id.get_max_id());

	if (makespan_test < makespan)
	{
		for (int i = 0; i < cleanup_task_ids.size(); i++)
		{
			int cleanup_task_id = cleanup_task_ids[i];
			for (int j = 0; j < remaining_tasks.size(); j++)
			{
				remaining_tasks[j].successors.erase(remove_if(remaining_tasks[j].successors.begin(), remaining_tasks[j].successors.end(), [&cleanup_task_id](int successor) {return successor == cleanup_task_id; }), remaining_tasks[j].successors.end());
			}
		}
		task_list = remaining_tasks;
		return makespan_test;
	}

	return makespan;
}

string SAT_encoder::encode(string project_lib_folder, string project_lib_file, int setup_time)
{
#pragma region setup
	parse_input_file(project_lib_folder + project_lib_file);

#pragma region upper bound
	vector<Task> upper_bound_task_list = parsed_tasks;
	vector<int> upper_bound_start_times;

	calculate_rurs(upper_bound_task_list);
	sort(upper_bound_task_list.begin() + 1, upper_bound_task_list.end() - 1, [](const Task& lhs, const Task& rhs) {return lhs.rur > rhs.rur; });
	fix_presedence_constraint(upper_bound_task_list);

	construct_upper_bound_task_list(upper_bound_task_list);
	upper_bound_makespan = calculate_sgs_makespan(upper_bound_task_list);
#pragma endregion

	critical_path();
	preempt_tasks(setup_time);

	set_start_variables();
	set_process_variables();

	vector<Task> reduced_preempted_tasks = preempted_tasks;
	remove_duplicate_segments(reduced_preempted_tasks);
#pragma endregion

#pragma region CNF encoding
	write_cnf_file(preempted_tasks, project_lib_file);
#pragma endregion


	Task back_task = preempted_tasks.back();

	return string();
}

void SAT_encoder::parse_input_file(string filename)
{
	string tmp;

	ifstream project_lib(filename);

	if (!project_lib.is_open()) {
		throw runtime_error("Could not open lib file");
	};

	int task_count;
	int resource_count;

	project_lib >> horizon >> task_count >> resource_count >> optimal_solution;

	resource_availabilities = vector<int>(resource_count);
	for (int i = 0; i < resource_count; i++)
	{
		project_lib >> resource_availabilities[i];
	}

	parsed_tasks = vector<Task>(task_count);
	for (int i = 0; i < task_count; i++)
	{
		int parsed_task_id;
		project_lib >> parsed_task_id;
		parsed_tasks[i].id = unique_task_id.get_unused_id();
		parsed_tasks[i].segment = 1;

		parsed_tasks[i].resource_requirements = vector<int>(resource_count);
		for (int j = 0; j < resource_count; j++)
		{
			project_lib >> parsed_tasks[i].resource_requirements[j];
		}

		project_lib >> parsed_tasks[i].duration;

		int successor_count;
		project_lib >> successor_count;
		parsed_tasks[i].successors = vector<pair<int, int>>(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> parsed_tasks[i].successors[j].first;
			parsed_tasks[i].successors[j].second = 1;
		};
	}

	// Close the file
	project_lib.close();
}

void SAT_encoder::critical_path()
{
	parsed_tasks[0].early_start = 0;
	parsed_tasks[0].early_finish = -1;
	for (int j = 0; j < parsed_tasks[0].successors.size(); j++)
	{
		forward_pass(parsed_tasks[0].successors[j], 0);
	}

	parsed_tasks.back().late_start = upper_bound_makespan + 1;
	parsed_tasks.back().late_finish = upper_bound_makespan;
	for (int j = 0; j < parsed_tasks.size(); j++)
	{
		for (int k = 0; k < parsed_tasks[j].successors.size(); k++)
		{
			if (parsed_tasks[j].successors[k].first == parsed_tasks.back().id && parsed_tasks[j].successors[k].second == parsed_tasks.back().segment)
			{
				backward_pass(pair<int, int>(parsed_tasks[j].id, parsed_tasks[j].segment), upper_bound_makespan);
			}
		}
	}
}

void SAT_encoder::forward_pass(pair<int, int> task_segment_id, int early_start)
{
	for (int i = 0; i < parsed_tasks.size(); i++)
	{
		if (parsed_tasks[i].id == task_segment_id.first && parsed_tasks[i].segment == task_segment_id.second)
		{
			if (early_start > parsed_tasks[i].early_start) {
				parsed_tasks[i].early_start = early_start;
				parsed_tasks[i].early_finish = early_start + parsed_tasks[i].duration;
				for (int j = 0; j < parsed_tasks[i].successors.size(); j++)
				{
					forward_pass(parsed_tasks[i].successors[j], parsed_tasks[i].early_finish);
				}
			}
		}
	}
}

void SAT_encoder::backward_pass(pair<int, int> task_segment_id, int late_finish)
{
	for (int i = 0; i < parsed_tasks.size(); i++)
	{
		if (parsed_tasks[i].id == task_segment_id.first && parsed_tasks[i].segment == task_segment_id.second)
		{
			if (late_finish < parsed_tasks[i].late_finish) {
				parsed_tasks[i].late_finish = late_finish;
				parsed_tasks[i].late_start = late_finish - parsed_tasks[i].duration;

				for (int j = 0; j < parsed_tasks.size(); j++)
				{
					for (int k = 0; k < parsed_tasks[j].successors.size(); k++)
					{
						if (parsed_tasks[j].successors[k].first == task_segment_id.first && parsed_tasks[j].successors[k].second == task_segment_id.second)
						{
							backward_pass(pair<int, int>(parsed_tasks[j].id, parsed_tasks[j].segment), parsed_tasks[i].late_start);
						}
					}
				}
			}
		}
	}
}

void SAT_encoder::preempt_tasks(int setup_time)
{
	preempted_tasks.push_back(parsed_tasks.front());
	for (Task parsed_task : parsed_tasks)
	{
		preempt_task(parsed_task, setup_time);
	}
	preempted_tasks.push_back(parsed_tasks.back());
}

void SAT_encoder::preempt_task(Task task, int setup_time)
{
	for (int i = 1; i <= task.duration; i++)
	{
		for (int j = task.duration + 1 - i; j > 0; j--)
		{
			Task task_part = task;
			task_part.segment = i;
			task_part.duration = task_part.segment > 1 ? j + setup_time : j;
			if (task_part.segment > 1) {
				task_part.early_start += (i - 1);
			}
			if (task_part.segment + task_part.duration - 1 < task.duration) {
				task_part.late_finish -= (task.duration - task_part.segment - task_part.duration + 1 + setup_time);
			}
			if (task_part.segment + task_part.duration != task.segment + task.duration) {
				task_part.successors = vector<pair<int, int>>{ pair<int, int>(task.id, task_part.segment + task_part.duration) };
			}
			preempted_tasks.push_back(task_part);
		}
	}
}

void SAT_encoder::set_start_variables()
{
	for (int i = 1; i < preempted_tasks.size(); i++)
	{
		for (int t = preempted_tasks[i].early_start; t <= preempted_tasks[i].late_finish - preempted_tasks[i].duration; t++)
		{
			preempted_tasks[i].start_variables.push_back(cnf_variable.get_variable());
		}
	}
}

void SAT_encoder::set_process_variables()
{
	for (int i = 1; i < preempted_tasks.size() - 1; i++)
	{
		for (int t = preempted_tasks[i].early_start; t <= preempted_tasks[i].late_finish; t++)
		{
			preempted_tasks[i].process_variables.push_back(cnf_variable.get_variable());
		}
	}
}

void SAT_encoder::remove_duplicate_segments(vector<Task>& task_list)
{
	int i = 0;
	while (i < task_list.size())
	{
		bool removed_segment = false;
		int j = i + 1;
		while (j < task_list.size())
		{
			if (task_list[j].id == task_list[i].id &&
				task_list[j].segment >= task_list[i].segment &&
				task_list[j].segment < task_list[i].segment + task_list[i].duration)
			{
				task_list.erase(task_list.begin() + j);
				removed_segment = true;
			}
			j++;
		}
		if (removed_segment) {
			i = 0;
		}
		else {
			i++;
		}
	}
}

void SAT_encoder::calculate_rurs(vector<Task>& task_list)
{
	for (int i = 0; i < task_list.size(); i++)
	{
		task_list[i].rur = 0;
		for (int j = 0; j < resource_availabilities.size(); j++)
		{
			task_list[i].rur += (double)task_list[i].resource_requirements[j] / (double)resource_availabilities[j];
		}
		task_list[i].rur *= task_list[i].duration;
	}
}

void SAT_encoder::fix_presedence_constraint(vector<Task>& task_list)
{
	vector<int> task_in_violation_at_index;

	bool presedence_violated = check_presedence_violation(task_list, task_in_violation_at_index);
	while (presedence_violated) {
		move(task_list, task_in_violation_at_index[0], task_in_violation_at_index[1]);
		presedence_violated = check_presedence_violation(task_list, task_in_violation_at_index);
	}
}

bool SAT_encoder::check_presedence_violation(vector<Task>& task_list, vector<int>& task_in_violation_at_index)
{
	for (int i = 1; i < task_list.size(); i++)
	{
		vector<pair<int, int>> successors = task_list[i].successors;
		while (successors.size() > 0)
		{
			pair<int, int> successor = successors[0];
			successors.erase(successors.begin());
			for (int j = 0; j < i; j++)
			{
				if (task_list[j].id == successor.first) {
					task_in_violation_at_index = { i,j };
					return true;
				}
			}
			for (int j = 0; j < parsed_tasks.size(); j++)
			{
				if (parsed_tasks[j].id == successor.first) {
					for (int k = 0; k < parsed_tasks[j].successors.size(); k++)
					{
						successors.push_back(parsed_tasks[j].successors[k]);
					}
					break;
				}
			}
		}
	}

	return false;

	/*for (int i = 1; i < task_list.size(); i++)
	{
		for (int j = 0; j < i; j++)
		{
			for (int k = 0; k < task_list[i].successors.size(); k++)
			{
				if (task_list[i].successors[k].first == task_list[j].id && task_list[i].successors[k].second == task_list[j].segment)
				{
					task_in_violation_at_index = { i, j };
					return true;
				}
			}
		}
	}

	return false;*/
}

// Found on StackOverflow answer: https://stackoverflow.com/a/57399634
template <typename t> void SAT_encoder::move(vector<t>& v, size_t oldIndex, size_t newIndex)
{
	if (oldIndex > newIndex)
	{
		rotate(v.rend() - oldIndex - 1, v.rend() - oldIndex, v.rend() - newIndex);
	}
	else {
		rotate(v.begin() + oldIndex, v.begin() + oldIndex + 1, v.begin() + newIndex + 1);
	}
}



void SAT_encoder::construct_upper_bound_task_list(vector<Task>& task_list)
{
	int task_list_size = task_list.size();

	vector<Task> new_task_list;
	vector<Task> remaining_task_list;
	new_task_list.push_back(task_list[0]);
	new_task_list.push_back(task_list[1]);
	new_task_list.push_back(task_list.back());

	for (int i = 2; i < task_list_size - 1; i++)
	{
		remaining_task_list.push_back(task_list[i]);
	}

	while (remaining_task_list.size() > 0) {
		int minimum_makespan = horizon;
		int optimal_index = -1;
		Task to_schedule_task = remaining_task_list[0];
		remaining_task_list.erase(remaining_task_list.begin());
		for (int i = 0; i <= new_task_list.size(); i++)
		{
			new_task_list.insert(new_task_list.begin() + i, to_schedule_task);
			vector<int>tmp(2);
			if (!check_presedence_violation(new_task_list, tmp)) {
				int makespan_test = calculate_sgs_makespan(new_task_list);
				if (makespan_test < minimum_makespan) {
					minimum_makespan = makespan_test;
					optimal_index = i;
				}
			}
			new_task_list.erase(new_task_list.begin() + i);
		}
		assert(optimal_index >= 0);
		new_task_list.insert(new_task_list.begin() + optimal_index, to_schedule_task);
	}

	task_list = new_task_list;
}

int SAT_encoder::calculate_sgs_makespan(vector<Task>& task_list)
{
	vector<vector<int>> remaining_resources;
	for (int i = 0; i < horizon; i++)
	{
		remaining_resources.push_back(resource_availabilities);
	}

	for (int i = 0; i < task_list.size(); i++)
	{
		task_list[i].earliest_start_time = 0;
	}

	for (int i = 0; i < task_list.size(); i++)
	{
		task_list[i].earliest_start_time = find_earliest_start_time(task_list[i].duration, task_list[i].resource_requirements, remaining_resources, task_list[i].earliest_start_time);
		for (int j = 0; j < task_list[i].duration; j++)
		{
			for (int k = 0; k < task_list[i].resource_requirements.size(); k++)
			{
				remaining_resources[task_list[i].earliest_start_time + j][k] = remaining_resources[task_list[i].earliest_start_time + j][k] - task_list[i].resource_requirements[k];
			}
		}

		for (int j = 0; j < task_list[i].successors.size(); j++)
		{
			for (int k = 0; k < task_list.size(); k++)
			{
				if (task_list[i].successors[j].first == task_list[k].id && task_list[i].successors[j].second == task_list[k].segment && task_list[i].earliest_start_time + task_list[i].duration > task_list[k].earliest_start_time)
				{
					task_list[k].earliest_start_time = task_list[i].earliest_start_time + task_list[i].duration;
				}
			}
		}
	}

	return task_list.back().earliest_start_time + task_list.back().duration;
}

int SAT_encoder::find_earliest_start_time(int duration, vector<int> resource_requirements, vector<vector<int>>& remaining_resources, int earliest_start_time)
{
	bool found_possible_timeslot = false;
	while (!found_possible_timeslot) {
		bool requirements_to_high = false;
		for (int i = 0; i < duration; i++)
		{
			for (int j = 0; j < resource_requirements.size(); j++)
			{
				if (resource_requirements[j] > remaining_resources[earliest_start_time + i][j])
				{
					requirements_to_high = true;
				}
			}
		}
		if (requirements_to_high) {
			earliest_start_time++;
		}
		else {
			found_possible_timeslot = true;
		}
	}

	return earliest_start_time;
}

void SAT_encoder::write_cnf_file(vector<Task>& task_list, string project_lib_file)
{
	stringstream cnf_file_content;
	int clause_count = 0;
	build_completion_clauses(task_list, cnf_file_content, clause_count);
	build_precedence_clauses(task_list, cnf_file_content, clause_count);
	build_consistency_clauses(task_list, cnf_file_content, clause_count);
	build_resource_clauses(task_list, cnf_file_content, clause_count);
	build_objective_clauses(task_list, cnf_file_content, clause_count);

	ofstream schedule_file(extract_filename_without_extention(project_lib_file) + '.' + cnf_file_type);
	if (schedule_file.is_open())
	{
		// p FORMAT VARIABLES CLAUSES
		schedule_file << 'p' << ' ' << cnf_file_type << ' ' << cnf_variable.get_variable_count() << ' ' << clause_count << ' ' << get_cnf_hard_clause_weight() << '\n';
		schedule_file << cnf_file_content.str();
		schedule_file.close();
	}
}

void SAT_encoder::build_completion_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{

	for (Task task : parsed_tasks)
	{
		if (task.id == 0)
		{
			continue;
		}

		vector<Task> task_segments;

		for (Task task_segment : task_list)
		{
			if (task_segment.id == task.id) {
				task_segments.push_back(task_segment);
			}
		}

		for (int time_slot = 1; time_slot <= task.duration; time_slot++)
		{
			vector<Task> active_task_segments;
			for (Task task_segment : task_segments)
			{
				if (task_segment.segment <= time_slot && task_segment.segment + task_segment.duration - 1 >= time_slot) {
					active_task_segments.push_back(task_segment);
				}
			}
			cnf_file_content << get_cnf_hard_clause_weight() << ' ';
			for (Task active_task_segment : active_task_segments)
			{
				for (int32_t start_variable : active_task_segment.start_variables)
				{
					cnf_file_content << start_variable << ' ';
				}
			}
			cnf_file_content << '0' << '\n';
			clause_count++;
		}

	}
}

void SAT_encoder::build_precedence_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{
	for (Task successor : task_list)
	{
		if (successor.id == 0)
		{
			continue;
		}

		vector<vector<Task>> predecessor_groups(parsed_tasks.size());
		for (Task predecessor : task_list)
		{
			if (predecessor.id == 0)
			{
				continue;
			}

			for (pair<int, int> successor_id_segment : predecessor.successors)
			{
				if (successor.id == successor_id_segment.first && successor.segment == successor_id_segment.second)
				{
					predecessor_groups[predecessor.id].push_back(predecessor);
				}
			}
		}

		for (vector<Task> predecessor_group : predecessor_groups)
		{
			for (int i = 0; i < successor.start_variables.size(); i++)
			{
				if (predecessor_group.size() > 0) {
					cnf_file_content << get_cnf_hard_clause_weight() << ' ';
					cnf_file_content << '-' << successor.start_variables[i] << ' ';
					for (Task predecessor : predecessor_group)
					{
						assert(i + successor.early_start - predecessor.duration - predecessor.early_start >= 0);
						assert(predecessor.start_variables.size() >= 0);
						for (int j = 0; j <= i + successor.early_start - predecessor.duration - predecessor.early_start && j < predecessor.start_variables.size(); j++)
						{
							cnf_file_content << predecessor.start_variables[j] << ' ';
						}
					}
					cnf_file_content << '0' << '\n';
					clause_count++;
				}
			}
		}
	}
}

void SAT_encoder::build_consistency_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{
	for (Task task : task_list)
	{
		if (task.id == 0)
		{
			continue;
		}

		for (int i = 0; i < task.start_variables.size(); i++)
		{
			for (int j = i; j < i + task.duration; j++)
			{
				cnf_file_content << get_cnf_hard_clause_weight() << ' ';
				cnf_file_content << '-' << task.start_variables[i] << ' ';
				cnf_file_content << task.process_variables[j] << ' ';
				cnf_file_content << '0' << '\n';
				clause_count++;
			}
		}
	}
}

void SAT_encoder::build_resource_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{
	for (int i = 0; i < resource_availabilities.size(); i++)
	{
		for (int j = 0; j <= upper_bound_makespan; j++)
		{
			vector<int64_t> weights;
			vector<int32_t> literals;
			for (Task task : task_list)
			{
				if (task.duration == 0)
				{
					continue;
				}
				if (task.resource_requirements[i] > 0 && task.early_start <= j && task.late_finish >= j)
				{
					literals.push_back(task.process_variables[j - task.early_start]);
					weights.push_back(task.resource_requirements[i]);
				}
			}

			vector<vector<int32_t>> formula;
			int32_t first_fresh_variable = cnf_variable.get_variable_count() + 1;
			int resource_availability = resource_availabilities[i];
			first_fresh_variable = pb2cnf.encodeLeq(weights, literals, resource_availability, formula, first_fresh_variable) + 1;
			cnf_variable.set_last_used_variable(first_fresh_variable - 1);

			for (vector<int32_t> disjunction : formula)
			{
				cnf_file_content << get_cnf_hard_clause_weight() << ' ';
				for (int32_t variable : disjunction)
				{
					cnf_file_content << variable << ' ';
				}
				cnf_file_content << '0' << '\n';
				clause_count++;
			}
		}
	}
}

void SAT_encoder::build_objective_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{
	Task finish = task_list.back();

	int32_t latest_start_variable = cnf_variable.get_variable();
	cnf_file_content << get_cnf_hard_clause_weight() << ' ';
	cnf_file_content << '-' << latest_start_variable << ' ';
	cnf_file_content << '0' << '\n';
	clause_count++;

	cnf_file_content << finish.early_start << ' ';
	cnf_file_content << latest_start_variable << ' ';
	cnf_file_content << '0' << '\n';
	clause_count++;

	vector<int32_t> literals;

	for (int i = 0; i < finish.start_variables.size(); i++)
	{
		cnf_file_content << 1 << ' ';
		cnf_file_content << finish.start_variables[i] << ' ';
		cnf_file_content << '0' << '\n';
		clause_count++;
		literals.push_back(finish.start_variables[i]);
	}
}

// Base file without extention extraction found at https://stackoverflow.com/a/24386991
static string extract_filename_without_extention(string file_path)
{
	string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
	string::size_type const p(base_filename.find_last_of('.'));
	return base_filename.substr(0, p);
}

int SAT_encoder::get_cnf_hard_clause_weight()
{
	return ((horizon * (horizon + 1)) / 2) + 1;
}