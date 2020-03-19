#!/bin/bash
#SBATCH --nodes=26
#SBATCH --ntasks=130
#SBATCH --cpus-per-task=4
#SBATCH --output=mallob_id8000_26x5x4_3720s_log
#SBATCH --error=mallob_id8000_26x5x4_3720s_err
#SBATCH --job-name=mallob_id8000_26x5x4_3720s
#SBATCH --partition=normal
#SBATCH --time=1:02:00
mkdir -p mallob_logs/mallob_id8000_26x5x4_3720s
echo logging into mallob_logs/mallob_id8000_26x5x4_3720s
module load mpi/impi/2019
module load compiler/gnu/7
export MPIRUN_OPTIONS='-binding domain='${SLURM_CPUS_PER_TASK}':compact -print-rank-map -envall'
mpiexec.hydra --bootstrap slurm ${MPIRUN_OPTIONS} -n ${SLURM_NTASKS} build/mallob scenarios/scenario_all_c2 -c=2 -l=0.950 -t=4 -T=3700 -lbc=4 -time-per-instance=0 -cpuh-per-instance=10.000 -derandomize -ba=8 -g=1 -md=0 -p=0.100 -s=1 -v=4 -warmup -jjp -cg -r=bisec -bm=ed -yield -log=mallob_logs/mallob_id8000_26x5x4_3720s
echo finished
