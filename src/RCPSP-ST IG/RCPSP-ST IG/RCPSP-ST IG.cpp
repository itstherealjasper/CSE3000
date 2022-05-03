#include "RCPSP-ST IG.h"

using namespace std;

struct Job
{
	int jobnr;
	int modes_count;
	int successor_count;
	vector<int> successors;
	int mode;
	int duration;
	int resourcenr = 1; // default value required
	int resource_requirement = 0; // default value required
	// average resource utility rate
	double arur;

	bool operator > (const Job& o) const
	{
		return (arur > o.arur);
	}
};

const string project_lib_filename = "C:/Projects/cse3000/src/RCPSP-ST IG/RCPSP-ST IG/j30.sm/j301_1.sm";
const int destruction_count = 6;

void parse_jobs(int& job_count, int& horizon, std::vector<Job>& jobs, int& resource_count, vector<int>& resource_availability, string filename);
void extend_successors(Job& job, std::vector<Job>& jobs);
void calculate_average_resource_utility_rate(std::vector<Job>& jobs, std::vector<int>& resource_availabilities);
void fix_presedence_constraint(std::vector<Job>& jobs);
bool check_presedence_violation(std::vector<Job>& jobs);
bool check_presedence_violation(std::vector<Job>& jobs, std::vector<int>& job_in_violation_at_index);
template <typename t> void move(std::vector<t>& v, size_t oldIndex, size_t newIndex);
int construct_schedule(int horizon, int job_count, std::vector<Job>& jobs, std::vector<int> resource_availabilities, std::vector<int>& schedule);
void build_remaning_resources(std::vector<int>& resource_availabilities, int horizon, std::vector<vector<int>>& remaining_resources);
int find_earliest_start_time(int duration, int resourcenr, int resource_requirement, std::vector<std::vector<int>>& remaining_resources, int earliest_start_time);
void write_resource_schedule(int makespan, int resource_count, std::vector<int>& schedule, int job_count, std::vector<int>& resource_availabilities, std::vector<Job>& jobs, std::string filename);

void greedy_destruction_rebuild(int job_count, std::vector<Job>& jobs, int makespan, std::vector<int>& resource_availabilities, int horizon);

int main()
{
	int horizon;
	int job_count;
	vector<Job> jobs;
	int resource_count;
	vector<int> resource_availabilities;

#pragma region setup
	parse_jobs(job_count, horizon, jobs, resource_count, resource_availabilities, project_lib_filename);

	extend_successors(jobs[0], jobs);

	calculate_average_resource_utility_rate(jobs, resource_availabilities);

	sort(jobs.begin() + 1, jobs.end() - 1, greater<Job>());

	fix_presedence_constraint(jobs);
#pragma endregion


#pragma region initial schedule
	int makespan;
	vector<int> schedule;

	vector<Job> scheduled_jobs;
	vector<Job> to_schedule_jobs;
	scheduled_jobs.push_back(jobs[0]);
	scheduled_jobs.push_back(jobs[1]);
	for (int i = 2; i < job_count; i++)
	{
		to_schedule_jobs.push_back(jobs[i]);
	}
	while (to_schedule_jobs.size() > 0) {
		int minimum_makespan = horizon;
		int optimal_index = -1;
		Job to_schedule_job = to_schedule_jobs[0];
		to_schedule_jobs.erase(to_schedule_jobs.begin());
		for (int i = 0; i <= scheduled_jobs.size(); i++)
		{
			scheduled_jobs.insert(scheduled_jobs.begin() + i, to_schedule_job);
			if (!check_presedence_violation(scheduled_jobs)) {
				int makespan_test = construct_schedule(horizon, job_count, scheduled_jobs, resource_availabilities, vector<int>(job_count, 0));
				if (makespan_test < minimum_makespan) {
					minimum_makespan = makespan_test;
					optimal_index = i;
				}
			}
			scheduled_jobs.erase(scheduled_jobs.begin() + i);
		}
		assert(optimal_index >= 0);
		scheduled_jobs.insert(scheduled_jobs.begin() + optimal_index, to_schedule_job);
	}

	makespan = construct_schedule(horizon, job_count, scheduled_jobs, resource_availabilities, schedule);

	write_resource_schedule(makespan, resource_count, schedule, job_count, resource_availabilities, jobs, "initial_schedule");
#pragma endregion

	return 0;
}

