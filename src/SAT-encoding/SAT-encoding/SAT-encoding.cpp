#include "SAT-encoding.h"
#include "../pblib/pblib/pb2cnf.h"

using namespace std;

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

	static Task_Id* instance;
	Task_Id() {};

	int next_unused_id = 0;
	vector<int> reusable_ids;
public:
	static Task_Id* get_instance() {
		if (!instance) {
			instance = new Task_Id;
		};
		return instance;
	}

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

	static CNF_variable* instance;
	CNF_variable() {};

	int32_t last_used_variable = 0;
public:
	static CNF_variable* get_instance() {
		if (!instance) {
			instance = new CNF_variable;
		};
		return instance;
	}

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
void calculate_rurs(vector<Task>& task_list);
void fix_presedence_constraint(vector<Task>& task_list);
bool check_presedence_violation(vector<Task>& task_list, vector<int>& task_in_violation_at_index);
template <typename t> void move(vector<t>& v, size_t oldIndex, size_t newIndex);
void construct_upper_bound_task_list(vector<Task>& task_list);
int calculate_sgs_makespan(vector<Task>& task_list);
int find_earliest_start_time(int duration, vector<int> resource_requirements, vector<vector<int>>& remaining_resources, int earliest_start_time);
void write_cnf_file(vector<Task>& task_list);
void build_consistency_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
void build_precedence_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
void build_completion_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
void build_resource_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
void build_objective_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
string extract_filename_without_extention(string& file_path);

// Global algorithm variables
const int setup_time = 5;

// Global variables for the specific problem instance used
const string project_lib_folder_j30 = "C:/Projects/cse3000/src/parsed-datasets/DC1/";
string project_lib_file_j30 = "mv1.RCP";

Task_Id* Task_Id::instance = 0;
Task_Id* unique_task_id = unique_task_id->get_instance();

int horizon;
int optimal_solution;
vector<Task> parsed_tasks;
vector<int> resource_availabilities;
vector<Task> preempted_tasks;

// Global CNF construction variables
int upper_bound_makespan;
PB2CNF pb2cnf;
CNF_variable* CNF_variable::instance = 0;
CNF_variable* cnf_variable = cnf_variable->get_instance();
const string cnf_file_type = "wcnf";
static int get_cnf_hard_clause_weight() {
	return ((horizon * (horizon + 1)) / 2) + 1;
}
const bool write_comments = false;

int main()
{
#pragma region setup
	parse_input_file(project_lib_folder_j30 + project_lib_file_j30);

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
	preempt_tasks();

	set_start_variables();
	set_process_variables();

	vector<Task> reduced_preempted_tasks = preempted_tasks;
	remove_duplicate_segments(reduced_preempted_tasks);
#pragma endregion

#pragma region CNF encoding
	write_cnf_file(preempted_tasks);
#pragma endregion


	Task back_task = preempted_tasks.back();

	return 0;
}

void parse_input_file(string filename)
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
		parsed_tasks[i].id = unique_task_id->get_unused_id();
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

void critical_path()
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

void forward_pass(pair<int, int> task_segment_id, int early_start)
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

void backward_pass(pair<int, int> task_segment_id, int late_finish)
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

void preempt_tasks()
{
	preempted_tasks.push_back(parsed_tasks.front());
	for each (Task parsed_task in parsed_tasks)
	{
		preempt_task(parsed_task);
	}
	preempted_tasks.push_back(parsed_tasks.back());
}

void preempt_task(Task task)
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

void set_start_variables()
{
	for (int i = 1; i < preempted_tasks.size(); i++)
	{
		for (int t = preempted_tasks[i].early_start; t <= preempted_tasks[i].late_finish - preempted_tasks[i].duration; t++)
		{
			preempted_tasks[i].start_variables.push_back(cnf_variable->get_variable());
		}
	}
}

void set_process_variables()
{
	for (int i = 1; i < preempted_tasks.size() - 1; i++)
	{
		for (int t = preempted_tasks[i].early_start; t <= preempted_tasks[i].late_finish; t++)
		{
			preempted_tasks[i].process_variables.push_back(cnf_variable->get_variable());
		}
	}
}

void remove_duplicate_segments(vector<Task>& task_list)
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

void calculate_rurs(vector<Task>& task_list)
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

void fix_presedence_constraint(vector<Task>& task_list)
{
	vector<int> task_in_violation_at_index;

	bool presedence_violated = check_presedence_violation(task_list, task_in_violation_at_index);
	while (presedence_violated) {
		move(task_list, task_in_violation_at_index[0], task_in_violation_at_index[1]);
		presedence_violated = check_presedence_violation(task_list, task_in_violation_at_index);
	}
}

bool check_presedence_violation(vector<Task>& task_list, vector<int>& task_in_violation_at_index)
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

	//for (int i = 1; i < task_list.size(); i++)
	//{
	//	for (int j = 0; j < i; j++)
	//	{
	//		for (int k = 0; k < task_list[i].successors.size(); k++)
	//		{
	//			if (task_list[i].successors[k].first == task_list[j].id && task_list[i].successors[k].second == task_list[j].segment)
	//			{
	//				task_in_violation_at_index = { i, j };
	//				return true;
	//			}
	//		}
	//	}
	//}

	//return false;
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

void construct_upper_bound_task_list(vector<Task>& task_list)
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
			vector<int> tmp;
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

int calculate_sgs_makespan(vector<Task>& task_list)
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

