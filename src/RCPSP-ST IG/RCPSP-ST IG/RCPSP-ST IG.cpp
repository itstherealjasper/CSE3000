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
};

void parseJobs(int& job_count, std::vector<Job>& jobs, int& resource_count, vector<int>& resource_availability, string filename);

const string project_lib_filename = "C:/Projects/cse3000/src/RCPSP-ST IG/RCPSP-ST IG/j30.sm/j301_1.sm";

int main()
{
	int job_count;
	vector<Job> jobs;
	int resource_count;
	vector<int> resource_availabilities;

	parseJobs(job_count, jobs, resource_count, resource_availabilities, project_lib_filename);

	return 0;
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
	for (size_t i = 0; i < 5; i++)
	{
		getline(project_lib, line);
	};

	getline(project_lib, line, ':');
	project_lib >> job_count;

	for (size_t i = 0; i < 2; i++)
	{
		getline(project_lib, line);
	};

	getline(project_lib, line, ':');
	project_lib >> resource_count;
	getline(project_lib, line);

	for (size_t i = 0; i < 9; i++)
	{
		getline(project_lib, line);
	};

	for (size_t i = 0; i < job_count; i++)
	{
		Job job;
		jobs.push_back(job);
		project_lib >> jobs[i].jobnr >> jobs[i].modes_count >> jobs[i].successor_count;
		//cout << i << ' ' << jobs[i].jobnr <<'\n';
		vector<int> successors(jobs[i].successor_count);
		for (size_t j = 0; j < jobs[i].successor_count; j++)
		{
			project_lib >> successors[j];
		};
		jobs[i].successors = successors;
	}

	for (size_t i = 0; i < 5; i++)
	{
		getline(project_lib, line);
	};

	for (size_t i = 0; i < job_count; i++)
	{
		project_lib >> jobs[i].jobnr >> jobs[i].mode >> jobs[i].duration;
		//cout << i << ' ' << jobs[i].jobnr << ' ' << jobs[i].duration << '\n';
		vector<int> resource_requirements(resource_count);
		for (size_t j = 0; j < resource_count; j++)
		{
			project_lib >> resource_requirements[j];
		};
		jobs[i].resource_requirements = resource_requirements;
	}

	for (size_t i = 0; i < 4; i++)
	{
		getline(project_lib, line);
	};

	for (size_t i = 0; i < resource_count; i++)
	{
		int resource_availability;
		project_lib >> resource_availability;
		resource_availabilities.push_back(resource_availability);
	}

	// Close the file
	project_lib.close();


	/*for (size_t i = 0; i < 32; i++)
	{
		cout << "jobnr" << '\t' << "#successors" << '\t' << "duration" << '\n';
		cout << jobs[i].jobnr << '\t' << jobs[i].successor_count << "\t\t" << jobs[i].duration << '\n';
		cout << "successors" << '\n';
		for (size_t j = 0; j < jobs[i].successor_count; j++)
		{
			cout << jobs[i].successors[j] << ' ';
		}
		cout << '\n';
		cout << "resource requirements" << '\n';
		for (size_t j = 0; j < resource_count; j++)
		{
			cout << jobs[i].resource_requirements[j] << ' ';
		}
		cout << '\n';
	}
	
	for (size_t i = 0; i < resource_count; i++)
	{
		cout << resource_availabilities[i] << ' ';
	}*/

}
