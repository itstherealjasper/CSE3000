#include "rcpsp.h"

using namespace std;

struct Task
{
	int activity_id;
	vector<int> successors;
	int duration;
	int resource_type = 0; // default value required
	int resource_requirement = 0; // default value required
	double rur; // resource utility rate

	bool operator > (const Task& o) const
	{
		return (rur > o.rur);
	}
};

void parse_activities(string filename);
void generate_all_activity_segments();
void generate_activity_segments(Task activity, vector<int> successors, vector<Task> activity_segments);

// Parameters to tune the algorithm
const int destruction_count = 10;
const int cpu_time_deadline = 1; // seconds

// Global variables for the specific problem instance used
const string project_lib_filename = "C:/Projects/cse3000/src/RCPSP-ST IG/RCPSP-ST IG/j30.sm/j301_1.sm";
//const string project_lib_filename = "C:/Projects/cse3000/src/RCPSP-ST IG/RCPSP-ST IG/j120.sm/j1201_1.sm";
int horizon;
int activity_count;
vector<Task> activities;
int segment_count;
vector<vector<Task>> segments;
int resource_count;
vector<int> resource_availabilities;

int main()
{
	parse_activities(project_lib_filename);

	generate_all_activity_segments();

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
	project_lib >> activity_count;

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

	for (int i = 0; i < activity_count; i++)
	{
		Task activity;
		activity.activity_id = i;
		activity.segment_id = i;
		activities.push_back(activity);

		int successor_count;
		project_lib >> tmp >> tmp >> successor_count;
		vector<int> successors(successor_count);
		for (int j = 0; j < successor_count; j++)
		{
			project_lib >> successors[j];
			successors[j]--;
		};
		activities[i].successors = successors;
	}


	for (int i = 0; i < 5; i++)
	{
		getline(project_lib, tmp);
	};

	for (int i = 0; i < activity_count; i++)
	{
		project_lib >> tmp >> tmp >> activities[i].duration;
		int resource_requirement;
		for (int j = 0; j < resource_count; j++)
		{
			int parsed_resource_requirement;
			project_lib >> parsed_resource_requirement;

			if (parsed_resource_requirement > 0) {
				activities[i].resource_requirement = parsed_resource_requirement;
				activities[i].resource_type = j;
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

void generate_all_activity_segments()
{
	for each (Task activity in activities)
	{
		vector<Task> activity_segments;
		generate_activity_segments(activity, activity.successors, activity.duration, activity_segments);
	}
}

void generate_activity_segments(Task activity, vector<int> successors, int remaining_duration, vector<Task> activity_segments)
{
	if (remaining_duration == 0)
	{
		return;
	}
	for (int i = activity.duration; i > 0; i--)
	{
		Task new_segment = activity;
		new_segment.duration = i;
		new_segment.segment = activity.duration - i + 1;
		new_segment.successors = successors;
		activity_segments.push_back(new_segment);
		generate_activity_segments(new_segment, vector<int>(){})
	}
}