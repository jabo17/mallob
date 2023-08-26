#!/bin/bash

# This script should be called as follows (after setting all options below as needed):
# nohup bash run_sat_benchmark.sh --run path/to/benchmark-file 2>&1 > OUT &
# After executing this command, you can press Ctrl+C and later log out of the server 
# PROPERLY, i.e., with "exit" and not due to a connection timeout.
# 
# The progress can be monitored in real time with:
# tail -f OUT
# Also check with `htop` that the machine's cores are actually busy.
# 
# To stop/cancel an experiment running in the background, run:
# bash run_sat_benchmark.sh --stop
# 
# You can extract basic coverage / run time information from finished experiments like so:
# bash sat_benchmark.sh --extract path/to/my/experiment
# The provided path must contain a folder named i for each instance index i.
# In that path, some text files with raw information will be created.
# - qualified-runtimes-and-results.txt: 
#   Contains one line for each instance with its ID, the run time (= time limit if 
#   unsolved) and the found result ("sat" or "unsat" or "unknown").
# - qualified-runtimes-{sat,unsat}.txt:
#   Contains one line for each instance found {SAT, UNSAT} with its ID and the run time.
# - cdf-runtimes.txt, cdf-runtimes-{sat,unsat}.txt:
#   Using one of these files as a sequence of x- and y-coordinates, you will get a 
#   performance plot as commonly used by the SAT community. The x-coordinate is the time 
#   limit per instance and the y-coordinate is the number of instances solved in that 
#   limit.

#####################################################################
# TODO Configuration of your experiments

# 8 for normal utilization, keeping hardware threads idle
# 4 for full utilization, spawning a solver at each hardware thread
nhwthreadsperproc=32

# Some environment variables for Mallob
RDMAV_FORK_SAFE=1
NPROCS="$(($(nproc)/2/$nhwthreadsperproc))" 
PATH="build:$PATH"

# TODO Set the portfolio of solvers to cycle through
# (k=Kissat, c=CaDiCaL, l=Lingeling, g=Glucose)
portfolio="kcl"
#portfolio="k"
#portfolio="c"

# Clause buffering decay factor. Usually 1.0 for modestly parallel setups
# and 0.9 for massively parallel setups.
cbdf=1.0

# Timeout per instance in seconds
timeout=300

# Run all instances from this index up to the end
# (Default: 1; set to another number i if continuing an interrupted 
# experiment where i-1 instances were run successfully)
startinstance=1

# TODO Base log directory; use a descriptive name for each experiment. No spaces.
baselogdir="/workspace/borowitz/cls-diversitiy-exp2-138"

# TODO Add any further options to the name of this log directory as well.
# Results from older experiments with the same sublogdir will be overwritten!
sublogdir="${baselogdir}/${portfolio}-cbdf${cbdf}-T${timeout}"

# TODO Add further options to these arguments Mallob is called with.
#malloboptions="-t=4 -T=$timeout -v=3 -sleep=1000 -appmode=fork -v=3 -interface-fs=0 -trace-dir=. -pipe-large-solutions=0 -processes-per-host=$NPROCS -regular-process-allocation -max-lits-per-thread=50000000 -strict-clause-length-limit=20 -clause-filter-clear-interval=500 -max-lbd-partition-size=2 -export-chunks=20 -clause-buffer-discount=$cbdf -satsolver=$portfolio"
malloboptions=" \
`#deployment` -rpa=1 -pph=$NPROCS -mlpt=50000000 -t=$nhwthreadsperproc -T=$timeout \ 
`#portfolio,diversification` -satsolver=$portfolio -isp=0.5 -div-phases=1 -div-noise=1 -div-elim=0 -scsd=1 \ 
`#sharingsetup` -scll=255 -slbdl=255 -csm=2 -mlbdps=5 -cfm=3 -cfci=15 -mscf=5 -bem=1 -aim=1 -rlbd=0 -ilbd=1 \ 
`#sharingvolume` -s=0.5 -cbbs=$((188 * $nhwthreadsperproc)) -cblm=1 -cblp=100000 -cusv=1 \ 
`#randomseed` -seed=0"

#####################################################################


if [ -z $1 ]; then
    echo "Usage:"
    echo "Run a benchmark: bash $0 --run path/to/benchmark-file"
    echo "Extract benchmark results: bash $0 --extract path/to/experiments"
    echo "Stop a running benchmark: bash $0 --stop"
    exit 1
