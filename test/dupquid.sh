#!/bin/bash
COUNT=200000
../bin/genquid $COUNT > dupchk
if [[ -n $(sort dupchk | uniq -c -d) ]]; then
  echo "Duplicate keys found"
  exit 1
fi
rm -rf dupchk
