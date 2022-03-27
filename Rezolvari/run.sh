#!/bin/bash

input=""
for i in "${@:2}"; do
    input="${input} $i"
done
run_file=$1
TIMEFORMAT=%R
( time ./$run_file $input > /dev/null )  2>${run_file}_time.txt
./$run_file $input > ${run_file}_res.txt

#insirat de pe https://stackoverflow.com/questions/3795470/how-do-i-get-just-real-time-value-from-time-command
#https://www.cyberciti.biz/faq/unix-linux-time-command-examples-usage-syntax/