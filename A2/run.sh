#!/bin/bash  
# Bash script to run the ./cw executable with combinations of input parameters and plotting the same using a python script

# parameter values for plotting
Ki=(1 4)
Km=(1 1.5)
Kn=(0.5 1)
Kf=(0.1 0.3)
Ps=(0.01 0.0001)

# compile the cpp program and generate the cw executable
g++ cw.cpp -o cw
mkdir Output
count=1

# iterating through parameter values
for i in ${!Ki[@]}; do
    for m in ${!Km[@]}; do 
        for n in ${!Kn[@]}; do 
            for f in ${!Kf[@]}; do 
                for s in ${!Ps[@]}; do
                    ./cw -i ${Ki[$i]} -m ${Km[$m]} -n ${Kn[$n]} -f ${Kf[$f]} -s ${Ps[$s]} -T 3000 -o ./Output/plot_${count} ;
                    count=$((count+1)) ;
                done
            done
        done
    done
done

# generate the plots 
python plot.py