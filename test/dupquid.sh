../bin/genquid 2000 > dupchk
sort dupchk | uniq -c -d
rm -rf dupchk