fi

# Cleanup / killing function
function cleanup() {
    killall -9 mpirun 2>/dev/null
    killall -9 build/mallob 2>/dev/null
    killall -9 ./build/mallob_sat_process 2>/dev/null
    rm /dev/shm/*mallob* 2>/dev/null
}

# Clean up other running experiments
if [ "$1" == "--stop" ]; then
    touch STOP_IMMEDIATELY
    cleanup
    sleep 3
    rm STOP_IMMEDIATELY
    echo "Stopped experiments."
    exit 0
fi

# Extract run time results
if [ "$1" == "--extract" ]; then

    # Set $1 to results directory
    shift 1
    if [ -z $1 ]; then
        echo "Provide a results directory."
        exit 1
    fi

    i=1
    > $1/qualified-runtimes.txt
    > $1/qualified-runtimes-sat.txt
    > $1/qualified-runtimes-unsat.txt
    nsat=0
    nunsat=0
    par2sum=0
    while [ -d "$1/$i" ]; do
    
        if [ -f STOP_IMMEDIATELY ]; then
            # Signal to stop
            echo "Stopping because STOP_IMMEDIATELY is present"
            exit
        fi
        
        # Log files to parse
        dir="$1/$i"
        logfiles=$(echo $dir/*/log.*)
        echo "$logfiles"
        
        # Extract run time and result
        time=$(cat $logfiles|grep RESPONSE_TIME|awk '{print $6}')
        if [ "x$time" == "x" ]; then
            time="$timeout"
            result="unknown"
            par2sum=$(echo "$par2sum + 2*$time"|bc -l)
        else
            par2sum=$(echo "$par2sum + $time"|bc -l)
            if grep -q "s SATISFIABLE" $logfiles; then
                result="sat"
                nsat=$((nsat+1))
            else
                result="unsat"
                nunsat=$((nunsat+1))
            fi
        fi
        
        # Write run time and result to files
        echo "$i $time $result" >> $1/qualified-runtimes.txt
        echo "$i $time" >> $1/qualified-runtimes-$result.txt
        
        i=$((i+1))
    done
    
    # Postprocess and reformat files
    for res in "" -sat -unsat ; do
        cat $1/qualified-runtimes${res}.txt|awk '$2 < '$timeout' {print $2}'|sort -g > $1/sorted-runtimes${res}.txt
        cat $1/sorted-runtimes${res}.txt|awk '{print $1,NR}' > $1/cdf-runtimes${res}.txt
    done
    mv $1/qualified-runtimes.txt $1/qualified-runtimes-and-results.txt
    
    echo "Experiments on $((i-1)) instances found."
    echo "$((nsat+nunsat)) solved ($nsat sat, $nunsat unsat), PAR-2 score: $(echo "$par2sum / (${i}-1)"|bc -l)"
    exit 0
fi

