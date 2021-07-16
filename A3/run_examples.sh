#!/bin/bash  
# Bash script to run the ./ospf executable with combinations of input parameters

# compile the cpp program and generate the ospf executable
g++ ospf.cpp -o ospf -lpthread
if [ -d "ExamplesOutput" ]; then rm -Rf ExamplesOutput; fi
mkdir ExamplesOutput

# First Example File
# read number of nodes from file
read -r line < example-input1
read -ra ARR <<< "$line"
num_nodes=${ARR[0]}

# iterating through parameter values
count=0
while [ ${count} -lt ${num_nodes} ]
do
    bash -c "exec -a process1-${count} ./ospf -i ${count} -f example-input1 -o ./ExamplesOutput/output1 &" ;
    count=$((count+1)) ;
done

sleep 100

# kill the processes created
count=0
while [ ${count} -lt ${num_nodes} ]
do
    pkill -f process1-${count} ;
    count=$((count+1)) ;
done

# Second Example File
# read number of nodes from file
read -r line < example-input2
read -ra ARR <<< "$line"
num_nodes=${ARR[0]}

# iterating through parameter values
count=0
while [ ${count} -lt ${num_nodes} ]
do
    bash -c "exec -a process2-${count} ./ospf -i ${count} -f example-input2 -o ./ExamplesOutput/output2 &" ;
    count=$((count+1)) ;
done

sleep 100

# kill the processes created
count=0
while [ ${count} -lt ${num_nodes} ]
do
    pkill -f process2-${count} ;
    count=$((count+1)) ;
done
