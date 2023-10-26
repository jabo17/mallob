#!/bin/bash


if [ "$1" == "--extract-exp" ]; then
	#
	# For each instance it aggregates the solver-specific logs with produced clauses
	# and sorts them by hash (first, numeric, <), time (second, numeric, <)  
	#
	shift 1

	if [ -z $1 ]; then
		echo "Provide a results directory."
		exit 1
	fi

	job_file="extract_jobs.txt"
	rm $job_file

	dir="$1"
	start_inst=1

	inst=$start_inst
	while [ -d "$dir/$inst" ]; do # for each instance

		if [ -f STOP_IMMEDIATELY ]; then
			# Signal to stop
			echo "Stopping because STOP_MMEDIATELY is present"
			exit
		fi

		echo "bash scripts/run/duplicates.sh --extract-exp-inst $dir/$inst" >> $job_file
		
		inst=$(($inst+1))
	done
	parallel -j 10 --tmpdir=/home/borowitz/sat/mallob/tmp  < $job_file
fi

if [ "$1" == "--clean-exp-inst" ]; then
	#
	# Removes produced clause files except for the archive.
	# Fails if FINISHED_EXTRACT file is not found.
	#
	shift 1

	inst_dir=$1

	if [ -z $inst_dir ]; then
		echo "Provide a instance results directory."
		exit 1
	fi

	if [ ! -f "$inst_dir/FINISHED_EXTRACT" ]; then
		echo "Skipping extraction for $inst_dir because FINISHED_EXTRACT was not found."
		echo "Add the file and re-run the script if you explicity want to do so."
		exit 1
	fi

	agg_filename_sorted="cls_produced_sorted.txt"
	agg_file_sorted="$inst_dir/$agg_filename_sorted"
	agg_sorted_zip="$inst_dir/cls_produced_sorted.tar.gz"
	agg_file="$inst_dir/cls_produced.txt"
	
	if [ -f $agg_file ]; then
		rm $agg_file
	fi

	p=0 # process

	if [ -f $agg_file_sorted ]; then
		rm $agg_file_sorted
	fi
	
	# delete single logs
	p=0 # process
	while [ -d "$inst_dir/$p" ]; do
		for log_path in $inst_dir/$p/produced_cls.*.log; do
			rm $log_path
		done
		p=$(($p+1))
	done
fi