void write_cnf_file(vector<Task>& task_list)
{
	stringstream cnf_file_content;
	int clause_count = 0;
	build_completion_clauses(task_list, cnf_file_content, clause_count);
	build_precedence_clauses(task_list, cnf_file_content, clause_count);
	build_consistency_clauses(task_list, cnf_file_content, clause_count);
	build_resource_clauses(task_list, cnf_file_content, clause_count);
	build_objective_clauses(task_list, cnf_file_content, clause_count);

	ofstream schedule_file(extract_filename_without_extention(project_lib_file_j30) + '.' + cnf_file_type);
	if (schedule_file.is_open())
	{
		// p FORMAT VARIABLES CLAUSES
		schedule_file << 'p' << ' ' << cnf_file_type << ' ' << cnf_variable->get_variable_count() << ' ' << clause_count << ' ' << get_cnf_hard_clause_weight() << '\n';
		schedule_file << cnf_file_content.str();
		schedule_file.close();
	}
}

void build_completion_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{
	if (write_comments) {
		cnf_file_content << 'c' << ' ' << "Completion clauses" << '\n';
	}

	for each (Task task in parsed_tasks)
	{
		if (task.id == 0)
		{
			continue;
		}

		vector<Task> task_segments;

		for each (Task task_segment in task_list)
		{
			if (task_segment.id == task.id) {
				task_segments.push_back(task_segment);
			}
		}

		for (int time_slot = 1; time_slot <= task.duration; time_slot++)
		{
			vector<Task> active_task_segments;
			for each (Task task_segment in task_segments)
			{
				if (task_segment.segment <= time_slot && task_segment.segment + task_segment.duration - 1 >= time_slot) {
					active_task_segments.push_back(task_segment);
				}
			}
			cnf_file_content << get_cnf_hard_clause_weight() << ' ';
			for each (Task active_task_segment in active_task_segments)
			{
				for each (int32_t start_variable in active_task_segment.start_variables)
				{
					cnf_file_content << start_variable << ' ';
				}
			}
			cnf_file_content << '0' << '\n';
			clause_count++;
		}

	}
}

void build_precedence_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{
	for each (Task successor in task_list)
	{
		if (successor.id == 0)
		{
			continue;
		}

		vector<vector<Task>> predecessor_groups(parsed_tasks.size());
		for each (Task predecessor in task_list)
		{
			if (predecessor.id == 0)
			{
				continue;
			}

			for each (pair<int, int> successor_id_segment in predecessor.successors)
			{
				if (successor.id == successor_id_segment.first && successor.segment == successor_id_segment.second)
				{
					predecessor_groups[predecessor.id].push_back(predecessor);
				}
			}
		}

		for each (vector<Task> predecessor_group in predecessor_groups)
		{
			for (int i = 0; i < successor.start_variables.size(); i++)
			{
				if (predecessor_group.size() > 0) {
					cnf_file_content << get_cnf_hard_clause_weight() << ' ';
					cnf_file_content << '-' << successor.start_variables[i] << ' ';
					for each (Task predecessor in predecessor_group)
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

void build_consistency_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{
	for each (Task task in task_list)
	{
		if (task.id == 0)
		{
			continue;
		}

		for (int i = 0; i < task.start_variables.size(); i++)
		{
			if (write_comments) {
				cnf_file_content << 'c' << ' ' << "task: " << task.id << "; duration: " << task.duration << "; start at: " << task.early_start + i << "; in process between: [" << i << ", " << i + task.duration - 1 << ']' << '\n';
			}
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

void build_resource_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{
	for (int i = 0; i < resource_availabilities.size(); i++)
	{
		for (int j = 0; j <= upper_bound_makespan; j++)
		{
			vector<int64_t> weights;
			vector<int32_t> literals;
			if (write_comments) {
				cnf_file_content << 'c' << ' ' << "resource: " << i << "; timeslot: " << j << '\n';
				cnf_file_content << 'c' << ' ' << "possible active tasks: ";
			}
			for each (Task task in task_list)
			{
				if (task.duration == 0)
				{
					continue;
				}
				if (task.resource_requirements[i] > 0 && task.early_start <= j && task.late_finish >= j)
				{
					literals.push_back(task.process_variables[j - task.early_start]);
					weights.push_back(task.resource_requirements[i]);
					if (write_comments) {
						cnf_file_content << task.id + 1 << ' ' << '[' << task.early_start << ", " << task.late_finish << ']' << ", ";
					}
				}
			}
			if (write_comments) {
				cnf_file_content << '\n';
			}

			vector<vector<int32_t>> formula;
			int32_t first_fresh_variable = cnf_variable->get_variable_count() + 1;
			int resource_availability = resource_availabilities[i];
			first_fresh_variable = pb2cnf.encodeLeq(weights, literals, resource_availability, formula, first_fresh_variable) + 1;
			cnf_variable->set_last_used_variable(first_fresh_variable - 1);

			for each (vector<int32_t> disjunction in formula)
			{
				cnf_file_content << get_cnf_hard_clause_weight() << ' ';
				for each (int32_t variable in disjunction)
				{
					cnf_file_content << variable << ' ';
				}
				cnf_file_content << '0' << '\n';
				clause_count++;
			}
		}
	}
}

void build_objective_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count)
{
	Task finish = task_list.back();

	int32_t latest_start_variable = cnf_variable->get_variable();
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
string extract_filename_without_extention(string& file_path)
{
	string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
	string::size_type const p(base_filename.find_last_of('.'));
	return base_filename.substr(0, p);
}