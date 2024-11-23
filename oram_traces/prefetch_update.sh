

func(){
    workload=$1
    echo ${workload}
      if [ "$workload" == "random_addr" ];
      then
        path="/data3/haojie/oram_traces/${workload}_1000000.trace"
      else 
        path="/data3/haojie/oram_traces/${workload}_1000000.txt"
      fi
    
    python prefetch_update.py -i ${path} -p 1 &
    python prefetch_update.py -i ${path} -p 2 &
    python prefetch_update.py -i ${path} -p 4 &
    python prefetch_update.py -i ${path} -p 8 &
    wait
}


workloads=("random_addr" "stream_addr" "redis" "mcf" "lbm" "pagerank" "graphsearch" "criteo" "dblp" "orca")

for workload in "${workloads[@]}"
do
    func $workload &
done
wait
echo "All done"
