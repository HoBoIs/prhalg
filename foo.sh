#!/bin/bash
echo "foo"
echo $1
pids=()
for j in `seq 1 $1`; do
  echo "1 19" |./a.out&
  pids[${j}]=$!
done
for pid in ${pids[*]}; do
  wait $pid
done

