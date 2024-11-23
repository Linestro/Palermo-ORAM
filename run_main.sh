set -e
set -x 



make -j



mkdir -p results/
workloads=("random_addr" "stream_addr" "redis" "mcf" "lbm" "pagerank" "graphsearch" "criteo" "dblp" "orca")
modes=("oram") # "pathoram" "pageoram") # "ringoram" "idealoram")
prefetch_lens=(1 2 4 8)

for prefetch_len in "${prefetch_lens[@]}"
do
    for workload in "${workloads[@]}"
    do
    i=24
    num_of_ways=$(bc <<< "2^$i")
    ./ramulator configs/DDR4-config.cfg --mode="oram" --stats results/${workload}_output.txt --num_of_ways $((num_of_ways / prefetch_len)) ./oram_traces/${workload}_1000000_prefetch_${prefetch_len}.txt > results/"oram"_${workload}_prefetch_${prefetch_len}.log &
    done
    wait
done

wait
echo "Finished"