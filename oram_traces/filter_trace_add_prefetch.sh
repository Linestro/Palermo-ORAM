


paths=(
    "mcf.txt"
    "lbm.txt"
    "pagerank.txt"
    "graphsearch.txt"
    "criteo.txt"
    "dblp.txt"
    "orca.txt"
    "redis.txt"
    "random_addr.txt"
    "stream_addr.txt"
)

l=$1

for path in "${paths[@]}"
do
    ls ${path}     
wait
wait
done

ps=(1 2 4 8)
ps=(16)

for p in "${ps[@]}"
do
for path in "${paths[@]}"
do
    if [[ "$path" == "criteo.txt" ]]; then
        python filter_trace_add_prefetch.py -i ${path} -e 4 -l ${l} -p ${p} &
    elif [[ "$path" == "dblp.txt" ]]; then
        python filter_trace_add_prefetch.py -i ${path} -e 2 -l ${l} -p ${p} &
    elif [[ "$path" == "orca.txt" ]]; then
        python filter_trace_add_prefetch.py -i ${path} -e 12 -l ${l} -p ${p} &
    else
        python filter_trace_add_prefetch.py -i ${path} -e 1 -l ${l} -p ${p} &
    fi
    
# wait
done
done

wait

 echo "All done"