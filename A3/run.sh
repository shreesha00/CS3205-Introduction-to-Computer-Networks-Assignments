#!/bin/bash  
# Bash script to run the ./ospf executable with combinations of input parameters

# compile the cpp program and generate the ospf executable
g++ ospf.cpp -o ospf -lpthread
if [ -d "Output" ]; then rm -Rf Output; fi
mkdir Output

# First Example File
# read number of nodes from file
read -r line < input
read -ra ARR <<< "$line"
num_nodes=${ARR[0]}

# iterating through parameter values
count=0
while [ ${count} -lt ${num_nodes} ]
do
    bash -c "exec -a process-${count} ./ospf -i ${count} -f example-input1 -o ./Output/output &" ;
    count=$((count+1)) ;
done

sleep 100

# kill the processes created
count=0
while [ ${count} -lt ${num_nodes} ]
do
    pkill -f process-${count} ;
    count=$((count+1)) ;
done
