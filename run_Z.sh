set -e
set -x 



# mkdir -p results/
# make -j

compile_ktree(){
# ktree=$1  

# sed -i '42d' src/reservation.h
# sed -i '42i #define NUM_OF_WAYS '''${ktree}'''' src/reservation.h


# make -j


# for ((i=26; i>=0; i-=1)); do
#     ktree=$(bc <<< "2^$i")
#     # echo "2^$i = $result"
#     cp ramulator ramulator_${ktree}
#     mkdir -p results_${ktree}/
# done

# mv ramulator ramulator_${ktree}

mkdir -p results_Z_4/
mkdir -p results_Z_8/
mkdir -p results_Z_16/
mkdir -p results_Z_32/

modes=("oram") # "pathoram" "pageoram") # "ringoram" "idealoram")
# modes=("oram" "idealoram" "ringoram" "pathoram" "dram")
for mod in "${modes[@]}"
do
    # ./ramulator_Z_4 configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways 67108864 random_addr_1000000.trace > results_Z_4/${mod}.log &
    # ./ramulator_Z_8 configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways 33554432 random_addr_1000000.trace > results_Z_8/${mod}.log &
    # ./ramulator_Z_16 configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways 16777216 random_addr_1000000.trace > results_Z_16/${mod}.log &
    ./ramulator_Z_32 configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways 8388608 random_addr_1000000.trace > results_Z_32/${mod}.log &

done

wait
}

compile_ktree

wait
echo "Finished"