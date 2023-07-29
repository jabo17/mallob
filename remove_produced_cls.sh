#!/bin/bash


basedir=$1
until=$2

i=1

while [ -d "$basedir/$i" ] && [ "$i" -lt "$until" ]; do
	echo "Removing produced clauses from $basedir/$i"
	rm $basedir/$i/*/produced_cls.*.log

	i=$((i+1))
done
