#include "problem-parser.h"

using namespace std;
namespace fs = std::filesystem;

struct Task
{
	int id;
	vector<int> successors;
	int duration;
	vector<int> resource_requirements;
};

void write_custom_project_file(string file_path, int horizon, vector<Task> parsed_tasks, vector<int> resource_availabilities, int optimal_heuristic_solution);
string extract_filename_without_extention(string& file_path);
void parse_j30_file(string file_path, ifstream& optimal_solution_fs);
void parse_DC1_file(string file_path, ifstream& optimal_solution_fs);
void parse_RG30_file(string file_path, string set_directory, ifstream& optimal_solution_fs);

const string input_lib_folder = "C:/Projects/cse3000/src/datasets/";
const string output_lib_folder = "C:/Projects/cse3000/src/parsed-datasets/";

const string j30_sub_folder = "j30.sm/";

const string DC1_sub_folder = "DC1/";

const string RG30_sub_folder = "RG30/";
const vector<string> RG30_set_directories = { "Set1/", "Set2/", "Set3/","Set4/", "Set5/" };

int main(int argc, char* argv[])
{
	ifstream j30_optimal_solution_fs(input_lib_folder + "J30_BKS.csv");
	if (!j30_optimal_solution_fs.is_open()) {
		throw runtime_error("Could not open lib file");
	};
	vector<string> j30_filenames_sorted;
	for (auto& entry : fs::directory_iterator(input_lib_folder + j30_sub_folder)) {
		j30_filenames_sorted.push_back(entry.path().string());
	}
	sort(j30_filenames_sorted.begin(), j30_filenames_sorted.end(), [](const string& lhs, const string& rhs) {return make_pair(lhs.length(), cref(lhs)) < make_pair(rhs.length(), cref(rhs)); });
	//for (int i = 0; i < j30_filenames_sorted.size(); i++)
	//{
	//	if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 4) == "1.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 4) + "0.sm").c_str());
	//	}
	//	else if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 4) == "2.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 4) + "1.sm").c_str());
	//	}
	//	else if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 4) == "3.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 4) + "2.sm").c_str());
	//	}
	//	else if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 4) == "4.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 4) + "3.sm").c_str());
	//	}
	//	else if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 4) == "5.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 4) + "4.sm").c_str());
	//	}
	//	else if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 4) == "6.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 4) + "5.sm").c_str());
	//	}
	//	else if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 4) == "7.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 4) + "6.sm").c_str());
	//	}
	//	else if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 4) == "8.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 4) + "7.sm").c_str());
	//	}
	//	else if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 4) == "9.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 4) + "8.sm").c_str());
	//	}
	//	else if (j30_filenames_sorted[i].substr(j30_filenames_sorted[i].size() - 5) == "10.sm") {
	//		rename(j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size()).c_str(), (j30_filenames_sorted[i].substr(0, j30_filenames_sorted[i].size() - 5) + "9.sm").c_str());
	//	}
	//	else {
	//		continue;
	//	}
	//}
	for (auto& filename : j30_filenames_sorted) {
		parse_j30_file(filename, j30_optimal_solution_fs);
	}
	j30_optimal_solution_fs.close();

	ifstream DC1_optimal_solution_fs(input_lib_folder + "DC1_BKS.csv");
	if (!DC1_optimal_solution_fs.is_open()) {
		throw runtime_error("Could not open lib file");
	};
	vector<string> DC1_filenames_sorted;
	for (auto& entry : fs::directory_iterator(input_lib_folder + DC1_sub_folder)) {
		DC1_filenames_sorted.push_back(entry.path().string());
	}
	sort(DC1_filenames_sorted.begin(), DC1_filenames_sorted.end(), [](const string& lhs, const string& rhs) {return make_pair(lhs.length(), cref(lhs)) < make_pair(rhs.length(), cref(rhs)); });
	for (auto& filename : DC1_filenames_sorted) {
		parse_DC1_file(filename, DC1_optimal_solution_fs);
	}
	DC1_optimal_solution_fs.close();

	ifstream RG30_optimal_solution_fs(input_lib_folder + "RG30_BKS.csv");
	if (!RG30_optimal_solution_fs.is_open()) {
		throw runtime_error("Could not open lib file");
	};
	for (int i = 0; i < RG30_set_directories.size(); i++)
	{
		vector<string> RG30_filenames_sorted;
		for (auto& entry : fs::directory_iterator(input_lib_folder + RG30_sub_folder + RG30_set_directories[i])) {
			RG30_filenames_sorted.push_back(entry.path().string());
		}
		sort(RG30_filenames_sorted.begin(), RG30_filenames_sorted.end(), [](const string& lhs, const string& rhs) {return make_pair(lhs.length(), cref(lhs)) < make_pair(rhs.length(), cref(rhs)); });
		for (auto& filename : RG30_filenames_sorted) {
			parse_RG30_file(filename, RG30_set_directories[i], RG30_optimal_solution_fs);
		}
	}
	RG30_optimal_solution_fs.close();

	return 0;
}

