#include "SAT-encoding.h"

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
	vector<unsigned long> start_variables;
	vector<unsigned long> process_variables;
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

struct CNF_variable_id
{
private:

	static CNF_variable_id* instance;
	CNF_variable_id() {};

	unsigned long next_unused_id = 0;
public:
	static CNF_variable_id* get_instance() {
		if (!instance) {
			instance = new CNF_variable_id;
		};
		return instance;
	}

	unsigned long get_id()
	{
		if (next_unused_id < ULONG_MAX) {
			return ++next_unused_id;
		}
		else {
			throw new exception("Max id value reached");
		}
	}

	unsigned long get_id_count()
	{
		return next_unused_id;
	}
};

void parse_input_file(string filename);
void write_cnf_file();
void preempt_tasks();
void preempt_task(Task task);
//void critical_path();
void forward_pass(pair<int, int> task_segment_id, int earliest_start);
void backward_pass(pair<int, int> task_segment_id, int late_finish);
void set_start_variables();
void set_process_variables();
void build_consistency_clauses(stringstream& cnf_file_content, int& clause_count);
void build_precedence_clauses(stringstream& cnf_file_content, int& clause_count);
void build_start_clauses(stringstream& cnf_file_content, int& clause_count);
void build_cover_clauses(stringstream& cnf_file_content, int& clause_count);
void build_objective_clauses(stringstream& cnf_file_content, int& clause_count);

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
CNF_variable_id* CNF_variable_id::instance = 0;
CNF_variable_id* cnf_variable_id = cnf_variable_id->get_instance();
const string cnf_file_type = "wcnf";
static int get_cnf_hard_clause_weight() {
	return horizon + 1;
}


