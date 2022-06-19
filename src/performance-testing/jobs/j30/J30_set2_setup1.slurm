#!/bin/sh
#
#SBATCH --job-name="J30_set2_setup1"
#SBATCH --partition=compute
#SBATCH --time=12:00:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --output=output/J30_set2_setup1.out
#SBATCH --error=errors/J30_set2_setup1.err

for FI in parsed-datasets/j30/set2/*.RCP;
do
	srun timeout 60 ./executables/performance-testing/performance-testing "./$(dirname ${FI})/" "$(basename ${FI})" 1 1;
	srun timeout 60 ./executables/performance-testing/performance-testing "./$(dirname ${FI})/" "$(basename ${FI})" 1 0;
done;