void write_resource_schedule(int makespan, int resource_count, std::vector<int>& schedule, int job_count, std::vector<int>& resource_availabilities, std::vector<Job>& jobs, std::string filename)
{
	vector<vector<vector<int>>> resource_schedule;
	for (int i = 0; i < resource_count; i++)
	{
		resource_schedule.push_back(vector<vector<int>>(schedule[job_count - 1], vector<int>(resource_availabilities[i], 0)));
	}

	for (int i = 1; i < jobs.size() - 1; i++)
	{
		int start_time = schedule[jobs[i].jobnr - 1];
		int resource_index = 0;
		bool found_space = false;
		while (!found_space)
		{
			bool space_error = false;
			for (int j = start_time; j < start_time + jobs[i].duration; j++)
			{
				for (int k = 0; k < jobs[i].resource_requirement; k++)
				{
					if (resource_schedule[jobs[i].resourcenr - 1][j][k + resource_index] != 0) {
						space_error = true;
					}
				}
			}
			if (space_error)
			{
				resource_index++;
			}
			else
			{
				found_space = true;
			}
		}
		for (int j = start_time; j < start_time + jobs[i].duration; j++)
		{
			for (int k = 0; k < jobs[i].resource_requirement; k++)
			{
				resource_schedule[jobs[i].resourcenr - 1][j][k + resource_index] = jobs[i].jobnr;
			}
		}
	}

	ofstream schedule_file(filename + ".txt");
	if (schedule_file.is_open())
	{
		schedule_file << "Makespan: " << makespan << "\n\n";
		for (int i = 0; i < resource_count; i++)
		{
			schedule_file << "R " << i + 1 << '\n';
			for (int j = 0; j < resource_schedule[i][0].size(); j++)
			{
				for (int k = 0; k < resource_schedule[i].size(); k++)
				{
					if (resource_schedule[i][k][j] == 0)
					{
						schedule_file << ' ' << '\t';
					}
					else {
						schedule_file << resource_schedule[i][k][j] << '\t';
					}
				}
				schedule_file << '\n';
			}
			schedule_file << '\n' << '\n';
		}
		schedule_file.close();
	}
}


int construct_schedule(int horizon, int job_count, std::vector<Job>& jobs, std::vector<int> resource_availabilities, std::vector<int>& schedule)
{
	vector<vector<int>> remaining_resources;
	build_remaning_resources(resource_availabilities, horizon, remaining_resources);

	vector<int> new_schedule(job_count, 0);

	for (int i = 0; i < jobs.size(); i++)
	{
		int earliest_start_time = find_earliest_start_time(jobs[i].duration, jobs[i].resourcenr, jobs[i].resource_requirement, remaining_resources, new_schedule[jobs[i].jobnr - 1]);
		new_schedule[jobs[i].jobnr - 1] = earliest_start_time;
		for (int j = 0; j < jobs[i].duration; j++)
		{
			remaining_resources[earliest_start_time + j][jobs[i].resourcenr - 1] = remaining_resources[earliest_start_time + j][jobs[i].resourcenr - 1] - jobs[i].resource_requirement;
		}

		for (int j = 0; j < jobs[i].successor_count; j++)
		{
			if (earliest_start_time + jobs[i].duration > new_schedule[jobs[i].successors[j] - 1])
			{
				new_schedule[jobs[i].successors[j] - 1] = earliest_start_time + jobs[i].duration;
			}
		}
	}

	schedule = new_schedule;
	return schedule[schedule.size() - 1];
}

