#!/bin/bash
###   s b a t c h --array=1-$runs:1 $SL_FILE_NAME
#SBATCH --output=out/slurm_%j.txt
#SBATCH --error=err/slurm_err_%j.txt
#SBATCH --ntasks=1 # number of tasks
#SBATCH --ntasks-per-node=1 #number of tasks per node
#SBATCH --mem=32G
#SBATCH --cpus-per-task=32 # number of CPUs
#SBATCH --time=0-06:00:00 #Walltime
#SBATCH -p medium
#SBATCH --exclude=n[001-004,017-018]




# # #!/bin/bash
# # ###   s b a t c h --array=1-$runs:1 $SL_FILE_NAME
# # #SBATCH --output=out/slurm_%j.txt
# # #SBATCH --error=err/slurm_err_%j.txt
# # #SBATCH --ntasks=1 # number of tasks
# # #SBATCH --ntasks-per-node=1 #number of tasks per node
# # #SBATCH --mem=32G
# # #SBATCH --cpus-per-task=32 # number of CPUs
# # #SBATCH --time=0-6:00:00 #Walltime
# # #SBATCH -p medium
# # #SBATCH --exclude=n[001-004,017-018]










SRCDIR=`pwd`
echo -n "INSTANCES: "
echo $INSTANCES

INSTANCES=($(ls -1 $INSTANCES))
INSTANCE=${INSTANCES[$SLURM_ARRAY_TASK_ID]}

echo -n "INSTANCE: " 
echo $INSTANCE

echo -n "SLURM_ARRAY_TASK_ID: "
echo $SLURM_ARRAY_TASK_ID

echo -n "instance_path: "
echo $instance_path

instance_path=`dirname $INSTANCE`



mkdir -p "$SCRATCH_JOB/$instance_path" && cp $INSTANCE "$SCRATCH_JOB/$instance_path" -v
cp neat -v $SCRATCH_JOB

cd $SCRATCH_JOB

DESTINATION_FOLDER="$SRCDIR/$DESTINATION_FOLDER"



cat > tmp.ini <<EOF
; temporal config file for train in hpc hipatia


[Global]
MODE = train
PROBLEM_NAME = permu


MAX_TRAIN_TIME = 18000
POPSIZE = $POPSIZE
THREADS = 32

SEARCH_TYPE = phased
SEED = $SEED

CONTROLLER_NAME_PREFIX = $CONTROLLER_NAME_PREFIX
EXPERIMENT_FOLDER_NAME = $EXPERIMENT_FOLDER_NAME

MAX_TIME_PSO = $MAX_TIME_PSO


PROBLEM_TYPE = $PROBLEM_TYPE
PROBLEM_PATH = $INSTANCE

EOF




date
./neat "tmp.ini"
date

