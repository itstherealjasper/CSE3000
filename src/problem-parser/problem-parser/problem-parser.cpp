#include "problem-parser.h"

using namespace std;

struct Task
{
	int id;
	vector<int> successors;
	int duration;
	int resource_type;
	int resource_requirement;
};

void write_custom_project_file(string file_path, int horizon, vector<Task> parsed_tasks, vector<int> resource_availabilities);
void parse_j30_file(string file_path);
void parse_DC1_file(string file_path);
void parse_RG30_file(string file_path);

const string input_lib_folder = "C:/Projects/cse3000/src/datasets/";
const string output_lib_folder = "C:/Projects/cse3000/src/parsed-datasets/";

const string j30_sub_folder = "j30.sm/";
const string j30_lib_file = "j301_1.sm";

const string DC1_sub_folder = "DC1/";
const string DC1_lib_file = "mv1.RCP";

const string RG30_sub_folder = "RG30/";
const string RG30_set1_folder = "Set1/";
const string RG30_set2_folder = "Set2/";
const string RG30_set3_folder = "Set3/";
const string RG30_set4_folder = "Set4/";
const string RG30_set5_folder = "Set5/";
const string RG30_lib_file = "Pat1.RCP";

int main(int argc, char* argv[])
{
	if (argc > 1) {

	}
	else {
		parse_j30_file(input_lib_folder + j30_sub_folder + j30_lib_file);
		parse_DC1_file(input_lib_folder + DC1_sub_folder + DC1_lib_file);
		parse_RG30_file(input_lib_folder + RG30_sub_folder + RG30_set1_folder + RG30_lib_file);
	}

	return 0;
}

void write_custom_project_file(string file_path, int horizon, vector<Task> parsed_tasks, vector<int> resource_availabilities)
{
	stringstream lib_file_content;

	lib_file_content << "h;" << horizon << '\n';
	lib_file_content << "s;" << parsed_tasks.size() << '\n';
	lib_file_content << "r;" << resource_availabilities.size() << '\n';
	lib_file_content << '\n';
	lib_file_content << resource_availabilities[0];
	for (int i = 1; i < resource_availabilities.size(); i++)
	{
		lib_file_content << ';' << resource_availabilities[i];
	}
	lib_file_content << '\n';
	lib_file_content << '\n';

	for (int i = 0; i < parsed_tasks.size(); i++)
	{
		lib_file_content << parsed_tasks[i].id << ';' << parsed_tasks[i].resource_type << ';' << parsed_tasks[i].resource_requirement << ';' << parsed_tasks[i].duration << ';' << parsed_tasks[i].successors.size();

		for (int j = 0; j < parsed_tasks[i].successors.size(); j++)
		{
			lib_file_content << ';' << parsed_tasks[i].successors[j];
		}
		lib_file_content << '\n';
	}

	ofstream schedule_file(file_path);
	if (schedule_file.is_open())
	{
		schedule_file << lib_file_content.str();
		schedule_file.close();
	}
}

void parse_j30_file(string file_path)
{
	string tmp;

	ifstream project_lib(file_path);

	if (!project_lib.is_open()) {
		throw runtime_error("Could not open lib file");
	};

	int task_count;
	int horizon;
	int resource_count;
	vector<Task> parsed_tasks;
	vector<int> resource_availabilities;

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
		Task task;
		task.id = i;
		parsed_tasks.push_back(task);

		int successor_count;
		project_lib >> tmp >> tmp >> successor_count;
		vector<int> successors(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> successors[j];
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
	write_custom_project_file(output_lib_folder + j30_sub_folder + j30_lib_file, horizon, parsed_tasks, resource_availabilities);
}

void parse_DC1_file(string file_path)
{
	string tmp;

	ifstream project_lib(file_path);

	if (!project_lib.is_open()) {
		throw runtime_error("Could not open lib file");
	};

	int task_count;
	int horizon = 0;
	int resource_count;
	vector<Task> parsed_tasks;
	vector<int> resource_availabilities;

	project_lib >> task_count >> resource_count;
	resource_availabilities = vector<int>(resource_count);

	for (int i = 0; i < resource_count; i++)
	{
		project_lib >> resource_availabilities[i];
	}

	parsed_tasks = vector<Task>(task_count);
	for (int i = 0; i < task_count; i++)
	{
		Task task;
		task.id = i;

		project_lib >> task.duration;

		horizon += task.duration;

		int max_resource_requirement = -1;
		for (int j = 0; j < resource_count; j++)
		{
			int parsed_resource_requirement;
			project_lib >> parsed_resource_requirement;

			if (parsed_resource_requirement > max_resource_requirement) {
				max_resource_requirement = parsed_resource_requirement;
				task.resource_requirement = parsed_resource_requirement;
				task.resource_type = j;
			}
		};

		int successor_count;
		project_lib >> successor_count;
		task.successors = vector<int>(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> task.successors[j];
		}

		parsed_tasks[i] = task;
	}

	// Close the file
	project_lib.close();
	write_custom_project_file(output_lib_folder + DC1_sub_folder + DC1_lib_file, horizon, parsed_tasks, resource_availabilities);
}

void parse_RG30_file(string file_path)
{
	string tmp;

	ifstream project_lib(file_path);

	if (!project_lib.is_open()) {
		throw runtime_error("Could not open lib file");
	};

	int task_count;
	int horizon;
	int resource_count;
	vector<Task> parsed_tasks;
	vector<int> resource_availabilities;

	project_lib >> task_count >> resource_count;
	resource_availabilities = vector<int>(resource_count);

	for (int i = 0; i < resource_count; i++)
	{
		project_lib >> resource_availabilities[i];
	}

	parsed_tasks = vector<Task>(task_count);
	for (int i = 0; i < task_count; i++)
	{
		Task task;
		task.id = i;

		project_lib >> task.duration;

		horizon += task.duration;

		int max_resource_requirement = -1;
		for (int j = 0; j < resource_count; j++)
		{
			int parsed_resource_requirement;
			project_lib >> parsed_resource_requirement;

			if (parsed_resource_requirement > max_resource_requirement) {
				max_resource_requirement = parsed_resource_requirement;
				task.resource_requirement = parsed_resource_requirement;
				task.resource_type = j;
			}
		};

		int successor_count;
		project_lib >> successor_count;
		task.successors = vector<int>(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> task.successors[j];
		}

		parsed_tasks[i] = task;
	}

	// Close the file
	project_lib.close();
	//write_custom_project_file(output_lib_folder + RG30_sub_folder + RG30_set1_folder + RG30_lib_file, horizon, parsed_tasks, resource_availabilities);
}