void write_custom_project_file(string file_path, int horizon, vector<Task> parsed_tasks, vector<int> resource_availabilities, int optimal_heuristic_solution)
{
	stringstream lib_file_content;

	lib_file_content << "h;" << horizon << '\n';
	lib_file_content << "s;" << parsed_tasks.size() << '\n';
	lib_file_content << "r;" << resource_availabilities.size() << '\n';
	lib_file_content << "o;" << optimal_heuristic_solution << '\n';
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
		lib_file_content << parsed_tasks[i].id << ';';
		for (int j = 0; j < parsed_tasks[i].resource_requirements.size(); j++)
		{
			lib_file_content << parsed_tasks[i].resource_requirements[j] << ';';
		}

		lib_file_content << parsed_tasks[i].duration << ';' << parsed_tasks[i].successors.size();

		for (int j = 0; j < parsed_tasks[i].successors.size(); j++)
		{
			lib_file_content << ';' << parsed_tasks[i].successors[j];
		}
		lib_file_content << '\n';
	}

	ofstream schedule_file(file_path + ".RCP");
	if (schedule_file.is_open())
	{
		schedule_file << lib_file_content.str();
		schedule_file.close();
	}
}

void parse_j30_file(string file_path, ifstream& optimal_solution_fs)
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
	int optimal_heuristic_solution;

	getline(optimal_solution_fs, tmp, ';');
	getline(optimal_solution_fs, tmp, ';');
	optimal_solution_fs >> optimal_heuristic_solution;
	getline(optimal_solution_fs, tmp);

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
		parsed_tasks[i].resource_requirements = vector<int>(resource_count);
		for (int j = 0; j < resource_count; j++)
		{
			project_lib >> parsed_tasks[i].resource_requirements[j];
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

	string file_without_extension = extract_filename_without_extention(file_path);
	write_custom_project_file(output_lib_folder + j30_sub_folder + file_without_extension, horizon, parsed_tasks, resource_availabilities, optimal_heuristic_solution);
}

string extract_filename_without_extention(string& file_path)
{
	// Base file without extention extraction found at https://stackoverflow.com/a/24386991
	string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
	string::size_type const p(base_filename.find_last_of('.'));
	return base_filename.substr(0, p);
}

void parse_DC1_file(string file_path, ifstream& optimal_solution_fs)
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
	int optimal_heuristic_solution;

	getline(optimal_solution_fs, tmp, ';');
	getline(optimal_solution_fs, tmp, ';');
	optimal_solution_fs >> optimal_heuristic_solution;
	getline(optimal_solution_fs, tmp);

	project_lib >> task_count >> resource_count;
	resource_availabilities = vector<int>(resource_count);

	for (int i = 0; i < resource_count; i++)
	{
		project_lib >> resource_availabilities[i];
	}

	parsed_tasks = vector<Task>(task_count);
	for (int i = 0; i < task_count; i++)
	{
		parsed_tasks[i].id = i;

		project_lib >> parsed_tasks[i].duration;

		horizon += parsed_tasks[i].duration;

		parsed_tasks[i].resource_requirements = vector<int>(resource_count);
		for (int j = 0; j < resource_count; j++)
		{
			project_lib >> parsed_tasks[i].resource_requirements[j];
		};

		int successor_count;
		project_lib >> successor_count;
		parsed_tasks[i].successors = vector<int>(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> parsed_tasks[i].successors[j];
		}
	}

	// Close the file
	project_lib.close();

	string file_without_extension = extract_filename_without_extention(file_path);
	write_custom_project_file(output_lib_folder + DC1_sub_folder + file_without_extension, horizon, parsed_tasks, resource_availabilities, optimal_heuristic_solution);
}

void parse_RG30_file(string file_path, string set_directory, ifstream& optimal_solution_fs)
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
	int optimal_heuristic_solution;

	getline(optimal_solution_fs, tmp, ';');
	getline(optimal_solution_fs, tmp, ';');
	optimal_solution_fs >> optimal_heuristic_solution;
	getline(optimal_solution_fs, tmp);

	project_lib >> task_count >> resource_count;
	resource_availabilities = vector<int>(resource_count);

	for (int i = 0; i < resource_count; i++)
	{
		project_lib >> resource_availabilities[i];
	}

	parsed_tasks = vector<Task>(task_count);
	for (int i = 0; i < task_count; i++)
	{
		parsed_tasks[i].id = i;

		project_lib >> parsed_tasks[i].duration;

		horizon += parsed_tasks[i].duration;

		parsed_tasks[i].resource_requirements = vector<int>(resource_count);
		for (int j = 0; j < resource_count; j++)
		{
			project_lib >> parsed_tasks[i].resource_requirements[j];
		};

		int successor_count;
		project_lib >> successor_count;
		parsed_tasks[i].successors = vector<int>(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> parsed_tasks[i].successors[j];
		}
	}

	// Close the file
	project_lib.close();

	string file_without_extension = extract_filename_without_extention(file_path);
	write_custom_project_file(output_lib_folder + RG30_sub_folder + set_directory + file_without_extension, horizon, parsed_tasks, resource_availabilities, optimal_heuristic_solution);
}
