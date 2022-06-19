#!/bin/sh
#
#SBATCH --job-name="RG30_set4_setup2"
#SBATCH --partition=compute
#SBATCH --time=12:00:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --output=output/RG30_set4_setup2.out
#SBATCH --error=errors/RG30_set4_setup2.err

for FI in parsed-datasets/RG30/set4/*.RCP;
do
	srun timeout 60 ./executables/performance-testing/performance-testing "./$(dirname ${FI})/" "$(basename ${FI})" 2 1;
	srun timeout 60 ./executables/performance-testing/performance-testing "./$(dirname ${FI})/" "$(basename ${FI})" 2 0;
done;