#!/usr/bin/env sh
./bench server > /dev/null &
sleep 1
./bench client | column -s, -t