#!/bin/bash
set -e


FILENAME=mat5.txt
OUTFILE="${FILENAME%txt}out"

if [[ 'x'$1 == 'x' ]]; 
then
    echo "No arguements, simple run, for debug ./run.sh d, for valgrind use v"
    make -B
    ./main.exec ${FILENAME} ${OUTFILE}
else
    if [[ 'x'$1 == 'xv' ]];
    then
        make -B DEBUG=y
        echo "Running with valgrind"
            valgrind --suppressions=val.supp --track-origins=yes --show-reachable=yes --leak-check=full --log-file=val.out \
            ./main.exec ${FILENAME} ${OUTFILE}
    else
        if [[ 'x'$1 == 'xd' ]];
        then
            make -B DEBUG=y
            echo "Running with debug"
            ./main.exec ${FILENAME} ${OUTFILE}
        else
            echo "Bad arguements, no run, for debug ./run.sh d"
        fi
    fi
fi
