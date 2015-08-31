#!/bin/bash
COUNT=200000
../bin/genquid $COUNT > dupchk
if [[ -n $(cut -f 2 -d ' ' dupchk | sort | uniq -c -d) ]]; then
  echo "Duplicate keys found"
  exit 1
fi
rm -rf dupchk