# Extract Hash Values Of Produced Clauses
if [ "$1" == "--extract-produced-cls" ]; then
	shift 1
	if [ -z $1 ]; then
		echo "Provide a results directory."
		exit 1
	fi

	rm "eval_jobs.txt"

	min_i=1
	i=$min_i
	while [ -d "$1/$i" ]; do
		dir="$1/$i"
                tmp_file2="$dir/cls_produced2.tmp"
                tmp_file="$dir/cls_produced.tmp"
		rm $tmp_file2
		rm $tmp_file
		
		job=""
		j=0
		while [ -d "$dir/$j" ]; do
			for s in $dir/$j/produced_cls.*.log; do
				tmp=${s#*produced_cls.}
				solver=${tmp%.log}
				if [ ! -z "$job" ]; then
					job="${job}; "
				fi
				job="${job}cat $s|awk '{print \$0,\"$j ${solver}\"}' >> $tmp_file" # rm $dir/$j/produced_cls.$s.log"		
			done
			j=$(($j+1))
		done
		if [ ! -z "job" ]; then
			job="${job}; "
		fi
		job="${job}sort -k2n -k1n -o $tmp_file2 $tmp_file; rm $tmp_file"
		job="${job}; bash scripts/run/duplicates.sh --extract $dir && bash scripts/run/duplicates.sh --time-extract $dir  && rm $tmp_file2"

		echo "$job; echo \"Sorted Produced Clauses Of $dir\"" >> "eval_jobs.txt"

		i=$(($i+1))
	done
	parallel -j 10 < "eval_jobs.txt"
	exit 0
fi

# Extract Pairwise Solver Hash Overlap
if [ "$1" == "--eval-pw-produced-cls" ]; then
	shift 1
	if [ -z $1 ]; then
		echo "Provide a results directory."
		exit 1
	fi

	rm "eval_jobs.txt"

	min_i=1
	i=$min_i
	while [ -d "$1/$i" ]; do
		dir="$1/$i"
                job="bash scripts/run/duplicates.sh --pw-extract $dir"

		echo "$job; echo \"Sorted Produced Clauses Of $dir\"" >> "eval_jobs.txt"

		i=$(($i+1))
	done
	parallel -j 10 < "eval_jobs.txt"

	i=min_i
	stats="$1/pw_stat.out"
	rm $stats
	while [ -d "$1/$i" ]; do
		
		if [ -f STOP_IMMEDIATELY ]; then
			# Signal to stop
			echo "Stopping because STOP_MMEDIATELY is present"
			exit
		fi

                stat="$1/$i/pw_stat.out"

		cat $stat >> $stats

		i=$((i+1))
	done
	exit 0
fi

# Extract Pairwise Solver Hash Overlap
if [ "$1" == "--extract-time-produced-cls" ]; then
	shift 1
	if [ -z $1 ]; then
		echo "Provide a results directory."
		exit 1
	fi

	rm "eval_jobs.txt"

	min_i=1
	i=$min_i
	while [ -d "$1/$i" ]; do
		dir="$1/$i"
                job="bash scripts/run/duplicates.sh --time-extract $dir"

		echo "$job; echo \"Sorted Produced Clauses Of $dir\"" >> "eval_jobs.txt"

		i=$(($i+1))
	done
	parallel -j 10 < "eval_jobs.txt"

	i=min_i
	stats="$1/pw_stat.out"
	rm $stats
	while [ -d "$1/$i" ]; do
		
		if [ -f STOP_IMMEDIATELY ]; then
			# Signal to stop
			echo "Stopping because STOP_MMEDIATELY is present"
			exit
		fi

                stat="$1/$i/pw_stat.out"

		cat $stat >> $stats

		i=$((i+1))
	done
	exit 0
fi


# Do statistics with the produced clauses
if [ "$1" == "--eval-produced-cls" ]; then
	shift 1;
	i=1

	stats_dup="$1/stat.out"
	rm $stats_dup
	while [ -d "$1/$i" ]; do
		
		if [ -f STOP_IMMEDIATELY ]; then
			# Signal to stop
			echo "Stopping because STOP_MMEDIATELY is present"
			exit
		fi

                stat_dup="$1/$i/stat.out"

		cat $stat_dup >> $stats_dup

		i=$((i+1))
	done
	
	exit 0
fi

# Set $1 to benchmarks file
shift 1
if [ -z $1 ]; then
    echo "Provide a benchmark file."
    exit 1
fi

# Run experiments
i=1
for f in $(cat $1) ; do

        if [ -f STOP_IMMEDIATELY ]; then
            # Signal to stop
            echo "Stopping because STOP_IMMEDIATELY is present"
            exit
        fi

        # Skip any instances that should be skipped
        if [ $i -lt $startinstance ]; then 
                i=$((i+1))
                continue 
        fi

        echo "************************************************"
        echo "$i : $f"
        
        logdir="${sublogdir}/$i"
        rm -rf $logdir 2>/dev/null
        mkdir -p $logdir
        
        # Download file if necessary
        downloaded=false
        if [ ! -f "$f" ]; then
            echo "Cannot find file \"$f\" - trying to download from GBD"
            downloaded=true
            hash=$(echo "$f"|grep -oE "[0-9a-f]{32}"|head -1)
            while ! wget --content-disposition https://gbd.iti.kit.edu/file/$hash ; do sleep 1; done
            f=$(echo ${hash}-*.cnf.xz|head -1|awk '{print $1}')
        fi
        
        # Run Mallob
        mpirun -np $NPROCS --bind-to core --map-by ppr:${NPROCS}:node:pe=$nhwthreadsperproc build/mallob -mono=$f -log=$logdir $malloboptions 2>&1 > $logdir/OUT
        
        # Clean up
        cleanup
        if $downloaded; then
            rm -rf "$f"
        fi
        sleep 1

        i=$((i+1))
        echo ""
        echo ""
done
 
