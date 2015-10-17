#!/bin/bash
COUNT=2000
for i in `seq 1 50`; do
	echo "ROUND $i"
	echo "Count $(($COUNT*$i*10))"
	bin/genlookup3 $(($COUNT*$i*10)) > collchk
	echo "Duplicate $(cut -f 2 -d ' ' collchk | sort | uniq -c -d | wc -l)"
	rm -rf collchk
done  