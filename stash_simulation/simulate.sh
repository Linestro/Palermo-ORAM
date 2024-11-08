set -x
set -e


compile(){
    z=$1
    s=$2
    a=$3
    sl=$4
    cdl=$5
    evct=$6
    set=$7

    sed -i '5d' ../src/ring_bucket.h
    sed -i '5i #define Z '''${z}'''' ../src/ring_bucket.h
    sed -i '6d' ../src/ring_bucket.h
    sed -i '6i #define S '''${s}'''' ../src/ring_bucket.h
    sed -i '7d' ../src/ring_bucket.h
    sed -i '7i #define A '''${a}'''' ../src/ring_bucket.h
    sed -i '24d' ../stash_simulation/set_oram.cc
    sed -i '24i #define CUT_LVL '''${sl}'''' ../stash_simulation/set_oram.cc
    sed -i '25d' ../stash_simulation/set_oram.cc
    sed -i '25i #define CUTDOWN_LVL '''${cdl}'''' ../stash_simulation/set_oram.cc
    sed -i '26d' ../stash_simulation/set_oram.cc
    sed -i '26i #define EVCT_TIMES '''${evct}'''' ../stash_simulation/set_oram.cc
    sed -i '29d' ../stash_simulation/set_oram.cc
    sed -i '29i #define SET_NUM '''${set}'''' ../stash_simulation/set_oram.cc
    g++ -g -fopenmp -O3 set_oram.cc -o ./bin/set_oram_${z}_${s}_${a}_${sl}_${cdl}_${evct}_${set}_t.out
}

run(){
    z=$1
    s=$2
    a=$3
    sl=$4
    cdl=$5
    evct=$6
    set=$7
    time ./bin/set_oram_${z}_${s}_${a}_${sl}_${cdl}_${evct}_${set}_t.out ../random_addr.trace > output_${z}_${s}_${a}_${sl}_${cdl}_${evct}_${set}_t.log
}

# compile 5 7 5
# compile 5 7 6
# compile 5 7 7

# compile 4 5 4
# compile 4 5 5
# compile 4 5 3


# compile 8 12 8
# compile 8 12 9
# compile 8 12 10

# run 4 5 3 &
# run 4 5 4 &
# run 4 5 5 &



# run 8 12 8 &
# run 8 12 9 &
# run 8 12 10 &

# run 5 7 5 &
# run 5 7 6 &
# run 5 7 7 &

# compile 4 5 3 14 5 1 
# compile 4 5 3 14 9 1 512
# compile 4 5 3 14 9 1 256
# compile 4 5 3 14 9 1 128
# compile 4 5 3 14 5 1 32
# compile 4 5 3 14 5 1 16
compile 4 5 3 14 1 1 2



# compile 4 5 3 14 9 1 128
# compile 4 5 3 14 5 2
# compile 4 5 3 5 5 2
# compile 4 5 3 5 5 3
# compile 4 5 3 5 5 4

# compile 4 5 2 14 5
# compile 4 5 2 14 5
# compile 4 5 2 14 5


# run 4 5 3 5 5 1 &
# run 4 5 3 14 9 1 512 &
# run 4 5 3 14 9 1 256 &
# run 4 5 3 14 9 1 128 &
# run 4 5 3 14 5 1 32 &
# run 4 5 3 14 5 1 16 &
# run 4 5 3 14 5 1 8 &
run 4 5 3 14 1 1 2 &

# run 4 5 3 14 9 1 128 &

# run 4 5 3 14 5 2 &

# run 4 5 3 20 5 3 &
# run 4 5 3 20 5 4 &
# run 4 5 3 5 5 2 &
# run 4 5 3 5 5 3 &
# run 4 5 3 5 5 4 &
# run 4 5 2 14 5 &
# run 4 5 2 14 5 &
# run 4 5 2 14 5 &

wait


echo "All Finished"