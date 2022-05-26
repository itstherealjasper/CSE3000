#include "SAT-encoding.h"
#include "../pblib/pblib/pb2cnf.h"

using namespace std;

struct Task
{
	int id;
	int segment;
	vector<pair<int, int>> successors;
	int duration;
	int resource_type;
	int resource_requirement;

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
void write_cnf_file(vector<Task>& task_list);
void build_consistency_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
void build_precedence_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
void build_completion_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
void build_resource_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);
void build_objective_clauses(vector<Task>& task_list, stringstream& cnf_file_content, int& clause_count);

// Global algorithm variables
const int setup_time = 1;

// Global variables for the specific problem instance used
const string project_lib_folder_j30 = "C:/Projects/cse3000/src/j30.sm/";
const string project_lib_file_j30 = "j301_1";
const string project_lib_type_j30 = ".sm";

Task_Id* Task_Id::instance = 0;
Task_Id* unique_task_id = unique_task_id->get_instance();

int horizon;
vector<Task> parsed_tasks;
vector<int> resource_availabilities;
vector<Task> preempted_tasks;

// Global CNF construction variables
PB2CNF pb2cnf;
CNF_variable* CNF_variable::instance = 0;
CNF_variable* cnf_variable = cnf_variable->get_instance();
const string cnf_file_type = "wcnf";
static int get_cnf_hard_clause_weight() {
	return horizon + 1;
}


int main()
{
	parse_input_file(project_lib_folder_j30 + project_lib_file_j30 + project_lib_type_j30);
	critical_path();
	preempt_tasks();

	set_start_variables();
	set_process_variables();

	vector<Task> reduced_preempted_tasks = preempted_tasks;
	remove_duplicate_segments(reduced_preempted_tasks);

	write_cnf_file(reduced_preempted_tasks);

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
	int resource_count;
	project_lib >> resource_count;
	getline(project_lib, tmp);

	for (int i = 0; i < 9; i++)
	{
		getline(project_lib, tmp);
	};

	for (int i = 0; i < task_count; i++)
	{
		Task task;
		task.id = unique_task_id->get_unused_id();
		task.segment = 1;
		parsed_tasks.push_back(task);

		int successor_count;
		project_lib >> tmp >> tmp >> successor_count;
		vector<pair<int, int>> successors(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> successors[j].first;
			successors[j].first--;
			successors[j].second = 1;
		};
		parsed_tasks[i].successors = successors;
	}


	for (int i = 0; i < 5; i++)
	{
		getline(project_lib, tmp);
	};

	for (int i = 0; i < task_count; i++)
	{
		project_lib >> tmp >> tmp >> parsed_tasks[i].duration;
		int max_resource_requirement = -1;
		for (int j = 0; j < resource_count; j++)
		{
			int parsed_resource_requirement;
			project_lib >> parsed_resource_requirement;

			if (parsed_resource_requirement > max_resource_requirement) {
				max_resource_requirement = parsed_resource_requirement;
				parsed_tasks[i].resource_requirement = parsed_resource_requirement;
				parsed_tasks[i].resource_type = j;
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

void critical_path()
{
	parsed_tasks[0].early_start = 0;
	parsed_tasks[0].early_finish = -1;
	for (int j = 0; j < parsed_tasks[0].successors.size(); j++)
	{
		forward_pass(parsed_tasks[0].successors[j], 0);
	}

	parsed_tasks.back().late_start = horizon + 1;
	parsed_tasks.back().late_finish = horizon;
	for (int j = 0; j < parsed_tasks.size(); j++)
	{
		for (int k = 0; k < parsed_tasks[j].successors.size(); k++)
		{
			if (parsed_tasks[j].successors[k].first == parsed_tasks.back().id && parsed_tasks[j].successors[k].second == parsed_tasks.back().segment)
			{
				backward_pass(pair<int, int>(parsed_tasks[j].id, parsed_tasks[j].segment), horizon);
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
			task_part.duration = j;
			if (task_part.segment + task_part.duration != task.segment + task.duration) {
				task_part.successors = vector<pair<int, int>>{ pair<int, int>(task.id, task_part.segment + task_part.duration) };
			}
			preempted_tasks.push_back(task_part);
		}
	}
}

void set_start_variables()
{
	for (int i = 1; i < preempted_tasks.size() - 1; i++)
	{
		for (int t = preempted_tasks[i].early_start; t <= preempted_tasks[i].late_start; t++)
		{
			preempted_tasks[i].start_variables.push_back(cnf_variable->get_variable());
		}
	}
	for (int t = 0; t <= horizon; t++)
	{
		preempted_tasks.back().start_variables.push_back(cnf_variable->get_variable());
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

void write_cnf_file(vector<Task>& task_list)
{
	stringstream cnf_file_content;
	int clause_count = 0;
	build_completion_clauses(task_list, cnf_file_content, clause_count);
	build_precedence_clauses(task_list, cnf_file_content, clause_count);
	build_consistency_clauses(task_list, cnf_file_content, clause_count);
	build_resource_clauses(task_list, cnf_file_content, clause_count);
	//build_objective_clauses(task_list, cnf_file_content, clause_count);

	ofstream schedule_file(project_lib_file_j30 + '.' + cnf_file_type);
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
	for each (Task task in task_list)
	{
		if (task.id == 0)
		{
			continue;
		}

		cnf_file_content << get_cnf_hard_clause_weight() << ' ';
		for each (int32_t start_variable in task.start_variables)
		{
			cnf_file_content << start_variable << ' ';
		}
		cnf_file_content << '0' << '\n';
		clause_count++;
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
					for (int i = 0; i < successor.start_variables.size(); i++)
					{
						assert(i + successor.early_start - predecessor.duration - predecessor.early_start >= 0);
						assert(predecessor.start_variables.size() >= 0);
						cnf_file_content << get_cnf_hard_clause_weight() << ' ';
						cnf_file_content << '-' << successor.start_variables[i] << ' ';
						for (int j = 0; j <= i + successor.early_start - predecessor.duration - predecessor.early_start && j < predecessor.start_variables.size(); j++)
						{
							cnf_file_content << predecessor.start_variables[j] << ' ';
						}
						cnf_file_content << '0' << '\n';
						clause_count++;
					}


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
		for (int j = 0; j <= horizon; j++)
		{
			vector< int64_t > weights;
			vector< int32_t > literals;
			for each (Task task in task_list)
			{
				if (task.duration == 0)
				{
					continue;
				}
				if (task.resource_type == i && task.early_start <= j && task.late_finish >= j)
				{
					literals.push_back(task.process_variables[j - task.early_start]);
					weights.push_back(task.resource_requirement);
				}
			}
			vector< vector< int32_t > > formula;
			int32_t first_fresh_variable = cnf_variable->get_variable_count() + 1;
			first_fresh_variable = pb2cnf.encodeLeq(weights, literals, 11, formula, first_fresh_variable) + 1;
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

}