if [ "$1" == "--extract-exp-inst" ]; then
	#
	# Given an instance it aggregates the solver-specific logs with produced clauses
	# and sorts them by hash (first, numeric, <), time (second, numeric, <)  
	#
	shift 1

	inst_dir=$1

	if [ -z $inst_dir ]; then
		echo "Provide a instance results directory."
		exit 1
	fi

	if [ -f "$inst_dir/FINISHED_EXTRACT" ]; then
		echo "Skipping extraction for $inst_dir because FINISHED_EXTRACT was found."
		echo "Remove the file and re-run the script if you explicity want to do so."
		exit 1
	fi

	agg_filename_sorted="cls_produced_sorted.txt"
	agg_file_sorted="$inst_dir/$agg_filename_sorted"
	agg_sorted_zip="$inst_dir/cls_produced_sorted.tar.gz"
	agg_file="$inst_dir/cls_produced.txt"
	rm $agg_file

	p=0 # process
	while [ -d "$inst_dir/$p" ]; do
		for log_path in $inst_dir/$p/produced_cls.*.log; do
			solver=${log_path#*produced_cls.}
			solver=${solver%.log}

			cat $log_path|awk -v p="$p" -v s="$solver" '{print $0,p,s}' >> $agg_file
		done
		p=$(($p+1))
	done

	#sort aggregated file
	if [ -f $agg_file ]; then
		rm $agg_file_sorted
		sort -k2n -k1n -o $agg_file_sorted $agg_file
		status=$? # exit status of sort

		if [ $status -eq 0 ]; then 
			rm $agg_file
		else
			echo "Sort failed for $agg_file"
			exit 1
		fi

		# eval experiment
		bash scripts/run/duplicates.sh --eval-inst "$inst_dir"

		# compress agg sorted cls
		tar -czvf "$agg_sorted_zip" -C "$inst_dir" "$agg_filename_sorted"
		status=$?

		if [ $status -eq 0 ]; then 
			rm $agg_file_sorted
		else
			echo "Tar failed for $agg_file_sorted"
			exit 1
		fi

	fi

	touch "$inst_dir/FINISHED_EXTRACT"

	if [ -f $agg_file_sorted ]; then
		# delete single logs
		p=0 # process
		while [ -d "$inst_dir/$p" ]; do
			for log_path in $inst_dir/$p/produced_cls.*.log; do
				rm $log_path
			done
			p=$(($p+1))
		done
	fi
fi

if [ "$1" == "--eval-exp" ]; then
	#
	# For each instance it aggregates the solver-specific logs with produced clauses
	# and sorts them by hash (first, numeric, <), time (second, numeric, <)  
	#
	shift 1

	if [ -z $1 ]; then
		echo "Provide a results directory."
		exit 1
	fi

	job_file="eval_jobs.txt"
	rm $job_file

	dir="$1"
	start_inst=1

	inst=$start_inst
	while [ -d "$dir/$inst" ]; do # for each instance

		if [ -f STOP_IMMEDIATELY ]; then
			# Signal to stop
			echo "Stopping because STOP_MMEDIATELY is present"
			exit
		fi

		echo "bash scripts/run/duplicates.sh --eval-inst $dir/$inst" >> $job_file
		
		inst=$(($inst+1))
	done
	parallel -j 5 --tmpdir=/home/borowitz/sat/mallob/tmp < $job_file
fi

if [ "$1" == "--eval-inst" ]; then
	#
	# Evaluate extracted produced clauses (sorted by hash,time) 
	#
	shift 1

	inst_dir=$1

	if [ -z $inst_dir ]; then
		echo "Provide a instance results directory."
		exit 1
	fi

	agg_file_sorted="$inst_dir/cls_produced_sorted.txt"
	agg_sorted_zip="$inst_dir/cls_produced_sorted.tar.gz"

	if [ ! -f $agg_file_sorted ]; then
		# check if the compressed version does exist
		if [ -f $agg_sorted_zip ]; then
			tar -xvf $agg_sorted_zip -C $inst_dir

			if [ ! -f $agg_file_sorted ]; then
				echo "Stopping because $agg_file_sorted does not exist"
				exit 1
			fi
		fi
	fi

	# OVERALL SOLVER
	dup_stat_file="$inst_dir/dup_stat.txt"
	# reports=hash reports, 
	# NR=unique hashes that were reported, 
	# sum-NR=reports with known hash, 
	# dup_hashes=unique hashes that were reported at least twice
	hash_reports=$(cat $agg_file_sorted|wc -l)
	cat $agg_file_sorted|awk '{print $2}'|uniq -c|awk -v reports="$hash_reports" \
	'{sum+=$1; if ($1>1) { dup_hashes+=1}} END{print reports,NR,sum-NR,dup_hashes}' > $dup_stat_file

	# time series of reports with hashes that were reported at least twice
	# printing time of first report time
	time_series_file="$inst_dir/cls_time_series.txt"
	cat $agg_file_sorted|awk 'BEGIN{last_h=""} {if (last_h == $2) {print $1;} else {last_h=$2;}}'|sort -k1n > $time_series_file
	# printing time of report time-first report time
	time_series_rel_file="$inst_dir/cls_time_series_rel.txt"
	cat $agg_file_sorted|awk 'BEGIN{first_t=0;last_h=""} {if (last_h == $2) {print $1-first_t;} else {first_t=$1;last_h=$2;}}'|sort -k1n > $time_series_rel_file

	# PAIRWISE
	filtered_agg_file_sorted="$inst_dir/filtered_produced_sorted.txt"
	pw_dup_file="$inst_dir/pw_dup.txt"
	# keep only hash, process, solver, then sort the lines and keep lines that appear at least twice
	# we do so as we count only duplicate hashes between solvers, and do not count how often a solver reported a hash
	cat $agg_file_sorted|awk '{print $2,$5,$6}'|sort|uniq -cd|awk '{print $2,$3,$4}' > $filtered_agg_file_sorted
	python3 scripts/run/pw_duplicates.py --extract-pw-inst "$filtered_agg_file_sorted" > $pw_dup_file
fi


if [ "$1" == "--eval-global" ]; then
	#
	# Make sure you called --eval-inst for every experiment before.
	# This script gathers and evaluates your experiment over all instances
	#
	shift 1
	
	dir=$1

	if [ -z $dir ]; then
		echo "Provide a results directory."
		exit 1
	fi
	
	dup_stat="$dir/dup_stat.txt"
	rm $dup_stat

	inst=1
	while [ -d "$dir/$inst" ]; do	
		if [ -f "$dir/$inst/dup_stat.txt" ]; then
			res=$(cat "$dir/$inst/dup_stat.txt")
			echo "$inst $res"
		       	echo "$inst $res" >> $dup_stat
		fi
		inst=$(($inst+1))
	done

	res=$(cat $dup_stat|awk 'BEGIN {log_sum_dup_reports=0;log_sum_dup_hashes=0} {log_sum_dup_reports+=log($4/$2);log_sum_dup_hashes+=log($5/$3)} END {print exp(1/NR * log_sum_dup_reports), exp(1/NR * log_sum_dup_hashes);}')
	echo $res
fi

if [ "$1" == "--extract" ]; then
	shift 1

	if [ -z $1 ]; then
		echo "Provide a results directory."
		exit 1
	fi

	tmp_file2="$1/cls_produced2.tmp" # produced clauses sorted by hash
	if [ ! -e "$tmp_file2" ]; then
		echo "$tmp_file2 does not exists."
		exit 1
	fi

	out_file="$1/stat.out"
	rm $out_file

	dup=$(cat $tmp_file2|awk '{print $2}'|uniq -cd |awk '{sum+=$1;} END{print sum-NR;}')
        amount=$(cat $tmp_file2|wc -l)
        rel=$(echo "scale=6;$dup/$amount"|bc)
	i=${1%"/"}
	i=${i##*"/"}
        echo "$i $dup $amount $rel"
        echo "$i $dup $amount $rel">> $out_file

	exit 0
fi

if [ "$1" == "--time-extract" ]; then
	shift 1

	if [ -z $1 ]; then
		echo "Provide a results directory."
		exit 1
	fi

	tmp_file2="$1/cls_produced2.tmp" # produced clauses sorted by hash
	if [ ! -e "$tmp_file2" ]; then
		echo "$tmp_file2 does not exists."
		exit 1
	fi

	out_file="$1/time_stat.out"
	rm $out_file

	cat $tmp_file2|awk 'BEGIN{first_t=0;last_h=""} {if (last_h == $2) {print first_t,$0;} else {first_t=$1;last_h=$2;}}' >> $out_file

	exit 0
fi



if [ "$1" == "--pw-extract" ]; then
	shift 1

	if [ -z $1 ]; then
		echo "Provide a results directory."
		exit 1
	fi

	stat="$1/pw_stat.out"

	rm $stat

	p=0
	while [ -d "$1/$p" ]; do
		for s in $1/$p/produced_cls.*.log; do
			solver=${s#*produced_cls.}
			solver=${solver%.log}
			cat $s|awk '{print $2}'|sort|uniq|sort > $1/$p/hash_produced_cls.$solver.log
		done
		p=$((p+1))		
	done

	p=0
	while [ -d "$1/$p" ]; do
		p2=$p
		while [ -d "$1/$p2" ]; do
			for s in $1/$p/hash_produced_cls.*.log; do
				for s2 in $1/$p2/hash_produced_cls.*.log; do
					solver=${s#*hash_produced_cls.}
					solver2=${s2#*hash_produced_cls.}
					solver=${solver%.log}
					solver2=${solver2%.log}
					if [ "$p2" -gt "$p" ] || [ "$solver2" -gt "$solver" ]; then
						duphash=$(comm -1 -2 $s $s2|wc -l) # hashes that appear more than once
						amount=$(cat $s $s2 |wc -l)
						amount=$(($amount-$duphash)) # unique hashes
						echo "$p $solver $p2 $solver2 $duphash $amount" >> $stat
						echo "$p2 $solver2 $p $solver $duphash $amount" >> $stat
						echo "$p $solver $p2 $solver2 $duphash $amount"
					fi
				done
			done
			p2=$(($p2+1))
		done
		p=$(($p+1))
	done
	
	p=0
	while [ -d "$1/$p" ]; do
		for s in $1/$p/hash_produced_cls.*.log; do
			rm $s
		done
		p=$((p+1))		
	done

	exit 0
fi

exit 1
