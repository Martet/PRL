#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Usage: $0 <input file> <number of steps>";
    exit 1;
fi;

#preklad
mpic++ --prefix /usr/local/share/OpenMPI -o life life.cpp 

#spusteni programu
mpirun --prefix /usr/local/share/OpenMPI -np 2 life $1 $2

#uklid
rm -f life
