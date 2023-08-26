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

	done
	parallel -j 10 < $job_file
fi

if [ "$1" == "--extract-exp-inst" ]; do
	shift 1

	inst_dir=$1

	if [ -z $inst_dir ]; then
		echo "Provide a instance results directory."
		exit 1
	fi


	agg_file_sorted="$inst_dir/cls_produced_sorted.tmp"
	agg_file="$inst_dir/cls_produced.tmp"
	rm $agg_file

	p=0 # process
	while [ -d "$inst_dir/$p"]; do
		for log_path in $inst_dir/$p/produced_cls.*.log; do
			solver=${log_path#produced_cls.}
			solver=${solver%.log}

			cat $log_path|awk -v p="$p" -v s="$solver" '{print $0,p,s}' >> $agg_file
		done
		p=$(($p+1))
	done

	#sort aggregated file
	if [ -f $agg_file ]; do
		rm $agg_file_sorted
		sort -k2n -k1n -o $agg_file_sorted $agg_file
		status=$? # exit status of sort

		if [ $status -eq 0 ]; then 
			rm $agg_file
		else
			echo "Sort failed for $inst_dir"
			exit 1
		fi
	fi


	if [ -f $agg_file_sorted ]; then
		# delete single logs
		p=0 # process
		while [ -d "$inst_dir/$p"]; do
			for log_path in $inst_dir/$p/produced_cls.*.log; do
				rm $log_path
			done
			p=$(($p+1))
		done
	fi
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
