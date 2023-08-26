#!/bin/bash


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