int find_earliest_start_time(int duration, int resourcenr, int resource_requirement, std::vector<std::vector<int>>& remaining_resources, int earliest_start_time)
{
	bool found_possible_timeslot = false;
	while (!found_possible_timeslot) {
		bool requirements_to_high = false;
		for (int i = 0; i < duration; i++)
		{
			if (resource_requirement > remaining_resources[earliest_start_time + i][resourcenr - 1])
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

void build_remaning_resources(std::vector<int>& resource_availabilities, int horizon, std::vector<std::vector<int>>& remaining_resources)
{
	for (int i = 0; i < horizon; i++)
	{
		remaining_resources.push_back(resource_availabilities);
	}
}

void fix_presedence_constraint(std::vector<Job>& jobs)
{
	vector<int> job_in_violation_at_index;

	bool presedence_violated = check_presedence_violation(jobs, job_in_violation_at_index);
	while (presedence_violated) {
		move(jobs, job_in_violation_at_index[0], job_in_violation_at_index[1]);
		presedence_violated = check_presedence_violation(jobs, job_in_violation_at_index);
	}
}

// Found on StackOverflow answer: https://stackoverflow.com/a/57399634
template <typename t> void move(std::vector<t>& v, size_t oldIndex, size_t newIndex)
{
	if (oldIndex > newIndex)
	{
		std::rotate(v.rend() - oldIndex - 1, v.rend() - oldIndex, v.rend() - newIndex);
	}
	else {
		std::rotate(v.begin() + oldIndex, v.begin() + oldIndex + 1, v.begin() + newIndex + 1);
	}
}

bool check_presedence_violation(std::vector<Job>& jobs)
{
	for (int i = 1; i < jobs.size(); i++)
	{
		for (int j = 0; j < i; j++)
		{
			int sucessor_index = find(jobs[i].successors.begin(), jobs[i].successors.end(), jobs[j].jobnr) - jobs[i].successors.begin();
			if (sucessor_index != jobs[i].successors.end() - jobs[i].successors.begin())
			{
				return true;
			}
		}
	}
	return false;
}

bool check_presedence_violation(std::vector<Job>& jobs, std::vector<int>& job_in_violation_at_index)
{
	for (int i = 1; i < jobs.size(); i++)
	{
		for (int j = 0; j < i; j++)
		{
			int sucessor_index = find(jobs[i].successors.begin(), jobs[i].successors.end(), jobs[j].jobnr) - jobs[i].successors.begin();
			if (sucessor_index != jobs[i].successors.end() - jobs[i].successors.begin())
			{
				job_in_violation_at_index = { i, j };
				return true;
			}
		}
	}

	return false;
}

void calculate_average_resource_utility_rate(std::vector<Job>& jobs, std::vector<int>& resource_availabilities)
{
	for (int i = 0; i < jobs.size(); i++)
	{
		jobs[i].arur = jobs[i].duration * (double)jobs[i].resource_requirement / (double)resource_availabilities[jobs[i].resourcenr - 1];
	}
}

void extend_successors(Job& job, std::vector<Job>& jobs) {
	if (job.jobnr == jobs[jobs.size() - 1].jobnr) {
		return;
	}
	for (int i = 0; i < job.successor_count; i++)
	{
		extend_successors(jobs[job.successors[i] - 1], jobs);
		for (int j = 0; j < jobs[job.successors[i] - 1].successor_count; j++)
		{
			if (find(job.successors.begin(), job.successors.end(), jobs[job.successors[i] - 1].successors[j]) == job.successors.end()) {
				job.successor_count++;
				job.successors.push_back(jobs[job.successors[i] - 1].successors[j]);
			}
		}
		sort(job.successors.begin(), job.successors.end());
	}
}

void parse_jobs(int& job_count, int& horizon, std::vector<Job>& jobs, int& resource_count, vector<int>& resource_availabilities, string filename)
{
	// Create a text string, which is used to output the text file
	string line;

	// Read from the text file
	ifstream project_lib(filename);

	if (!project_lib.is_open()) {
		throw runtime_error("Could not open lib file");
	};

	// Use a while loop together with the getline() function to read the file line by line
	for (int i = 0; i < 5; i++)
	{
		getline(project_lib, line);
	};

	getline(project_lib, line, ':');
	project_lib >> job_count;

	getline(project_lib, line, ':');
	project_lib >> horizon;

	for (int i = 0; i < 2; i++)
	{
		getline(project_lib, line);
	};

	getline(project_lib, line, ':');
	project_lib >> resource_count;
	getline(project_lib, line);

	for (int i = 0; i < 9; i++)
	{
		getline(project_lib, line);
	};

	for (int i = 0; i < job_count; i++)
	{
		Job job;
		jobs.push_back(job);
		project_lib >> jobs[i].jobnr >> jobs[i].modes_count >> jobs[i].successor_count;
		//cout << i << ' ' << jobs[i].jobnr <<'\n';
		vector<int> successors(jobs[i].successor_count);
		for (int j = 0; j < jobs[i].successor_count; j++)
		{
			project_lib >> successors[j];
		};
		jobs[i].successors = successors;
	}

	for (int i = 0; i < 5; i++)
	{
		getline(project_lib, line);
	};

	for (int i = 0; i < job_count; i++)
	{
		project_lib >> jobs[i].jobnr >> jobs[i].mode >> jobs[i].duration;
		//cout << i << ' ' << jobs[i].jobnr << ' ' << jobs[i].duration << '\n';
		int resource_requirement;
		for (int j = 0; j < resource_count; j++)
		{
			int parsed_resource_requirement;
			project_lib >> parsed_resource_requirement;

			if (parsed_resource_requirement > 0) {
				jobs[i].resource_requirement = parsed_resource_requirement;
				jobs[i].resourcenr = j + 1;
			}
		};
	}

	for (int i = 0; i < 4; i++)
	{
		getline(project_lib, line);
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
