set -e
set -x 



# mkdir -p results/
# make -j

compile_ktree(){
prefetch_len=$1  

sed -i '28d' src/reservation.h
sed -i '28i #define EMBED_DIM '''$(( 16*prefetch_len ))'''' src/reservation.h


make -j


# for ((i=26; i>=0; i-=1)); do
#     ktree=$(bc <<< "2^$i")
#     # echo "2^$i = $result"
#     cp ramulator ramulator_${ktree}
#     mkdir -p results_${ktree}/
# done

# mv ramulator ramulator_${ktree}
mkdir -p results_all/

func(){
    workload=$1
    echo ${workload}
      # if [ "$workload" == "random_addr" ];
      # then
      #   path="/data3/haojie/oram_traces/${workload}_1000000_prefetch_${prefetch_len}.trace"
      # else 
        path="/data3/haojie/oram_traces/${workload}_1000000_prefetch_${prefetch_len}.txt"
        # path="${workload}_2000.txt"
      # fi
        ls ${path}
        for ((i=24; i>=24; i-=1)); do
            ktree=$(bc <<< "2^$i")
            ./ramulator configs/DDR4-config.cfg --mode="oram" --stats results_all/${workload}_output.txt --num_of_ways $((ktree / prefetch_len)) ${path} > results_all/"oram"_${workload}_prefetch_${prefetch_len}.log &
        done
}
workloads=("random_addr" "stream_addr" "redis" "mcf" "lbm" "pagerank" "graphsearch" "criteo" "dblp" "orca")
workloads=("stream_addr" "criteo" "redis" "mcf" "pagerank") # "graphsearch" "criteo" "dblp" "orca")

# echo "All finished"

modes=("oram") # "pathoram" "pageoram") # "ringoram" "idealoram")
# modes=("oram" "idealoram" "ringoram" "pathoram" "dram")
for mod in "${modes[@]}"
do
    echo "$mod"        
    for workload in "${workloads[@]}"
    do
        func $workload
    done
    wait
done

wait

}


prefetch_lens=(1 2 4 8)
prefetch_lens=(8 4 2 1)
prefetch_lens=(16)
for p in "${prefetch_lens[@]}"
do
compile_ktree ${p}
done
wait

echo "Finished"