int main()
{
	parse_input_file(project_lib_folder_j30 + project_lib_file_j30 + project_lib_type_j30);
	preempt_tasks();

	set_start_variables();
	set_process_variables();

	write_cnf_file();

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

//void critical_path()
//{
//	for (int j = 0; j < preempted_tasks[0].successors.size(); j++)
//	{
//		forward_pass(preempted_tasks[0].successors[j], 0);
//	}
//
//	for (int j = 0; j < preempted_tasks.size(); j++)
//	{
//		for (int k = 0; k < preempted_tasks[j].successors.size(); k++)
//		{
//			if (preempted_tasks[j].successors[k].first == preempted_tasks.back().id && preempted_tasks[j].successors[k].second == preempted_tasks.back().segment)
//			{
//				backward_pass(pair<int, int>(preempted_tasks[j].id, preempted_tasks[j].segment), horizon);
//			}
//		}
//	}
//}
//
//void forward_pass(pair<int, int> task_segment_id, int early_start)
//{
//	for (int i = 0; i < preempted_tasks.size(); i++)
//	{
//		if (preempted_tasks[i].id == task_segment_id.first && preempted_tasks[i].segment == task_segment_id.second)
//		{
//			if (early_start > preempted_tasks[i].early_start) {
//				preempted_tasks[i].early_start = early_start;
//				preempted_tasks[i].early_finish = early_start + preempted_tasks[i].duration;
//				for (int j = 0; j < preempted_tasks[i].successors.size(); j++)
//				{
//					forward_pass(preempted_tasks[i].successors[j], preempted_tasks[i].early_finish);
//				}
//			}
//		}
//	}
//}
//
//void backward_pass(pair<int, int> task_segment_id, int late_finish)
//{
//	for (int i = 0; i < preempted_tasks.size(); i++)
//	{
//		if (preempted_tasks[i].id == task_segment_id.first && preempted_tasks[i].segment == task_segment_id.second)
//		{
//			if (late_finish < preempted_tasks[i].late_finish) {
//				preempted_tasks[i].late_finish = late_finish;
//				preempted_tasks[i].late_start = late_finish - preempted_tasks[i].duration;
//
//				for (int j = 0; j < preempted_tasks.size(); j++)
//				{
//					for (int k = 0; k < preempted_tasks[j].successors.size(); k++)
//					{
//						if (preempted_tasks[j].successors[k].first == task_segment_id.first && preempted_tasks[j].successors[k].second == task_segment_id.second)
//						{
//							backward_pass(pair<int, int>(preempted_tasks[j].id, preempted_tasks[j].segment), preempted_tasks[i].late_start);
//						}
//					}
//				}
//			}
//		}
//	}
//}

void set_start_variables()
{
	for (int i = 1; i < preempted_tasks.size() - 1; i++)
	{
		for (int t = 1; t <= horizon - preempted_tasks[i].duration + 1; t++)
		{
			preempted_tasks[i].start_variables.push_back(cnf_variable_id->get_id());
		}
	}
	for (int t = 1; t <= horizon; t++)
	{
		preempted_tasks.back().start_variables.push_back(cnf_variable_id->get_id());
	}
}

void set_process_variables()
{
	for (int i = 1; i < preempted_tasks.size() - 1; i++)
	{
		for (int t = 1; t <= horizon; t++)
		{
			preempted_tasks[i].process_variables.push_back(cnf_variable_id->get_id());
		}
	}
}

void write_cnf_file()
{
	stringstream cnf_file_content;
	int clause_count = 0;
	build_start_clauses(cnf_file_content, clause_count);
	build_precedence_clauses(cnf_file_content, clause_count);
	build_objective_clauses(cnf_file_content, clause_count);
	build_consistency_clauses(cnf_file_content, clause_count);
	build_cover_clauses(cnf_file_content, clause_count);

	ofstream schedule_file(project_lib_file_j30 + '.' + cnf_file_type);
	if (schedule_file.is_open())
	{
		// p FORMAT VARIABLES CLAUSES
		schedule_file << 'p' << ' ' << cnf_file_type << ' ' << cnf_variable_id->get_id_count() << ' ' << clause_count << ' ' << get_cnf_hard_clause_weight() << '\n';
		schedule_file << cnf_file_content.str();
		schedule_file.close();
	}
}

void build_start_clauses(stringstream& cnf_file_content, int& clause_count)
{
	for (int id = 1; id < parsed_tasks.size() - 1; id++)
	{
		for (int segment = 1; segment <= parsed_tasks[id].duration; segment++)
		{
			vector<Task> matched_tasks;
			for (int i = 0; i < preempted_tasks.size(); i++)
			{
				if (preempted_tasks[i].id == parsed_tasks[id].id && preempted_tasks[i].segment <= segment && segment <= preempted_tasks[i].segment + preempted_tasks[i].duration - 1) {
					matched_tasks.push_back(preempted_tasks[i]);
				}
			}

			cnf_file_content << get_cnf_hard_clause_weight() << ' ';
			for each (Task matched_task in matched_tasks)
			{
				for each (unsigned long start_variable in matched_task.start_variables)
				{
					cnf_file_content << start_variable << ' ';
				}
			}
			cnf_file_content << '0' << '\n';
			clause_count++;

			for (int i = 0; i < matched_tasks.size(); i++)
			{
				for (int j = i + 1; j < matched_tasks.size(); j++)
				{
					for (int k = 0; k < matched_tasks[i].start_variables.size(); k++)
					{
						for (int l = 0; l < matched_tasks[j].start_variables.size(); l++)
						{
							cnf_file_content << get_cnf_hard_clause_weight() << ' ';
							cnf_file_content << '-' << matched_tasks[i].start_variables[k] << ' ';
							cnf_file_content << '-' << matched_tasks[j].start_variables[l] << ' ';
							cnf_file_content << '0' << '\n';
							clause_count++;
						}
					}
				}
			}
		}
	}
	//for (int i = 1; i < preempted_tasks.size(); i++)
	//{
	//	cnf_file_content << get_cnf_hard_clause_weight() << ' ';
	//	for (int t = 0; t < preempted_tasks[i].start_variables.size(); t++)
	//	{
	//		cnf_file_content << preempted_tasks[i].start_variables[t] << ' ';
	//	}
	//	cnf_file_content << '0' << '\n';
	//	clause_count++;
	//}
}

void build_precedence_clauses(stringstream& cnf_file_content, int& clause_count)
{
	//for (int i = 1; i < preempted_tasks.size(); i++)
	//{
	//	for (int j = 1; j < preempted_tasks.size(); j++)
	//	{

	//		bool has_precedence = false;
	//		for (int k = 0; k < preempted_tasks[i].successors.size(); k++)
	//		{
	//			if (preempted_tasks[i].successors[k].first == preempted_tasks[j].id && preempted_tasks[i].successors[k].second == preempted_tasks[j].segment)
	//			{
	//				has_precedence = true;
	//			}
	//		}

	//		if (has_precedence) {
	//			for (int t = 0; t < preempted_tasks[j].start_variables.size(); t++)
	//			{
	//				int start_index = t - preempted_tasks[i].duration < 0 ? 0 : t - preempted_tasks[i].duration;
	//				for (int p = start_index; p < preempted_tasks[i].start_variables.size(); p++)
	//				{
	//					cnf_file_content << get_cnf_hard_clause_weight() << ' ';
	//					cnf_file_content << '-' << preempted_tasks[j].start_variables[t] << ' ';
	//					cnf_file_content << '-' << preempted_tasks[i].start_variables[p] << ' ';
	//					cnf_file_content << '0' << '\n';
	//					clause_count++;
	//				}
	//			}
	//		}
	//	}
	//}
}

void build_cover_clauses(stringstream& cnf_file_content, int& clause_count)
{
}

void build_objective_clauses(stringstream& cnf_file_content, int& clause_count)
{
	//for (int i = 0; i < preempted_tasks.back().start_variables.size(); i++)
	//{
	//	//cnf_file_content << (preempted_tasks.back().late_start - preempted_tasks.back().early_start - i) << ' ';
	//	cnf_file_content << 1 << ' ';
	//	cnf_file_content << preempted_tasks.back().start_variables[i] << ' ';
	//	cnf_file_content << '0' << '\n';
	//	clause_count++;
	//}
}

void build_consistency_clauses(stringstream& cnf_file_content, int& clause_count)
{
	//for (int i = 0; i < preempted_tasks.size(); i++)
	//{
	//	for (int t_i = 0; t_i <= preempted_tasks[i].late_start - preempted_tasks[i].early_start; t_i++)
	//	{
	//		for (int l = t_i; l <= t_i + preempted_tasks[i].duration - 1; l++)
	//		{
	//			cnf_file_content << get_cnf_hard_clause_weight() << ' ' << '-' << preempted_tasks[i].start_variables[t_i] << ' ' << preempted_tasks[i].process_variables[l] << ' ' << '0' << '\n';
	//			clause_count++;
	//		}
	//	}
	//}
}