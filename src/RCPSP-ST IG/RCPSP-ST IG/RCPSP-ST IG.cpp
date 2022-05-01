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
	vector<int> resource_requirements;
	// average resource utility rate
	double arur;

	bool operator > (const Job& o) const
	{
		return (arur > o.arur);
	}
};

const string project_lib_filename = "C:/Projects/cse3000/src/RCPSP-ST IG/RCPSP-ST IG/j30.sm/j301_1.sm";

void parseJobs(int& job_count, std::vector<Job>& jobs, int& resource_count, vector<int>& resource_availability, string filename);
void calculate_average_resource_utility_rate(std::vector<Job>& jobs, std::vector<int>& resource_availabilities);
bool check_presedence_violation(std::vector<Job>& jobs, std::vector<int>& job_in_violation_at_index);
template <typename t> void move(std::vector<t>& v, size_t oldIndex, size_t newIndex);

void fix_presedence_constraint(std::vector<Job>& jobs);

int main()
{
	int job_count;
	vector<Job> jobs;
	int resource_count;
	vector<int> resource_availabilities;

	parseJobs(job_count, jobs, resource_count, resource_availabilities, project_lib_filename);

	calculate_average_resource_utility_rate(jobs, resource_availabilities);

	sort(jobs.begin() + 1, jobs.end() - 1, greater<Job>());

	fix_presedence_constraint(jobs);

	return 0;
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
		std::rotate(v.rend() - oldIndex - 1, v.rend() - oldIndex, v.rend() - newIndex);
	else
		std::rotate(v.begin() + oldIndex, v.begin() + oldIndex + 1, v.begin() + newIndex + 1);
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
		int index = 0;
		double arur = 0.0;
		for each (int resource_requirement in jobs[i].resource_requirements)
		{
			arur += (double)resource_requirement / (double)resource_availabilities[index];
			index++;
		}
		arur *= jobs[i].duration;
		jobs[i].arur = arur;
	}
}

void parseJobs(int& job_count, std::vector<Job>& jobs, int& resource_count, vector<int>& resource_availabilities, string filename)
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
		vector<int> resource_requirements(resource_count);
		for (int j = 0; j < resource_count; j++)
		{
			project_lib >> resource_requirements[j];
		};
		jobs[i].resource_requirements = resource_requirements;
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
