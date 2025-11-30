#!/bin/bash

# Compile the simulator
gcc -o sim sim.c

# Function to extract misprediction rate
get_rate() {
    ./sim gshare $1 $2 $3 | grep "Misprediction Rate" | awk '{print $3}' | tr -d '%'
}

# Part A: Fix M=4, Vary N=1,2,3,4
echo "N MCF GoBMK" > data_part_a.dat
for n in 1 2 3 4; do
    rate_mcf=$(get_rate 4 $n mcf_trace.txt)
    rate_gobmk=$(get_rate 4 $n gobmk_trace.txt)
    echo "$n $rate_mcf $rate_gobmk" >> data_part_a.dat
done

# Part B: Fix N=4, Vary M=4,5,6,7
echo "M MCF GoBMK" > data_part_b.dat
for m in 4 5 6 7; do
    rate_mcf=$(get_rate $m 4 mcf_trace.txt)
    rate_gobmk=$(get_rate $m 4 gobmk_trace.txt)
    echo "$m $rate_mcf $rate_gobmk" >> data_part_b.dat
done

# Part C: Fix N=0, Vary M=4,5,6,7
echo "M MCF GoBMK" > data_part_c.dat
for m in 4 5 6 7; do
    rate_mcf=$(get_rate $m 0 mcf_trace.txt)
    rate_gobmk=$(get_rate $m 0 gobmk_trace.txt)
    echo "$m $rate_mcf $rate_gobmk" >> data_part_c.dat
done

cat data_part_a.dat
cat data_part_b.dat
cat data_part_c.dat
