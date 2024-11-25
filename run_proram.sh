set -e
set -x 



make clean
make -j


mkdir -p proram_results/
workloads=("random_addr" "stream_addr" "redis" "mcf" "lbm" "pagerank" "graphsearch" "criteo" "dblp" "orca")
modes=("pathoram")
prefetch_lens=(1 2 4 8)

for prefetch_len in "${prefetch_lens[@]}"
do
for workload in "${workloads[@]}"
do
for mod in "${modes[@]}"
do
    ./ramulator configs/DDR4-config.cfg --mode=${mod} --stats proram_results/${workload}_output.txt ./oram_traces/${workload}_1000000_prefetch_${prefetch_len}.txt > proram_results/${mod}_${workload}_prefetch_${prefetch_len}.log &
done
done
wait
done


wait
echo "Finished"
