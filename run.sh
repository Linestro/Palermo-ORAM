set -e
set -x 



# mkdir -p results/
# make -j

compile_ktree(){
# ktree=$1  

# sed -i '42d' src/reservation.h
# sed -i '42i #define NUM_OF_WAYS '''${ktree}'''' src/reservation.h


make -j


for ((i=26; i>=0; i-=1)); do
    ktree=$(bc <<< "2^$i")
    # echo "2^$i = $result"
    cp ramulator ramulator_${ktree}
    mkdir -p results_${ktree}/
done

# mv ramulator ramulator_${ktree}

modes=("oram") # "pathoram" "pageoram") # "ringoram" "idealoram")
# modes=("oram" "idealoram" "ringoram" "pathoram" "dram")
for mod in "${modes[@]}"
do
    # Echo each string
    echo "$mod"
    # ./ramulator_${ktree} configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt dram.trace > results_${ktree}/${mod}.log &
    # gdb -ex=r --args ./ramulator_${ktree} configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt random_addr_10000.trace > results_${ktree}/${mod}.log &
    
    for ((i=24; i>=24; i-=1)); do
        ktree=$(bc <<< "2^$i")

        ktree=$(( ktree / 4 ))
        ./ramulator_${ktree} configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways ${ktree} random_addr_250000.trace > results_${ktree}/${mod}.log &
    done

    # for ((i=11; i>=11; i-=1)); do
    #     ktree=$(bc <<< "2^$i")
    #     ./ramulator_${ktree} configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways ${ktree} random_addr_1000000.trace > results_${ktree}/${mod}.log &
    # done

    # for ((i=24; i>=17; i-=1)); do
    #     ktree=$(bc <<< "2^$i")
    #     ./ramulator_${ktree} configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways ${ktree} random_addr_1000000.trace > results_${ktree}/${mod}.log &
    # done

    # wait

    # # for ((i=18; i>=12; i-=1)); do
    # #     ktree=$(bc <<< "2^$i")
    # #     ./ramulator_${ktree} configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways ${ktree} random_addr_1000000.trace > results_${ktree}/${mod}.log &
    # # done

    # wait

    # for ((i=16; i>=9; i-=1)); do
    #     ktree=$(bc <<< "2^$i")
    #     ./ramulator_${ktree} configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways ${ktree} random_addr_1000000.trace > results_${ktree}/${mod}.log &
    # done

    # wait

    # for ((i=8; i>=0; i-=1)); do
    #     ktree=$(bc <<< "2^$i")
    #     ./ramulator_${ktree} configs/DDR4-config.cfg --mode=${mod} --stats my_output.txt --num_of_ways ${ktree} random_addr_1000000.trace > results_${ktree}/${mod}.log &
    # done

    # wait

done

wait
}

compile_ktree

wait
echo "Finished"