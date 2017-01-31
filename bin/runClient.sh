#!/bin/bash
for i in `seq 10`; do 
	./rdmaPingPong client 1234 127.0.0.1 2> /dev/null \
	| perl -ne 'if (/(.*) msg\/s/) { 
		print "$1;";
	}';
	sleep 1
done
printf "\n"
