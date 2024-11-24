set -e
set -x 



make -j


mkdir -p palermo_sw_results/


workloads=("random_addr" "stream_addr" "redis" "mcf" "lbm" "pagerank" "graphsearch" "criteo" "dblp" "orca")

prefetch_len=1
for workload in "${workloads[@]}"
do

i=24
num_of_ways=$(bc <<< "2^$i")
./ramulator configs/DDR4-config.cfg --mode="oram" --stats palermo_sw_results/${workload}_output.txt --num_of_ways $((num_of_ways / prefetch_len)) ./oram_traces/${workload}_1000000_prefetch_${prefetch_len}.txt > palermo_sw_results/"palermo_sw"_${workload}_prefetch_${prefetch_len}.log &

done

wait
echo "Finished"