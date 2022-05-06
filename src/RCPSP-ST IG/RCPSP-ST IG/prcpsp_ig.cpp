#include "rcpsp.h"

using namespace std;


struct Task
{
	int task_id;
	vector<int> successors;
	int duration;
	int resource_type = 0; // default value required
	int resource_requirement = 0; // default value required
	double rur; // resource utility rate

	void calculate_rur(vector<int>& resource_availabilities)
	{
		rur = (double)duration * (double)resource_requirement / (double)resource_availabilities[resource_type];
	}

	bool operator > (const Task& o) const
	{
		return (rur > o.rur);
	}
};

void parse_activities(string filename);
void extend_successors(Task& task);
void calculate_rurs();
void fix_presedence_constraint(vector<Task>& task_list);
template <typename t> void move(vector<t>& v, size_t oldIndex, size_t newIndex);
bool check_presedence_violation(vector<Task>& task_list);
bool check_presedence_violation(vector<Task>& task_list, vector<int>& task_in_violation_at_index);
void construct_initial_task_list(vector<Task>& task_list);
int calculate_makespan(vector<Task>& task_list);
int find_earliest_start_time(int duration, int resource_type, int resource_requirement, vector<vector<int>>& remaining_resources, int earliest_start_time);
int construct_schedule(vector<Task>& task_list, vector<int>& start_times);

// Parameters to tune the algorithm
const int destruction_count = 10;
const int cpu_time_deadline = 1; // seconds

// Global variables for the specific problem instance used
const string project_lib_filename = "C:/Projects/cse3000/src/RCPSP-ST IG/RCPSP-ST IG/j30.sm/j301_1.sm";
//const string project_lib_filename = "C:/Projects/cse3000/src/RCPSP-ST IG/RCPSP-ST IG/j120.sm/j1201_1.sm";
int horizon;
int task_count;
vector<Task> tasks;
int resource_count;
vector<int> resource_availabilities;

int main()
{
#pragma region setup
	parse_activities(project_lib_filename);
	extend_successors(tasks[0]);
	calculate_rurs();
#pragma endregion

#pragma region initial schedule
	vector<Task> initial_task_list = tasks;
	vector<int> initial_start_times;
	int initial_makespan;

	sort(initial_task_list.begin() + 1, initial_task_list.end() - 1, greater<Task>());
	fix_presedence_constraint(initial_task_list);
	construct_initial_task_list(initial_task_list);
	initial_makespan = construct_schedule(initial_task_list, initial_start_times);
	cout << "Initial scheduling done \nMakespan: " << initial_makespan << "\n";
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

	// Use a while loop together with the getline() function to read the file line by line
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
		activity.task_id = i;
		tasks.push_back(activity);

		int successor_count;
		project_lib >> tmp >> tmp >> successor_count;
		vector<int> successors(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> successors[j];
			successors[j]--;
		};
		tasks[i].successors = successors;
	}


	for (int i = 0; i < 5; i++)
	{
		getline(project_lib, tmp);
	};

	for (int i = 0; i < task_count; i++)
	{
		project_lib >> tmp >> tmp >> tasks[i].duration;
		int resource_requirement;
		for (int j = 0; j < resource_count; j++)
		{
			int parsed_resource_requirement;
			project_lib >> parsed_resource_requirement;

			if (parsed_resource_requirement > 0) {
				tasks[i].resource_requirement = parsed_resource_requirement;
				tasks[i].resource_type = j;
			}
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
}

void extend_successors(Task& task)
{
	if (task.task_id == tasks[tasks.size() - 1].task_id) {
		return;
	}
	for (int i = 0; i < task.successors.size(); i++)
	{
		extend_successors(tasks[task.successors[i]]);
		for (int j = 0; j < tasks[task.successors[i]].successors.size(); j++)
		{
			if (find(task.successors.begin(), task.successors.end(), tasks[task.successors[i]].successors[j]) == task.successors.end()) {
				task.successors.push_back(tasks[task.successors[i]].successors[j]);
			}
		}
		sort(task.successors.begin(), task.successors.end());
	}
}

void calculate_rurs()
{
	for (int i = 0; i < task_count; i++)
	{
		tasks[i].calculate_rur(resource_availabilities);
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
			int sucessor_index = find(task_list[i].successors.begin(), task_list[i].successors.end(), task_list[j].task_id) - task_list[i].successors.begin();
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
			int sucessor_index = find(task_list[i].successors.begin(), task_list[i].successors.end(), task_list[j].task_id) - task_list[i].successors.begin();
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

	for (int i = 2; i < task_count; i++)
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
				int makespan_test = calculate_makespan(new_task_list);
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

int calculate_makespan(vector<Task>& task_list)
{
	vector<vector<int>> remaining_resources;
	for (int i = 0; i < horizon; i++)
	{
		remaining_resources.push_back(resource_availabilities);
	}

	vector<int> start_times(task_count, 0);

	for (int i = 0; i < task_list.size(); i++)
	{
		int earliest_start_time = find_earliest_start_time(task_list[i].duration, task_list[i].resource_type, task_list[i].resource_requirement, remaining_resources, start_times[task_list[i].task_id]);
		start_times[task_list[i].task_id] = earliest_start_time;
		for (int j = 0; j < task_list[i].duration; j++)
		{
			remaining_resources[earliest_start_time + j][task_list[i].resource_type] = remaining_resources[earliest_start_time + j][task_list[i].resource_type] - task_list[i].resource_requirement;
		}

		for (int j = 0; j < task_list[i].successors.size(); j++)
		{
			if (earliest_start_time + task_list[i].duration > start_times[task_list[i].successors[j]])
			{
				start_times[task_list[i].successors[j]] = earliest_start_time + task_list[i].duration;
			}
		}
	}

	return start_times.back();
}

int construct_schedule(vector<Task>& task_list, vector<int>& start_times)
{
	vector<vector<int>> remaining_resources;
	for (int i = 0; i < horizon; i++)
	{
		remaining_resources.push_back(resource_availabilities);
	}

	vector<int> new_start_times(task_count, 0);

	for (int i = 0; i < task_list.size(); i++)
	{
		int earliest_start_time = find_earliest_start_time(task_list[i].duration, task_list[i].resource_type, task_list[i].resource_requirement, remaining_resources, new_start_times[task_list[i].task_id]);
		new_start_times[task_list[i].task_id] = earliest_start_time;
		for (int j = 0; j < task_list[i].duration; j++)
		{
			remaining_resources[earliest_start_time + j][task_list[i].resource_type] = remaining_resources[earliest_start_time + j][task_list[i].resource_type] - task_list[i].resource_requirement;
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

int find_earliest_start_time(int duration, int resource_type, int resource_requirement, vector<vector<int>>& remaining_resources, int earliest_start_time)
{
	bool found_possible_timeslot = false;
	while (!found_possible_timeslot) {
		bool requirements_to_high = false;
		for (int i = 0; i < duration; i++)
		{
			if (resource_requirement > remaining_resources[earliest_start_time + i][resource_type])
			{
				requirements_to_high = true;
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