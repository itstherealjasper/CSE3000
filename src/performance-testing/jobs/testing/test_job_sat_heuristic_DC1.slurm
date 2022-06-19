#!/bin/sh
#
#SBATCH --job-name="test_job_sat_heuristic_DC1"
#SBATCH --partition=compute
#SBATCH --time=00:30:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --output=output/test_job_sat_heuristic_DC1.out
#SBATCH --error=errors/test_job_sat_heuristic_DC1.err

for FI in limited-parsed-datasets/DC1/*.RCP;
do
	srun timeout 20 ./executables/performance-testing/performance-testing "./$(dirname ${FI})/" "$(basename ${FI})" 1 1;
	srun timeout 20 ./executables/performance-testing/performance-testing "./$(dirname ${FI})/" "$(basename ${FI})" 1 0;
done;