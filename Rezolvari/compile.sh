#!/bin/bash

if [ $# -eq 0 ]; then
    exit -1
fi

gcc ${1}.c -o ${1} 2> /dev/null > /dev/null

if [[ $? -ne 0 ]]; then
    exit -1
else
    exit 0
fi