#!/bin/sh
#
#SBATCH --job-name="DC1_set3_setup5"
#SBATCH --partition=compute
#SBATCH --time=12:00:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --output=output/DC1_set3_setup5.out
#SBATCH --error=errors/DC1_set3_setup5.err

for FI in parsed-datasets/DC1/set3/*.RCP;
do
	srun timeout 60 ./executables/performance-testing/performance-testing "./$(dirname ${FI})/" "$(basename ${FI})" 5 1;
	srun timeout 60 ./executables/performance-testing/performance-testing "./$(dirname ${FI})/" "$(basename ${FI})" 5 0;
done;