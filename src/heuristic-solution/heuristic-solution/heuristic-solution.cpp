#include "heuristic-solution.h"

using namespace std;

struct Id
{
private:
	int id = 0;
	vector<int> reusable_ids;
public:
	int get_max_id() {
		return id;
	}

	int get_next_id() {
		int result;
		if (reusable_ids.size() > 0) {
			result = reusable_ids[0];
			reusable_ids.erase(reusable_ids.begin());
		}
		else {
			result = id;
			id = id + 1;
			assert(id < INT_MAX);
		}
		return result;
	}

	void add_reusable_id(int id) {
		reusable_ids.push_back(id);
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

void parse_activities(string filename);
void extend_initial_successors(Task& task);
void calculate_rurs();
void fix_presedence_constraint(vector<Task>& task_list);
template <typename t> void move(vector<t>& v, size_t oldIndex, size_t newIndex);
bool check_presedence_violation(vector<Task>& task_list);
bool check_presedence_violation(vector<Task>& task_list, vector<int>& task_in_violation_at_index);
void construct_initial_task_list(vector<Task>& task_list);
int calculate_makespan(vector<Task>& task_list, int task_count);
int find_earliest_start_time(int duration, vector<int> resource_requirements, vector<vector<int>>& remaining_resources, int earliest_start_time);
int construct_schedule(vector<Task>& task_list, vector<int>& start_times, int task_count);
int optimize_task_list(int makespan, vector<Task>& job_list, bool preempt_tasks);
void extend_successors(Task& task, vector<Task>& task_list);

//void write_resource_schedule(int makespan, vector<int>& start_times, vector<Task>& task_list, string filename);
//void try_build_3d_resource_schedule(vector<Task>& task_list, vector<int>& start_times, vector<vector<vector<int>>>& resource_schedule, vector<vector<vector<int>>>& resource_schedule_preemption_ids);

// Parameters to tune the algorithm
const int destruction_count = 8;
const int cpu_time_deadline_optimization = 10; // seconds
const int cpu_time_deadline_preemption = 50; // seconds
const int setup_time = 1;

// Global variables for the specific problem instance used
const string project_lib_folder_j30 = "C:/Projects/cse3000/src/datasets/j30.sm/";
const string project_lib_file_j30 = "j301_4";
const string project_lib_type_j30 = ".sm";

int horizon;
int initial_task_count;
vector<Task> initial_tasks;
int resource_count;
vector<int> resource_availabilities;

// Global task counter to prevent duplicates
Id job_id;

int main()
{
#pragma region setup
	parse_activities(project_lib_folder_j30 + project_lib_file_j30 + project_lib_type_j30);
	extend_initial_successors(initial_tasks[0]);
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

	cout << "Initial scheduling done \nMakespan: " << initial_makespan << "\n";
	//write_resource_schedule(initial_makespan, initial_start_times, initial_task_list, "initial_schedule");
#pragma endregion
	// from here on all (global) variables with the initial_ preamble will not be modified

#pragma region algorithm
	vector<Task> task_list = initial_task_list;
	int makespan = initial_makespan;

	clock_t c_start = clock();

	long double time_elapsed_ms = 0.0;
	while (time_elapsed_ms < 1000 * cpu_time_deadline_optimization) {
		makespan = optimize_task_list(makespan, task_list, false);
		clock_t c_end = clock();
		time_elapsed_ms = 1000.0 * (c_end - c_start) / CLOCKS_PER_SEC;
	}

	cout << "CPU time used for optimization: " << time_elapsed_ms << " ms\nMakespan: " << makespan << "\n";

	vector<int> start_times = vector<int>(job_id.get_max_id(), 0);
	construct_schedule(task_list, start_times, job_id.get_max_id());
	//write_resource_schedule(makespan, start_times, task_list, "optimized_schedule");

	c_start = clock();
	time_elapsed_ms = 0.0;
	while (time_elapsed_ms < 1000 * cpu_time_deadline_preemption) {
		makespan = optimize_task_list(makespan, task_list, true);
		clock_t c_end = clock();
		time_elapsed_ms = 1000.0 * (c_end - c_start) / CLOCKS_PER_SEC;
	}

	cout << "CPU time used for optimization with preemption: " << time_elapsed_ms << " ms\nMakespan: " << makespan << "\n";

	start_times = vector<int>(job_id.get_max_id(), 0);
	construct_schedule(task_list, start_times, job_id.get_max_id());
	//write_resource_schedule(makespan, start_times, task_list, "preempted_schedule");

#pragma endregion


	return 0;
}

void parse_activities(string filename)
{

	// Create a text string, which is used to output the text file
	string tmp;

	// Read from the text file
	ifstream project_lib(filename);

	if (!project_lib.is_open()) {
		throw runtime_error("Could not open lib file");
	};

	int task_count;

	for (int i = 0; i < 5; i++)
	{
		getline(project_lib, tmp);
	};

	getline(project_lib, tmp, ':');
	project_lib >> task_count;

	getline(project_lib, tmp, ':');
	project_lib >> horizon;

	for (int i = 0; i < 2; i++)
	{
		getline(project_lib, tmp);
	};

	getline(project_lib, tmp, ':');
	project_lib >> resource_count;
	getline(project_lib, tmp);

	for (int i = 0; i < 9; i++)
	{
		getline(project_lib, tmp);
	};

	for (int i = 0; i < task_count; i++)
	{
		Task activity;
		activity.task_id = job_id.get_next_id();
		activity.split_task_id = activity.task_id;
		initial_tasks.push_back(activity);

		int successor_count;
		project_lib >> tmp >> tmp >> successor_count;
		vector<int> successors(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> successors[j];
			successors[j]--;
		};
		initial_tasks[i].successors = successors;
	}


	for (int i = 0; i < 5; i++)
	{
		getline(project_lib, tmp);
	};

	for (int i = 0; i < task_count; i++)
	{
		project_lib >> tmp >> tmp >> initial_tasks[i].duration;
		initial_tasks[i].resource_requirements = vector<int>(resource_count, 0);
		for (int j = 0; j < resource_count; j++)
		{
			project_lib >> initial_tasks[i].resource_requirements[j];
		};
	}

	for (int i = 0; i < 4; i++)
	{
		getline(project_lib, tmp);
	};

	for (int i = 0; i < resource_count; i++)
	{
		int resource_availability;
		project_lib >> resource_availability;
		resource_availabilities.push_back(resource_availability);
	}

	// Close the file
	project_lib.close();

	initial_task_count = task_count;
}

void extend_initial_successors(Task& task)
{
	if (task.split_task_id == initial_tasks[initial_tasks.size() - 1].split_task_id) {
		return;
	}
	for (int i = 0; i < task.successors.size(); i++)
	{
		extend_initial_successors(initial_tasks[task.successors[i]]);
		for (int j = 0; j < initial_tasks[task.successors[i]].successors.size(); j++)
		{
			if (find(task.successors.begin(), task.successors.end(), initial_tasks[task.successors[i]].successors[j]) == task.successors.end()) {
				task.successors.push_back(initial_tasks[task.successors[i]].successors[j]);
			}
		}
		sort(task.successors.begin(), task.successors.end());
	}
}

void calculate_rurs()
{
	for (int i = 0; i < initial_task_count; i++)
	{
		initial_tasks[i].calculate_rur(resource_availabilities);
	}
}

void fix_presedence_constraint(vector<Task>& task_list)
{
	vector<int> task_in_violation_at_index;

	bool presedence_violated = check_presedence_violation(task_list, task_in_violation_at_index);
	while (presedence_violated) {
		move(task_list, task_in_violation_at_index[0], task_in_violation_at_index[1]);
		presedence_violated = check_presedence_violation(task_list, task_in_violation_at_index);
	}
}

// Found on StackOverflow answer: https://stackoverflow.com/a/57399634
template <typename t> void move(vector<t>& v, size_t oldIndex, size_t newIndex)
{
	if (oldIndex > newIndex)
	{
		rotate(v.rend() - oldIndex - 1, v.rend() - oldIndex, v.rend() - newIndex);
	}
	else {
		rotate(v.begin() + oldIndex, v.begin() + oldIndex + 1, v.begin() + newIndex + 1);
	}
}

bool check_presedence_violation(vector<Task>& task_list, vector<int>& task_in_violation_at_index)
{
	for (int i = 1; i < task_list.size(); i++)
	{
		for (int j = 0; j < i; j++)
		{
			int sucessor_index = find(task_list[i].successors.begin(), task_list[i].successors.end(), task_list[j].split_task_id) - task_list[i].successors.begin();
			if (sucessor_index != task_list[i].successors.end() - task_list[i].successors.begin())
			{
				task_in_violation_at_index = { i, j };
				return true;
			}
		}
	}

	return false;
}

bool check_presedence_violation(vector<Task>& task_list)
{
	for (int i = 1; i < task_list.size(); i++)
	{
		for (int j = 0; j < i; j++)
		{
			int sucessor_index = find(task_list[i].successors.begin(), task_list[i].successors.end(), task_list[j].split_task_id) - task_list[i].successors.begin();
			if (sucessor_index != task_list[i].successors.end() - task_list[i].successors.begin())
			{
				return true;
			}
		}
	}

	return false;
}

void construct_initial_task_list(vector<Task>& task_list)
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
			if (!check_presedence_violation(new_task_list)) {
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

int calculate_makespan(vector<Task>& task_list, int task_count)
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

int construct_schedule(vector<Task>& task_list, vector<int>& start_times, int task_count)
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

int find_earliest_start_time(int duration, vector<int> resource_requirements, vector<vector<int>>& remaining_resources, int earliest_start_time)
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

int optimize_task_list(int makespan, vector<Task>& task_list, bool preempt_tasks)
{
	vector<Task> new_task_list = task_list;

	vector<int> destruction_ids;
	vector<vector<vector<int>>> destruction_split_ids;
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
	extend_successors(new_task_list[0], new_task_list);

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
					if (!check_presedence_violation(possible_task_list)) {
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

void extend_successors(Task& task, vector<Task>& task_list)
{
	if (task.split_task_id == initial_task_count - 1) {
		return;
	}
	for (int i = 0; i < task.successors.size(); i++)
	{
		int successor_id = task.successors[i];
		Task successor;
		bool successor_found = false;
		int j = 1;
		while (!successor_found)
		{
			if (task_list[j].split_task_id == successor_id)
			{
				successor = task_list[j];
				successor_found = true;
			}
			j++;
		}
		extend_successors(successor, task_list);
		for (int j = 0; j < successor.successors.size(); j++)
		{
			int successor_successor_id = successor.successors[j];
			if (find(task.successors.begin(), task.successors.end(), successor_successor_id) == task.successors.end())
			{
				task.successors.push_back(successor_successor_id);
			}
		}
		sort(task.successors.begin(), task.successors.end());
	}
}

//void write_resource_schedule(int makespan, vector<int>& start_times, vector<Task>& task_list, string filename)
//{
//	vector<vector<vector<int>>> resource_schedule;
//	vector<vector<vector<int>>> resource_schedule_preemption_ids;
//
//	vector<Task> scheduling_task_list = task_list;
//
//	bool found_possible_schedule = false;
//	while (!found_possible_schedule) {
//		try
//		{
//			try_build_3d_resource_schedule(scheduling_task_list, start_times, resource_schedule, resource_schedule_preemption_ids);
//			found_possible_schedule = true;
//		}
//		catch (const exception&)
//		{
//			shuffle(scheduling_task_list.begin(), scheduling_task_list.end(), default_random_engine());
//		}
//	}
//
//	ofstream schedule_file(filename + ".txt");
//	if (schedule_file.is_open())
//	{
//		schedule_file << "Makespan: " << makespan << "\n\n";
//		for (int i = 1; i <= makespan; i++)
//		{
//			schedule_file << i << '\t';
//		}
//		schedule_file << '\n' << '\t';
//		for (int i = 0; i < resource_count; i++)
//		{
//			schedule_file << "R " << i + 1 << '\n';
//			for (int j = 0; j < resource_schedule[i][0].size(); j++)
//			{
//				for (int k = 0; k < resource_schedule[i].size(); k++)
//				{
//					if (resource_schedule[i][k][j] == 0)
//					{
//						schedule_file << ' ' << '\t';
//					}
//					else {
//						schedule_file << resource_schedule[i][k][j] << '\t';
//					}
//				}
//				schedule_file << '\n';
//			}
//		}
//		schedule_file.close();
//	}
//
//	ofstream schedule_file_preemption_ids(filename + "_preemption_ids" + ".txt");
//	if (schedule_file_preemption_ids.is_open())
//	{
//		schedule_file_preemption_ids << "Makespan: " << makespan << "\n\n";
//		for (int i = 1; i <= makespan; i++)
//		{
//			schedule_file_preemption_ids << i << '\t' << '\t';
//		}
//		schedule_file_preemption_ids << '\n';
//		for (int i = 0; i < resource_count; i++)
//		{
//			schedule_file_preemption_ids << "R " << i + 1 << '\n';
//			for (int j = 0; j < resource_schedule_preemption_ids[i][0].size(); j++)
//			{
//				for (int k = 0; k < resource_schedule_preemption_ids[i].size(); k++)
//				{
//					if (resource_schedule_preemption_ids[i][k][j] == 0)
//					{
//						schedule_file_preemption_ids << ' ' << '\t';
//					}
//					else {
//						schedule_file_preemption_ids << resource_schedule_preemption_ids[i][k][j] << '\t';
//					}
//				}
//				schedule_file_preemption_ids << '\n';
//			}
//		}
//		schedule_file_preemption_ids.close();
//	}
//}

//void try_build_3d_resource_schedule(vector<Task>& task_list, vector<int>& start_times, vector<vector<vector<int>>>& resource_schedule, vector<vector<vector<int>>>& resource_schedule_preemption_ids)
//{
//	vector<vector<vector<int>>> new_resource_schedule;
//	vector<vector<vector<int>>> new_resource_schedule_preemption_ids;
//
//	for (int i = 0; i < resource_count; i++)
//	{
//		new_resource_schedule.push_back(vector<vector<int>>(start_times[initial_task_count - 1], vector<int>(resource_availabilities[i], 0)));
//		new_resource_schedule_preemption_ids.push_back(vector<vector<int>>(start_times[initial_task_count - 1], vector<int>(resource_availabilities[i], 0)));
//	}
//
//	for (int i = 0; i < task_list.size(); i++)
//	{
//		assert(task_list[i].split_task_id < start_times.size());
//		int start_time = start_times[task_list[i].split_task_id];
//		int resource_index = 0;
//		bool found_space = false;
//		while (!found_space)
//		{
//			bool space_error = false;
//			for (int j = start_time; j < start_time + task_list[i].duration; j++)
//			{
//				for (int k = 0; k < task_list[i].resource_requirement; k++)
//				{
//					assert(task_list[i].resource_type < new_resource_schedule.size());
//					assert(j < new_resource_schedule[task_list[i].resource_type].size());
//					if (k + resource_index >= new_resource_schedule[task_list[i].resource_type][j].size()) {
//						throw exception();
//					}
//					assert(k + resource_index < new_resource_schedule[task_list[i].resource_type][j].size());
//					if (new_resource_schedule[task_list[i].resource_type][j][k + resource_index] != 0) {
//						space_error = true;
//					}
//				}
//			}
//			if (space_error)
//			{
//				resource_index++;
//			}
//			else
//			{
//				found_space = true;
//			}
//		}
//		for (int j = start_time; j < start_time + task_list[i].duration; j++)
//		{
//			for (int k = 0; k < task_list[i].resource_requirement; k++)
//			{
//				new_resource_schedule[task_list[i].resource_type][j][k + resource_index] = task_list[i].task_id;
//				new_resource_schedule_preemption_ids[task_list[i].resource_type][j][k + resource_index] = task_list[i].split_task_id;
//			}
//		}
//	}
//
//	resource_schedule = new_resource_schedule;
//	resource_schedule_preemption_ids = new_resource_schedule_preemption_ids;
//}