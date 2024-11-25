#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>
#include <vector>
#include <random>
#include <cassert>
#include <cstring>
#include "timer.h"
#include "ring_bucket.h"
#include <functional>
#include <limits.h>
#include <set>
#include <cmath>
#include <queue>
#include <deque>
#include "cache.h"


#define START_MEM 0x000000000
#define INDIV_TABLE_SIZE 0x100000000
#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)
#define KTREE 2

#define EMBED_DIM 16
#define EMBED_REPEAT  ((int) (EMBED_DIM) / (64 / 4))
#define CACHED_NUM_NODE  ((int) 1024 / EMBED_REPEAT)               // 4 MB cache: 4*1024*1024 / 64  =  32768
#define TOTAL_NUM_TOP_NODE  ((int) 67108864 / EMBED_REPEAT)         // 8 GB Mem (16 GB with 50% dummy) =  67108864 
#define STASH_MAITANENCE  128


#define NODEID_METADATA_START   0x7000000
#define POS1_METADATA_START     0x8000000
#define POS2_METADATA_START     0x9000000

#define DATA_TREE_START         0x0000000
#define POS1_TREE_START         0x2000000
#define POS2_TREE_START         0x4000000

using namespace std;

struct pos_map_line{
    long long original_address = 0xdeadbeef;
    long long block_id = -1;
    long long node_id = -1;
    int offset = -1;
    bool pending = false;
};

struct rs_line{
    public:
    bool is_write;
    string type;
    long long orig_addr;
    int num_of_lvls;
    int line_id = -1;
    vector<long long> oram_node_id;
    vector<int> bucket_offset;
    vector<long long> oram_lvl_addr;
    vector<bool> oram_finished_vec_for_read;
    int leading_write_line_num = -1;
    /////////////////////////
    vector<long long> oram_addr_for_pull;
    vector<pair<bool, string>> oram_finished_vec_for_pull;
    vector<long long> oram_addr_for_write;
    vector<bool> oram_finished_vec_for_write;
    /////////////////////////
    vector<long long> oram_addr_for_early_pull;
    vector<pair<bool, string>> oram_finished_vec_for_early_pull;
    vector<long long> oram_addr_for_early_write;
    vector<bool> oram_finished_vec_for_early_write;
    int oram_unfinished_cnt = -1;
    long long result;
    bool valid = false;
    bool ready = false;
    rs_line(int n_line_id, int n_lvls){
        num_of_lvls = n_lvls;
        line_id = n_line_id;
        assert(num_of_lvls > 0);
        orig_addr = 0xdeadbeef;
        is_write = false;
        oram_node_id.resize(num_of_lvls, -1);
        bucket_offset.resize(num_of_lvls, -1);
        oram_lvl_addr.resize(num_of_lvls, -1);
        oram_finished_vec_for_read.resize(num_of_lvls, false);

        oram_addr_for_pull.resize((Z + S) * num_of_lvls, -1);
        oram_finished_vec_for_pull.resize((Z + S) * num_of_lvls, make_pair(false, "unknown"));
        oram_addr_for_write.resize((Z + S) * num_of_lvls, -1);
        oram_finished_vec_for_write.resize((Z + S) * num_of_lvls, false);
        oram_unfinished_cnt = 0;

        oram_addr_for_early_pull.resize((Z + S), -1);
        oram_finished_vec_for_early_pull.resize((Z + S), make_pair(false, "unknown"));
        oram_addr_for_early_write.resize((Z + S), -1);
        oram_finished_vec_for_early_write.resize((Z + S), false);
        valid = false;
        ready = false;
    }

    bool update_ready_bit(){
        assert(valid && !ready && oram_unfinished_cnt > 0);
        oram_unfinished_cnt--;
        assert(oram_unfinished_cnt >= 0);
        if(oram_unfinished_cnt == 0){
            ready = true;
            return true;
        }
        return false;
    }

    void printline(){
        cout << "L# " << std::dec << line_id << " : ";
        if(!valid){
            cout << "Invalid" << endl;
            return;
        }
        cout << valid << " | " << type << " | " << is_write << " | " << leading_write_line_num << " | 0x" << std::hex << orig_addr << std::dec << " || ";
        if(type == "read-pull"){
            cout << " RPath: " << oram_node_id[num_of_lvls - 1] << " | ";
            for(int i = 0; i < num_of_lvls; i++){
                cout << oram_node_id[i] << "," << bucket_offset[i] << ",0x" << std::hex << oram_lvl_addr[i] << std::dec << "," << oram_finished_vec_for_read[i] << " | ";
            }
        }
        else if(type == "write-pull"){
            cout << " WPath: " << oram_node_id[num_of_lvls - 1] << " | ";
            for(int i = 0; i < (Z+S)*num_of_lvls; i++){
                cout << "0x" << std::hex << oram_addr_for_pull[i] << std::dec << "," << oram_finished_vec_for_pull[i].first << " | ";
            }
        }
        else if(type == "write-push"){
            cout << " WPath: " << oram_node_id[num_of_lvls - 1] << " | ";
            for(int i = 0; i < (Z+S)*num_of_lvls; i++){
                cout << "0x" << std::hex << oram_addr_for_write[i] << std::dec << "," << oram_finished_vec_for_write[i] << " | ";
            }
        }
        else if (type == "early-pull"){
            cout << " Node: " << oram_node_id[0] << " | ";
            for(int i = 0; i < Z+S; i++){
                cout << "0x" << std::hex << oram_addr_for_early_pull[i]  << std::dec << "," << oram_finished_vec_for_early_pull[i].first << " | ";
            }
        }
        else if (type == "early-push"){
            cout << " Node: " << oram_node_id[0] << " | ";
            for(int i = 0; i < (Z+S); i++){
                cout << "0x" << std::hex << oram_addr_for_early_write[i] << std::dec << "," << oram_finished_vec_for_early_write[i] << " | ";
            }
        }
        else if (type == "init"){
            cout << "Init dummy state";
        }
        else{
            assert(0);
        }
        cout << "| " << ready << endl;
    }
};


class mshr_entry{
    public:
    int rs_row_num;
    int rs_offset;
    bool pull;
    string type;
    mshr_entry(int n_rs_row_num, int n_rs_offset, bool n_pull, string n_type){
        rs_row_num = n_rs_row_num;
        rs_offset = n_rs_offset;
        pull = n_pull;
        type = n_type;
    }
    bool is_equal(const mshr_entry& rhs)
    {
        return (rs_row_num == rhs.rs_row_num) && (rs_offset == rhs.rs_offset);
    }
    void mshr_print(){
        if(pull){
            cout << "(" << rs_row_num << "," << rs_offset << ",pull," << type << ")" ;
        }
        else{
            cout << "(" << rs_row_num << "," << rs_offset << ",push," << type << ")" ;
        }
    }
};

class ramulator_packet{
    public:
    long long address;
    bool pull;
    bool metadata = false;
    string name;
    ramulator_packet(long long n_address, bool n_pull, bool n_metadata, string n_name){
        address = n_address;
        pull = n_pull;
        metadata = n_metadata;
        name = n_name;
    }
};

class rs{
    public:
    string type = "oram";
    int num_of_entries = -1;
    int num_of_lvls = -1;
    int lowest_uncached_lvl = 0;
    
    vector<rs_line*> rs_vec;
    vector<pair<long long, int>> early_reshuffle_nodes;
    int head = 0;
    int tail = 0;
    int last_write_line = -1;
    vector<long long> node_id_vec;
    vector<int> offset_vec;
    vector<ramulator_packet> ramualtor_input_vec;  // bool is pull, read: true
    
    map<long long, pair< vector<mshr_entry>, int > > mshr_array;     // address -> rs [row : col], [0, 0, 0, 0 .., 0]
    map<long long, pos_map_line> addr_to_origaddr; // real addr -> address
    map<long long, pos_map_line> posMap;        // address -> node, offset
    map<long long, long long> stash;            // address -> leaf (not assigned to binary tree yet so we do not have offset)

    // vector<long long> outstanding_req;
    long long rs_read_ddr_lines = 0;
    long long rs_write_ddr_lines = 0;
    long long rs_early_ddr_lines = 0;

    long long rs_read_rs_lines = 0;
    long long rs_write_rs_lines = 0;
    long long rs_early_rs_lines = 0;

    long long stash_size_max = 0;
    long long stash_size_total = 0;
    long long stash_sample_times = 0;

    int tree_move_factor = log2(KTREE);
    std::vector<long long> lvl_base_lookup;
    std::vector<long long> leaf_division;

    map<int, long long> lvl_counter;

    long long useful_read_pull_ddr = 0;
    long long dummy_read_pull_ddr = 0;    
    long long useful_early_pull_ddr = 0;
    long long useful_early_push_ddr = 0;
    long long useful_write_pull_ddr = 0;
    long long useful_write_push_ddr = 0;
    long long dummy_early_pull_ddr = 0;
    long long dummy_early_push_ddr = 0;
    long long dummy_write_pull_ddr = 0;
    long long dummy_write_push_ddr = 0;
    long long stash_violation = 0;

    long long check_nodeid_ddr = 0;
    long long update_nodeid_ddr = 0;

    set_cache* metadata_cache = new set_cache(64 * 1024, 8);

    vector<ring_bucket>* ring_bucket_vec = new vector<ring_bucket>();
    string name;   
    map<string, long long> stall_reason;

    rs(int n_lines, long long total_num_blocks, int n_lowest_uncached_lvl=0, string n_name="invalid_name"){
        name = n_name;
        num_of_entries = n_lines;
        num_of_lvls = ceil(log(float(total_num_blocks)) / log(float(KTREE))) + 1;
        lowest_uncached_lvl = n_lowest_uncached_lvl;
        // assert(num_of_entries > 0 && num_of_entries % (A + 1) == 0);
        assert(num_of_lvls > 2);            // we store oram_node [0]: node id; [1]: node lvl for early reshuffle to make the program simpler, so pls have more than 2 level, i.e., more than 2 total number of blocks. 
        rs_vec.reserve(num_of_entries);
        early_reshuffle_nodes.reserve(num_of_lvls);
        for(int i = 0; i < num_of_entries; i++){
            rs_vec.push_back(new rs_line(i, num_of_lvls));
        }
        node_id_vec.resize(num_of_lvls, -1);
        offset_vec.resize(num_of_lvls, -1);
        last_write_line = num_of_entries - 1;
        rs_vec[last_write_line]->type = "init";  // dummy write to start the program. 
        rs_vec[last_write_line]->valid = true;  // dummy write to start the program. 

        for(int i = 0; i < num_of_lvls; i++){
            assert(KTREE > 0);
            if((KTREE & (KTREE - 1)) == 0){     // if KTREE is power of 2
                leaf_division.push_back(((num_of_lvls - i - 1)*tree_move_factor));
                lvl_base_lookup.push_back((long long)(((1<<(i*tree_move_factor)) - 1) / (KTREE - 1)));
            }
            else{
                leaf_division.push_back(pow(KTREE, num_of_lvls - i - 1));
                lvl_base_lookup.push_back((long long)((pow(KTREE, i) - 1) / (KTREE - 1)));
            }
            lvl_counter[i] = 0;
        }

        
        long long valid_leaf_right = (pow(KTREE, (num_of_lvls)) - 1) / (KTREE - 1);
        for(long long i = 0; i < valid_leaf_right; i++){
            ring_bucket_vec->push_back(ring_bucket(i));
        }
        
        stall_reason["normal"] = 0;
        stall_reason["rs_hazard"] = 0;
        stall_reason["mc_hazard"] = 0;
        stall_reason["rw_swtich"] = 0;
    }

    ~rs(){
        for(int i = 0; i < num_of_entries; i++){
            delete rs_vec[i];
        }
    }

    void print_stats(){
        cout << "Stall reason distribution: " << endl;
        for (const auto& kvp : stall_reason) {
            std::cout << kvp.first << ": " << kvp.second << std::endl;
        }
        cout << "Early reshuffle distribution: " << endl;
        for (const auto& kvp : lvl_counter) {
            std::cout << kvp.first << ": " << kvp.second << std::endl;
        }
        cout << "=================" << endl;

        cout << "Max stash size is: " << stash_size_max << endl;
        if(stash_sample_times){
        cout << "Average stash size is: " << stash_size_total * 1.0 / stash_sample_times << endl;
        }
        else{
        cout << "Trace too short. To report stash size average have a longer trace. " << endl;
        }

        long long sum_ddr_accesses = useful_read_pull_ddr + dummy_read_pull_ddr + useful_early_pull_ddr + useful_early_push_ddr + useful_write_pull_ddr
                                + useful_write_push_ddr + dummy_early_pull_ddr + dummy_early_push_ddr
                                + dummy_write_pull_ddr + dummy_write_push_ddr;
        cout << "useful_read_pull_ddr: " << useful_read_pull_ddr << " (" << std::dec << useful_read_pull_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "dummy_read_pull_ddr: " << dummy_read_pull_ddr << " (" << std::dec << dummy_read_pull_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "useful_early_pull_ddr: " << useful_early_pull_ddr << " (" << std::dec << useful_early_pull_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "useful_early_push_ddr: " << useful_early_push_ddr << " (" << std::dec << useful_early_push_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "useful_write_pull_ddr: " << useful_write_pull_ddr << " (" << std::dec << useful_write_pull_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "useful_write_push_ddr: " << useful_write_push_ddr << " (" << std::dec << useful_write_push_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "dummy_early_pull_ddr: " << dummy_early_pull_ddr << " (" << std::dec << dummy_early_pull_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "dummy_early_push_ddr: " << dummy_early_push_ddr << " (" << std::dec << dummy_early_push_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "dummy_write_pull_ddr: " << dummy_write_pull_ddr << " (" << std::dec << dummy_write_pull_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "dummy_write_push_ddr: " << dummy_write_push_ddr << " (" << std::dec << dummy_write_push_ddr * 100.0 / sum_ddr_accesses << ") %" << endl;
        cout << "================="<< endl;
        cout << "Stash violation number of times: " << stash_violation<< endl;
        cout << "================="<< endl;
        
        cout << "Check NodeID traffic: " << check_nodeid_ddr << endl;
        cout << "Update NodeID traffic: " << update_nodeid_ddr << endl;

        cout << "================="<< endl;

        long long sum_rs_lines = rs_read_rs_lines + rs_write_rs_lines + rs_early_rs_lines;
        cout << "Read RS lines: " << rs_read_rs_lines << " (" << std::dec << rs_read_rs_lines * 100.0 / sum_rs_lines << ") %" << endl;
        cout << "Write RS lines: " << rs_write_rs_lines << " (" << std::dec << rs_write_rs_lines * 100.0 / sum_rs_lines << ") %" << endl;
        cout << "Early RS lines: " << rs_early_rs_lines << " (" << std::dec << rs_early_rs_lines * 100.0 / sum_rs_lines << ") %" << endl;

        long long sum_ddr_lines = rs_read_ddr_lines + rs_write_ddr_lines + rs_early_ddr_lines;
        cout << "Read DDR lines: " << rs_read_ddr_lines << " (" << std::dec << rs_read_ddr_lines * 100.0 / sum_ddr_lines << ") %" << endl;
        cout << "Write DDR lines: " << rs_write_ddr_lines << " (" << std::dec << rs_write_ddr_lines * 100.0 / sum_ddr_lines << ") %" << endl;
        cout << "Early DDR lines: " << rs_early_ddr_lines << " (" << std::dec << rs_early_ddr_lines * 100.0 / sum_ddr_lines << ") %" << endl;

    }

    void reset_stats(){
        useful_read_pull_ddr = 0;
        dummy_read_pull_ddr = 0;
        useful_early_pull_ddr = 0;
        useful_early_push_ddr = 0;
        useful_write_pull_ddr = 0;
        useful_write_push_ddr = 0;
        dummy_early_pull_ddr = 0;
        dummy_early_push_ddr = 0;
        dummy_write_pull_ddr = 0;
        dummy_write_push_ddr = 0;
        stash_violation = 0;

        check_nodeid_ddr = 0;
        check_nodeid_ddr = 0;
    }

    long long posmap_nodeid_access(long long node_id){
        if(name == "data"){
            if(metadata_cache->cache_access(node_id * 4 / 64 * 64 + NODEID_METADATA_START)){
                return -1;
            }
            return node_id * 2 / 64 * 64 + NODEID_METADATA_START;
        }
        if(name == "pos1"){
            if(metadata_cache->cache_access(node_id * 4 / 64 * 64 + POS1_METADATA_START)){
                return -1;
            }
            return node_id * 2 / 64 * 64 + POS1_METADATA_START;
        }
        if(name == "pos2"){
            if(metadata_cache->cache_access(node_id * 4 / 64 * 64 + POS2_METADATA_START)){
                return -1;
            }
            return node_id * 2 / 64 * 64 + POS2_METADATA_START;
        }
        assert(0);
        return -1;
    }

    long long P(long long leaf, int level, int max_num_levels) {
        /*
        * This function should be deterministic. 
        * INPUT: leaf in range 0 to num_leaves - 1, level in range 0 to num_levels - 1. 
        * OUTPUT: Returns the location in the storage of the bucket which is at the input level and leaf.
        */
        // return (long long)((1<<level) - 1 + (leaf >> (max_num_levels - level - 1)));
        
        if((KTREE & (KTREE - 1)) == 0){
            return (lvl_base_lookup[level] + (leaf >> leaf_division[level] ));
        }
        return (lvl_base_lookup[level] + leaf / leaf_division[level]);
        // return (lvl_base_lookup[level] + (leaf >> ((max_num_levels - level - 1)*tree_move_factor)));
        // return (long long)(((1<<(level*tree_move_factor)) - 1) / (KTREE - 1) + (leaf >> ((max_num_levels - level - 1)*tree_move_factor)));
        // return (long long)(((pow(KTREE, level) - 1) / (KTREE - 1)) + leaf / (pow(KTREE, max_num_levels - level - 1)));
    }

    void oram_packet_construct(vector<ramulator_packet> &ramualtor_input_vec, long long address, bool pull){
        if(name == "data"){
            for(int i = 0; i < EMBED_REPEAT; i++){
                ramualtor_input_vec.push_back(ramulator_packet(address * EMBED_REPEAT * 64 + i * 64 + DATA_TREE_START, pull, false, name));
            }
        }
        else if (name == "pos1"){
            ramualtor_input_vec.push_back(ramulator_packet(address * 64 + POS1_TREE_START, pull, false, name));
        }
        else if (name == "pos2"){
            ramualtor_input_vec.push_back(ramulator_packet(address * 64 + POS2_TREE_START, pull, false, name));
        }
        else{
            assert(0);
        }
        return;
    }

    long long id_convert_to_address(long long node_id, int offset){
        long long block_id = node_id * (Z + S) + offset;
        return (long long)(block_id);
    }

    bool mshr_hazard(){
        return mshr_array.size() >= 64;
    }

    void mshr_print(){
        cout << "Begin print MSHR" << endl;
        for(auto it = mshr_array.begin(); it != mshr_array.end(); it++){
            cout << std::hex << it->first << std::dec << ": ";
            for(int i = 0; i < it->second.first.size(); i++){
                it->second.first[i].mshr_print();
            }
            cout << endl;
        }
        cout << "End print MSHR" << endl;
    }

    void print_allline(){
        cout << "Begin print RS" << endl;
        cout << "L# : Valid | Type | Write | LastWrite | Orig Address  ||  NodeID, Off, Addr, Finished | .... ||  Ready? " << endl;
        for(int i = 0; i < num_of_entries; i++){
            rs_vec[i]->printline();
        }
        cout << "End print RS" << endl;
    }

    void mshr_add(long long address, int rs_row_num, int rs_offset, bool pull, string type){
        // cout << "MSHR adding rs row num: " << rs_row_num << " with type: " << type << endl;
        if(type == "read"){
            assert(pull);
            assert(rs_vec[rs_row_num]->oram_finished_vec_for_read[rs_offset] == false);
            rs_read_ddr_lines++;
        }
        else if(type == "write"){
            if(pull){
                assert(rs_vec[rs_row_num]->oram_finished_vec_for_pull[rs_offset].first == false);
            }
            else{
                // assert(rs_vec[rs_row_num]->oram_finished_vec_for_write[rs_offset] == false);
            }
            
            rs_write_ddr_lines++;
        }
        else if(type == "early"){
            if(pull){
                assert(rs_vec[rs_row_num]->oram_finished_vec_for_early_pull[rs_offset].first == false);
            }
            else{
                // assert(rs_vec[rs_row_num]->oram_finished_vec_for_early_write[rs_offset] == false);
            }
            rs_early_ddr_lines++;
        }

        if(pull == false){          // MSHR does not wait for write op
            oram_packet_construct(ramualtor_input_vec, address, pull);
            return;
        }
        
        address = (long long)((long)address);
        if(mshr_array.find(address) == mshr_array.end()){
            if(name == "data"){
                mshr_array[address] = make_pair(vector<mshr_entry>(), EMBED_REPEAT);
            }
            else{
                mshr_array[address] = make_pair(vector<mshr_entry>(), 1);
            }
            oram_packet_construct(ramualtor_input_vec, address, pull);
            // cout << "Outstanding request register: " << std::hex << address << std::dec << endl;
        }
        for(int i = 0; i < mshr_array[address].first.size(); i++){
            // TODO: in the future can remove this expensive assertion
            assert(!(mshr_array[address].first)[i].is_equal(mshr_entry(rs_row_num, rs_offset, pull, type)));
        }
        mshr_array[address].first.push_back(mshr_entry(rs_row_num, rs_offset, pull, type));
    }

    void mshr_remove(long long address){
        if(name == "data"){
            address -= DATA_TREE_START;
            int offset = (address / 64) % EMBED_REPEAT;
            address = (address - offset * 64) / (64 * EMBED_REPEAT);
        }
        else if(name == "pos1"){
            address -= POS1_TREE_START;
            address = address / 64;
        }
        else if(name == "pos2"){
            address -= POS2_TREE_START;
            address = address / 64;
        }
        else{
            assert(0);
        }
        assert(mshr_array.find(address) != mshr_array.end());
        assert(mshr_array[address].second > 0);
        mshr_array[address].second--;
        if(mshr_array[address].second > 0){
            return;
        }
        for(int i = 0; i < mshr_array[address].first.size(); i++){
            mshr_entry entry = (mshr_array[address].first)[i];
            string type = entry.type;
            bool pull = entry.pull;
            int rs_row_num = entry.rs_row_num;
            int rs_offset = entry.rs_offset;
            if(type == "read"){
                assert(pull);
                assert(rs_vec[rs_row_num]->oram_finished_vec_for_read[rs_offset] == false);
                rs_vec[rs_row_num]->oram_finished_vec_for_read[rs_offset] = true;
            }
            else if(type == "write"){
                if(pull){
                    assert(rs_vec[rs_row_num]->oram_finished_vec_for_pull[rs_offset].first == false);
                    rs_vec[rs_row_num]->oram_finished_vec_for_pull[rs_offset] = make_pair(true, "unknown");
                }
                else{
                    assert(rs_vec[rs_row_num]->oram_finished_vec_for_write[rs_offset] == false);
                    rs_vec[rs_row_num]->oram_finished_vec_for_write[rs_offset] = true;
                }
            }
            else if(type == "early"){
                if(pull){
                    assert(rs_vec[rs_row_num]->oram_finished_vec_for_early_pull[rs_offset].first == false);
                    rs_vec[rs_row_num]->oram_finished_vec_for_early_pull[rs_offset] = make_pair(true, "unknown");
                }
                else{
                    assert(rs_vec[rs_row_num]->oram_finished_vec_for_early_write[rs_offset] == false);
                    rs_vec[rs_row_num]->oram_finished_vec_for_early_write[rs_offset] = true;
                }
            }
            if(rs_vec[rs_row_num]->update_ready_bit() && rs_row_num == head){
                // cout << "Checking row number readiness: " << rs_row_num << endl;
                while(true){
                    if(rs_line_commit_attempt() == false){
                        break;
                    }
                }
            }
        }
        mshr_array.erase(address);
    }

    bool is_node_on_block_path(long long node_id, int node_lvl, long long block_id){
        return (P(block_id, node_lvl, num_of_lvls) == node_id);
        // return ((P(block_id, num_of_lvls - 1, num_of_lvls) + 1) >> (num_of_lvls - node_lvl - 1) == (node_id + 1));
    }

    void convert_to_ring_path_read(long long block_id, long long intended_node, int intended_offset, long long masking_writeback_block_id){
        // node_id_vec.resize(num_of_lvls, -1);
        // offset_vec.resize(num_of_lvls, -1);
        if(intended_node != -1){
            // cout << "Repeated block detected" << endl;
        }
        for(int i = 0; i < num_of_lvls; i++){
            long long node_id = P(block_id, i, num_of_lvls);
            node_id_vec[i] = node_id;
            std::pair<int, bool> offset_lookup;
            if(intended_node == node_id){
                // cout << "Read node id: " << node_id << " offset: " << intended_offset <<  endl;
                offset_lookup = (*ring_bucket_vec)[node_id].take_z(intended_offset);
                if(i >= lowest_uncached_lvl){
                    useful_read_pull_ddr++;
                }
            }
            else{
                offset_lookup = (*ring_bucket_vec)[node_id].next_s();
                if(i >= lowest_uncached_lvl){
                    dummy_read_pull_ddr++;
                }
            }
            offset_vec[i] = offset_lookup.first;

            assert(node_id >= 0);
            long long metadata_addr = posmap_nodeid_access(node_id);
            if(metadata_addr >= 0){
                check_nodeid_ddr++;
                ramualtor_input_vec.push_back(ramulator_packet(metadata_addr, true, true, name));
            }

            if(offset_lookup.second){
                if((masking_writeback_block_id < 0) || is_node_on_block_path(node_id, i, masking_writeback_block_id) == false){
                    // cout << "Node ID: " << node_id << " lvl: " << i << " masking_writeback_block_id: " << masking_writeback_block_id << " returns:" << is_node_on_block_path(node_id, i, masking_writeback_block_id) << endl;
                    early_reshuffle_nodes.push_back(make_pair(node_id, i));
                }
            }
        }
    }

    void convert_to_ring_path_write(long long block_id){
        // node_id_vec.resize(num_of_lvls, -1);
        for(int i = 0; i < num_of_lvls; i++){
            long long node_id = P(block_id, i, num_of_lvls);
            node_id_vec[i] = node_id;
        }
    }

    bool struct_hazard(int lookahead = 0){
        for(int i = lookahead - 1; i >= 0; i--){
            if(rs_vec[(tail + i) % num_of_entries]->valid == true && rs_vec[(tail + i) % num_of_entries]->type != "init"){
                return true;
            }
        }
        return false;
    }

    void privacy_hazard_check(int line_num){
        if(rs_vec[line_num]->is_write)  return;
        for(int i = (rs_vec[line_num]->leading_write_line_num + 1) %  num_of_entries; i != line_num; i = (i + 1) % num_of_entries){
            // cout << "Line number: " << line_num << endl;
            // cout << "rs_vec[i]->valid " << rs_vec[i]->valid << endl;
            // cout << "rs_vec[i]->ready " << rs_vec[i]->ready << endl;
            assert(rs_vec[i]->valid && rs_vec[i]->ready);
            if(rs_vec[i]->orig_addr == rs_vec[line_num]->orig_addr){
                rs_vec[line_num]->result = rs_vec[i]->result;
            }
        }
    }
    
    bool rs_line_commit_attempt(){
        // cout << "rs_line_commit_attempt @ head " << head << endl;
        // cout << "Now tail is @ tail " << tail << endl;
        if(rs_vec[head]->valid == false || rs_vec[head]->ready == false){
            return false;
        }
        // privacy_hazard_check(head);
        // if(rs_vec[head]->is_write){
        //     int region_lead = rs_vec[head]->leading_write_line_num;
        //     assert(region_lead >= 0);
        //     for(int i = region_lead; i != head; i = (i + 1) % num_of_entries){
        //         rs_vec[i]->valid = false;           // erase region by region
        //     }
        // }
        rs_vec[head]->valid = false;
        head = (head + 1) % num_of_entries;
        // cout << "Head moving to " << head << endl;
        return true;
    }

    void ld_fwd_st_check(int line_number, long long read_node_id, bool is_pull=false, bool is_early=false){
        if(type != "oram"){
            return;
        }
        // check the immediate above write and forward, that is it.
        // cout << "Leading write line number: " << last_write_line << endl;
        for(int i = 0; i < lowest_uncached_lvl; i++){
            if(is_pull){
                for(int j = 0; j < Z + S; j++){
                    rs_vec[line_number]->oram_finished_vec_for_pull[i * (Z + S) + j] = make_pair(true, "unkown");
                }
            }
            else{
                rs_vec[line_number]->oram_finished_vec_for_read[i] = true;
            }
        }
        if(rs_vec[last_write_line]->type == "init" || rs_vec[last_write_line]->valid == false){
            return;
        }
        if(rs_vec[last_write_line]->oram_node_id.size() == 0){
            assert(0);
            return; 
        }
        long long write_leaf_id = rs_vec[last_write_line]->oram_node_id.back();
        if(write_leaf_id < 0){
            long long early_node = rs_vec[last_write_line]->oram_node_id[0];
            long long early_node_lvl = rs_vec[last_write_line]->oram_node_id[1];
            if(early_node_lvl < lowest_uncached_lvl){
                return;
            }
            long long valid_leaf_left = lvl_base_lookup[num_of_lvls - 1];
            bool is_same = (P(read_node_id - valid_leaf_left, early_node_lvl, num_of_lvls) == early_node);
            if(is_same){
                // cout << "This is same @ lvl " << i << endl;
                if(is_pull){
                    for(int j = 0; j < Z + S; j++){
                        rs_vec[line_number]->oram_finished_vec_for_pull[early_node_lvl * (Z + S) + j] = make_pair(true, "unkown");
                    }
                }
                else{
                    rs_vec[line_number]->oram_finished_vec_for_read[early_node_lvl] = true;
                }
            }
            return;
        }
        if(is_early){           // if early pull, then your last write should not have the same path with you, otherwise why are you early pulling? (This is possible by now when S < A. Let it proceed without assert 0) 
            assert(is_pull);
            // cout << "Read leaf id: " << read_node_id << endl;
            // cout << "Write leaf id: " << write_leaf_id << endl;
            // for(int i = 0; i < rs_vec[last_write_line]->oram_node_id.size(); i++){
            //     if(read_node_id == rs_vec[last_write_line]->oram_node_id[i]){
            //         rs_vec[line_number]->valid = true;
            //         print_allline();
            //         assert(0);
            //     }
            // }
            return;
        }
        // long long xor_result = (write_leaf_id + 1) ^ (read_node_id + 1);
        // cout << "read_node_id: " << read_node_id << endl;
        // cout << "Hoisting write_leaf_id: " << write_leaf_id << endl;
        // if(xor_result == 0){
        //     rs_vec[line_number]->ready = true;
        // }
        
        // cout << "Comparing: " << read_node_id << " and " << write_leaf_id << endl;
        for(int i = lowest_uncached_lvl; i < num_of_lvls; i++){
            // long long valid_leaf_left = (pow(KTREE, (num_of_lvls-1)) - 1) / (KTREE - 1);
            long long valid_leaf_left = lvl_base_lookup[num_of_lvls - 1]; // ((1<<((num_of_lvls-1)*tree_move_factor)) - 1) / (KTREE - 1);
            // cout << "valid_leaf_left: " << valid_leaf_left << endl;
            // cout << "write_leaf_id: " << write_leaf_id << endl;
            // bool is_same = xor_result & (0x1 << (num_of_lvls - 1 - i));
            assert(read_node_id >= valid_leaf_left);
            assert(write_leaf_id >= valid_leaf_left);
            bool is_same = (P(read_node_id - valid_leaf_left, i, num_of_lvls) == P(write_leaf_id - valid_leaf_left, i, num_of_lvls));
            // if(is_same == 0){
            if(is_same){
                // cout << "This is same @ lvl " << i << endl;
                if(is_pull){
                    for(int j = 0; j < Z + S; j++){
                        rs_vec[line_number]->oram_finished_vec_for_pull[i * (Z + S) + j] = make_pair(true, "unkown");
                    }
                }
                else{
                    rs_vec[line_number]->oram_finished_vec_for_read[i] = true;
                }
                // cout << "Registered same @ lvl " << i << endl;
            }
            else{
                // cout << "This is not the same @ lvl " << i << endl;
                break;
            }
        }
    }

    void rs_line_register_early_reshuffle_r(long long node_id, int node_lvl){
        assert(rs_vec[tail]->valid == false || rs_vec[tail]->type == "init");
        // cout << "Registering node# " << node_id << " into " << tail << " th line EARLY PULL" << endl;
        // cout << "Line # " << tail << " ";
        rs_vec[tail]->type = "early-pull";
        rs_vec[tail]->orig_addr = 0xdeadbeef;
        for(int j = 0; j < Z + S; j++){
            long long pull_addr = id_convert_to_address(node_id, j);
            rs_vec[tail]->oram_addr_for_early_pull[j] = pull_addr;
            rs_vec[tail]->oram_finished_vec_for_early_pull[j] = make_pair(true, "unknown"); 
            if(addr_to_origaddr.find(pull_addr) != addr_to_origaddr.end()){
                pos_map_line line_info = addr_to_origaddr[pull_addr];
                assert(stash.find(line_info.original_address) == stash.end());
                // cout << "Stash holding: " << std::hex << line_info.original_address << "->" << line_info.block_id << std::dec << endl;
                // cout << "Map removing: " << std::hex << pull_addr << "->" << addr_to_origaddr[pull_addr].original_address << std::dec << endl;
                assert(line_info.original_address >= 0);
                stash[line_info.original_address] = line_info.block_id;
                addr_to_origaddr.erase(pull_addr);
                posMap.erase(line_info.original_address);
            }
        }
        for(int i = 0; i < num_of_lvls; i++){
            if(i == 0){
                rs_vec[tail]->oram_node_id[i] = node_id;
            }
            else if (i == 1){
                rs_vec[tail]->oram_node_id[i] = node_lvl;
            }
            else{
                rs_vec[tail]->oram_node_id[i] = -1;
            }
        }
        vector<int> flushed_offset;
        (*ring_bucket_vec)[node_id].flush_offset(flushed_offset);
        for(int i = 0; i < (*ring_bucket_vec)[node_id].z_arr.size(); i++){
            flushed_offset.push_back((*ring_bucket_vec)[node_id].z_arr[i]);
            rs_vec[tail]->oram_finished_vec_for_early_pull[(*ring_bucket_vec)[node_id].z_arr[i]] = make_pair(false, "useful");
        }
        for(int i = 0; i < (*ring_bucket_vec)[node_id].s_arr.size(); i++){
            flushed_offset.push_back((*ring_bucket_vec)[node_id].s_arr[i]);
            rs_vec[tail]->oram_finished_vec_for_early_pull[(*ring_bucket_vec)[node_id].s_arr[i]] = make_pair(false, "dummy");
        }
        // cout << "flushed_offset size: " << flushed_offset.size() << endl;
        // for(int j = 0; j < flushed_offset.size(); j++){
        //     int fo = flushed_offset[j];
        //     rs_vec[tail]->oram_finished_vec_for_early_pull[fo] = false; 
        // }
        if(node_lvl < lowest_uncached_lvl){
            for(int j = 0; j < Z + S; j++){
                rs_vec[tail]->oram_finished_vec_for_early_pull[j] = make_pair(true, "unknown"); 
            }
        }
        ld_fwd_st_check(tail, node_id, true, true);     
        for(int i = 0; i < rs_vec[tail]->oram_finished_vec_for_early_pull.size(); i++){
            if(rs_vec[tail]->oram_finished_vec_for_early_pull[i].first == false){
                if(rs_vec[tail]->oram_finished_vec_for_early_pull[i].second == "useful"){
                    useful_early_pull_ddr++;
                }
                else{
                    dummy_early_pull_ddr++;
                }
                mshr_add(rs_vec[tail]->oram_addr_for_early_pull[i], tail, i, true, "early");
                // cout << "MSHR early called" << endl;
                rs_vec[tail]->oram_unfinished_cnt++;
            }
        }
        rs_vec[tail]->valid = true;
        rs_vec[tail]->ready = (rs_vec[tail]->oram_unfinished_cnt == 0);
        rs_vec[tail]->is_write = false;
        rs_vec[tail]->leading_write_line_num = last_write_line;
        // rs_vec[tail]->printline(); 
        if(rs_vec[tail]->ready && tail == head){
            rs_line_commit_attempt();
        }
        tail = (tail + 1) % num_of_entries;
        // then writeback
        // if(!(rs_vec[tail]->valid == false || rs_vec[tail]->type == "init")){
        //     print_allline();
        // }
        // assert(rs_vec[tail]->valid == false || rs_vec[tail]->type == "init");
        // // cout << "Registering leaf# " << node_id << " into " << tail << " th line EARLY WRITE" << endl;
        // // cout << "Line # " << tail << " ";
        // rs_vec[tail]->type = "early-push";
        // rs_vec[tail]->orig_addr = 0xdeadbeef;
        // for(int j = 0; j < Z + S; j++){
        //     rs_vec[tail]->oram_addr_for_early_write[j] = id_convert_to_address(node_id, j);
        //     // rs_vec[tail]->oram_finished_vec_for_early_write[j] = false; 
        //     rs_vec[tail]->oram_finished_vec_for_early_write[j] = true; 
        //     if(node_lvl >= lowest_uncached_lvl){
        //         mshr_add(rs_vec[tail]->oram_addr_for_early_write[j], tail, j, false, "early");
        //     }
        // }
        // for(int i = 0; i < num_of_lvls; i++){
        //     if(i == 0){
        //         rs_vec[tail]->oram_node_id[i] = node_id;
        //     }
        //     else if (i == 1){
        //         rs_vec[tail]->oram_node_id[i] = node_lvl;
        //     }
        //     else{
        //         rs_vec[tail]->oram_node_id[i] = -1;
        //     }
        // }        
        // // if(node_lvl < lowest_uncached_lvl){
        // //     for(int j = 0; j < Z + S; j++){
        // //         rs_vec[tail]->oram_finished_vec_for_early_write[j] = true; 
        // //     }
        // // }
        // // for(int i = 0; i < rs_vec[tail]->oram_finished_vec_for_early_write.size(); i++){
        // //     if(rs_vec[tail]->oram_finished_vec_for_early_write[i] == false){
        // //         mshr_add(rs_vec[tail]->oram_addr_for_early_write[i], tail, i, false, "early");
        // //         // cout << "MSHR early called" << endl;
        // //         rs_vec[tail]->oram_unfinished_cnt++;
        // //     }
        // // }
        // rs_vec[tail]->valid = true;
        // rs_vec[tail]->ready = (rs_vec[tail]->oram_unfinished_cnt == 0);
        // rs_vec[tail]->is_write = true;
        // rs_vec[tail]->leading_write_line_num = last_write_line;
        // last_write_line = tail;
        // // rs_vec[tail]->printline();      
        // if(rs_vec[tail]->ready && tail == head){
        //     rs_line_commit_attempt();
        // }   
        // tail = (tail + 1) % num_of_entries;
    }

    void rs_line_register_early_reshuffle_w(long long node_id, int node_lvl){
        // assert(rs_vec[tail]->valid == false || rs_vec[tail]->type == "init");
        // // cout << "Registering node# " << node_id << " into " << tail << " th line EARLY PULL" << endl;
        // // cout << "Line # " << tail << " ";
        // rs_vec[tail]->type = "early-pull";
        // rs_vec[tail]->orig_addr = 0xdeadbeef;
        // for(int j = 0; j < Z + S; j++){
        //     long long pull_addr = id_convert_to_address(node_id, j);
        //     rs_vec[tail]->oram_addr_for_early_pull[j] = pull_addr;
        //     rs_vec[tail]->oram_finished_vec_for_early_pull[j] = make_pair(true, "unknown"); 
        //     if(addr_to_origaddr.find(pull_addr) != addr_to_origaddr.end()){
        //         pos_map_line line_info = addr_to_origaddr[pull_addr];
        //         assert(stash.find(line_info.original_address) == stash.end());
        //         // cout << "Stash holding: " << std::hex << line_info.original_address << "->" << line_info.block_id << std::dec << endl;
        //         // cout << "Map removing: " << std::hex << pull_addr << "->" << addr_to_origaddr[pull_addr].original_address << std::dec << endl;
        //         assert(line_info.original_address >= 0);
        //         stash[line_info.original_address] = line_info.block_id;
        //         addr_to_origaddr.erase(pull_addr);
        //         posMap.erase(line_info.original_address);
        //     }
        // }
        // for(int i = 0; i < num_of_lvls; i++){
        //     if(i == 0){
        //         rs_vec[tail]->oram_node_id[i] = node_id;
        //     }
        //     else if (i == 1){
        //         rs_vec[tail]->oram_node_id[i] = node_lvl;
        //     }
        //     else{
        //         rs_vec[tail]->oram_node_id[i] = -1;
        //     }
        // }
        // vector<int> flushed_offset;
        // (*ring_bucket_vec)[node_id].flush_offset(flushed_offset);
        // for(int i = 0; i < (*ring_bucket_vec)[node_id].z_arr.size(); i++){
        //     flushed_offset.push_back((*ring_bucket_vec)[node_id].z_arr[i]);
        //     rs_vec[tail]->oram_finished_vec_for_early_pull[(*ring_bucket_vec)[node_id].z_arr[i]] = make_pair(false, "useful");
        // }
        // for(int i = 0; i < (*ring_bucket_vec)[node_id].s_arr.size(); i++){
        //     flushed_offset.push_back((*ring_bucket_vec)[node_id].s_arr[i]);
        //     rs_vec[tail]->oram_finished_vec_for_early_pull[(*ring_bucket_vec)[node_id].s_arr[i]] = make_pair(false, "dummy");
        // }
        // // cout << "flushed_offset size: " << flushed_offset.size() << endl;
        // // for(int j = 0; j < flushed_offset.size(); j++){
        // //     int fo = flushed_offset[j];
        // //     rs_vec[tail]->oram_finished_vec_for_early_pull[fo] = false; 
        // // }
        // if(node_lvl < lowest_uncached_lvl){
        //     for(int j = 0; j < Z + S; j++){
        //         rs_vec[tail]->oram_finished_vec_for_early_pull[j] = make_pair(true, "unknown"); 
        //     }
        // }
        // ld_fwd_st_check(tail, node_id, true, true);     
        // for(int i = 0; i < rs_vec[tail]->oram_finished_vec_for_early_pull.size(); i++){
        //     if(rs_vec[tail]->oram_finished_vec_for_early_pull[i].first == false){
        //         if(rs_vec[tail]->oram_finished_vec_for_early_pull[i].second == "useful"){
        //             useful_early_pull_ddr++;
        //         }
        //         else{
        //             dummy_early_pull_ddr++;
        //         }
        //         mshr_add(rs_vec[tail]->oram_addr_for_early_pull[i], tail, i, true, "early");
        //         // cout << "MSHR early called" << endl;
        //         rs_vec[tail]->oram_unfinished_cnt++;
        //     }
        // }
        // rs_vec[tail]->valid = true;
        // rs_vec[tail]->ready = (rs_vec[tail]->oram_unfinished_cnt == 0);
        // rs_vec[tail]->is_write = false;
        // rs_vec[tail]->leading_write_line_num = last_write_line;
        // // rs_vec[tail]->printline(); 
        // if(rs_vec[tail]->ready && tail == head){
        //     rs_line_commit_attempt();
        // }
        // tail = (tail + 1) % num_of_entries;
        // then writeback
        // if(!(rs_vec[tail]->valid == false || rs_vec[tail]->type == "init")){
        //     print_allline();
        // }
        assert(rs_vec[tail]->valid == false || rs_vec[tail]->type == "init");
        // cout << "Registering leaf# " << node_id << " into " << tail << " th line EARLY WRITE" << endl;
        // cout << "Line # " << tail << " ";
        rs_vec[tail]->type = "early-push";
        rs_vec[tail]->orig_addr = 0xdeadbeef;
        for(int j = 0; j < Z + S; j++){
            rs_vec[tail]->oram_addr_for_early_write[j] = id_convert_to_address(node_id, j);
            // rs_vec[tail]->oram_finished_vec_for_early_write[j] = false; 
            rs_vec[tail]->oram_finished_vec_for_early_write[j] = true; 
            if(node_lvl >= lowest_uncached_lvl){
                mshr_add(rs_vec[tail]->oram_addr_for_early_write[j], tail, j, false, "early");
            }
        }
        for(int i = 0; i < num_of_lvls; i++){
            if(i == 0){
                rs_vec[tail]->oram_node_id[i] = node_id;
            }
            else if (i == 1){
                rs_vec[tail]->oram_node_id[i] = node_lvl;
            }
            else{
                rs_vec[tail]->oram_node_id[i] = -1;
            }
        }        
        // if(node_lvl < lowest_uncached_lvl){
        //     for(int j = 0; j < Z + S; j++){
        //         rs_vec[tail]->oram_finished_vec_for_early_write[j] = true; 
        //     }
        // }
        // for(int i = 0; i < rs_vec[tail]->oram_finished_vec_for_early_write.size(); i++){
        //     if(rs_vec[tail]->oram_finished_vec_for_early_write[i] == false){
        //         mshr_add(rs_vec[tail]->oram_addr_for_early_write[i], tail, i, false, "early");
        //         // cout << "MSHR early called" << endl;
        //         rs_vec[tail]->oram_unfinished_cnt++;
        //     }
        // }
        rs_vec[tail]->valid = true;
        rs_vec[tail]->ready = (rs_vec[tail]->oram_unfinished_cnt == 0);
        rs_vec[tail]->is_write = true;
        rs_vec[tail]->leading_write_line_num = last_write_line;
        last_write_line = tail;
        // rs_vec[tail]->printline();      
        if(rs_vec[tail]->ready && tail == head){
            rs_line_commit_attempt();
        }   
        tail = (tail + 1) % num_of_entries;
    }

    void rs_line_register_write(){
        // pull starts with read from the path, and then write back
        assert(rs_vec[tail]->valid == false || rs_vec[tail]->type == "init");
        // cout << "Registering leaf# " << node_id_vec.back() << " into " << tail << " th line PULL" << endl;
        // cout << "Line # " << tail << " ";
        rs_vec[tail]->type = "write-pull";
        rs_vec[tail]->orig_addr = 0xdeadbeef;
        for(int i = 0; i < num_of_lvls; i++){
            long long node_id = node_id_vec[i];
            rs_vec[tail]->oram_node_id[i] = node_id;
            for(int j = 0; j < Z + S; j++){
                long long pull_addr = id_convert_to_address(node_id, j);
                rs_vec[tail]->oram_addr_for_pull[i * (Z + S) + j] = pull_addr;
                rs_vec[tail]->oram_finished_vec_for_pull[i * (Z + S) + j] = make_pair(true, "unkown"); 
                if(addr_to_origaddr.find(pull_addr) != addr_to_origaddr.end()){
                    pos_map_line line_info = addr_to_origaddr[pull_addr];
                    if(stash.find(line_info.original_address) != stash.end()){
                        cout << "Violating addr: " << std::hex << line_info.original_address << std::dec << endl;
                        cout << "Orig -> mapped: " << std::hex << line_info.original_address << "->" << pull_addr << std::dec << endl;
                    }
                    assert(stash.find(line_info.original_address) == stash.end());
                    // cout << "Stash holding: " << std::hex << line_info.original_address << "->" << line_info.block_id << std::dec << endl;
                    // cout << "Map removing: " << std::hex << pull_addr << "->" << addr_to_origaddr[pull_addr].original_address << std::dec << endl;
                    assert(line_info.original_address >= 0);
                    stash[line_info.original_address] = line_info.block_id;
                    addr_to_origaddr.erase(pull_addr);
                    posMap.erase(line_info.original_address);
                }
            }
            for(int j = 0; j < (*ring_bucket_vec)[node_id].z_arr.size(); j++){
                int not_used_offset = (*ring_bucket_vec)[node_id].z_arr[j];
                rs_vec[tail]->oram_addr_for_pull[i * (Z + S) + not_used_offset] = id_convert_to_address(node_id, not_used_offset);
                rs_vec[tail]->oram_finished_vec_for_pull[i * (Z + S) + not_used_offset] = make_pair(false, "useful"); 
            }
            int streamed_Z = (*ring_bucket_vec)[node_id].z_arr.size();
            int remaining_S = Z - streamed_Z;
            assert(remaining_S >= 0 && remaining_S <= (*ring_bucket_vec)[node_id].s_arr.size());
            for(int j = 0; j < remaining_S; j++){
                int not_used_offset = (*ring_bucket_vec)[node_id].s_arr[j];
                rs_vec[tail]->oram_addr_for_pull[i * (Z + S) + not_used_offset] = id_convert_to_address(node_id, not_used_offset);
                rs_vec[tail]->oram_finished_vec_for_pull[i * (Z + S) + not_used_offset] = make_pair(false, "dummy"); 
            }
            for(int j = remaining_S; j < (*ring_bucket_vec)[node_id].s_arr.size(); j++){
                int not_used_offset = (*ring_bucket_vec)[node_id].s_arr[j];
                rs_vec[tail]->oram_addr_for_pull[i * (Z + S) + not_used_offset] = id_convert_to_address(node_id, not_used_offset);
                rs_vec[tail]->oram_finished_vec_for_pull[i * (Z + S) + not_used_offset] = make_pair(true, "unknown"); 
            }
        }
        ld_fwd_st_check(tail, node_id_vec.back(), true);     
        for(int i = 0; i < rs_vec[tail]->oram_finished_vec_for_pull.size(); i++){
            if(rs_vec[tail]->oram_finished_vec_for_pull[i].first == false){
                if(rs_vec[tail]->oram_finished_vec_for_pull[i].second == "useful"){
                    useful_write_pull_ddr++;
                }
                else{
                    dummy_write_pull_ddr++;
                }
                mshr_add(rs_vec[tail]->oram_addr_for_pull[i], tail, i, true, "write");
                rs_vec[tail]->oram_unfinished_cnt++;
            }
        }
        rs_vec[tail]->valid = true;
        rs_vec[tail]->ready = (rs_vec[tail]->oram_unfinished_cnt == 0);
        rs_vec[tail]->is_write = false;
        rs_vec[tail]->leading_write_line_num = last_write_line;
        // rs_vec[tail]->printline(); 
        if(rs_vec[tail]->ready && tail == head){
            rs_line_commit_attempt();
        }
        tail = (tail + 1) % num_of_entries;
        // then writeback
        assert(rs_vec[tail]->valid == false || rs_vec[tail]->type == "init");
        // cout << "Registering leaf# " << node_id_vec.back() << " into " << tail << " th line WRITE" << endl;
        // cout << "Line # " << tail << " ";
        rs_vec[tail]->type = "write-push";
        rs_vec[tail]->orig_addr = 0xdeadbeef;
        for(int i = 0; i < lowest_uncached_lvl; i++){
            long long node_id = node_id_vec[i];
            rs_vec[tail]->oram_node_id[i] = node_id;
            for(int j = 0; j < Z + S; j++){
                long long converted_addr = id_convert_to_address(node_id, j);
                rs_vec[tail]->oram_addr_for_write[i * (Z + S) + j] = converted_addr;
                rs_vec[tail]->oram_finished_vec_for_write[i * (Z + S) + j] = true; 
            }
        }
        for(int i = lowest_uncached_lvl; i < num_of_lvls; i++){
            long long node_id = node_id_vec[i];
            rs_vec[tail]->oram_node_id[i] = node_id;
            for(int j = 0; j < Z + S; j++){
                long long converted_addr = id_convert_to_address(node_id, j);
                rs_vec[tail]->oram_addr_for_write[i * (Z + S) + j] = converted_addr;
                // rs_vec[tail]->oram_finished_vec_for_write[i * (Z + S) + j] = false; 
                rs_vec[tail]->oram_finished_vec_for_write[i * (Z + S) + j] = true; 
                mshr_add(converted_addr, tail, i * (Z + S) + j, false, "write");
                // rs_vec[tail]->oram_unfinished_cnt++;
            }
        }
        rs_vec[tail]->valid = true;
        rs_vec[tail]->ready = (rs_vec[tail]->oram_unfinished_cnt == 0);
        rs_vec[tail]->is_write = true;
        rs_vec[tail]->leading_write_line_num = last_write_line;
        last_write_line = tail;
        // rs_vec[tail]->printline();      
        if(rs_vec[tail]->ready && tail == head){
            rs_line_commit_attempt();
        }  
        tail = (tail + 1) % num_of_entries;
    }


    void rs_line_register_read(long long orig_addr, long long intended_node, int intended_offset){
        assert(rs_vec[tail]->valid == false || rs_vec[tail]->type == "init");
        // cout << "Registering leaf# " << node_id_vec.back() << " into " << tail << " th line READ" << endl;
        // cout << "Line # " << tail << " ";
        rs_vec[tail]->type = "read-pull";
        rs_vec[tail]->orig_addr = orig_addr;
        for(int i = 0; i < num_of_lvls; i++){
            long long extract_node_id = node_id_vec[i];
            int extract_offset = offset_vec[i];
            rs_vec[tail]->oram_node_id[i] = extract_node_id;
            rs_vec[tail]->bucket_offset[i] = extract_offset;
            rs_vec[tail]->oram_lvl_addr[i] = id_convert_to_address(extract_node_id, extract_offset);
            rs_vec[tail]->oram_finished_vec_for_read[i] = false;      
        }
        ld_fwd_st_check(tail, node_id_vec.back());     
        // cout << "After ld st check of " << std::hex << orig_addr << std::dec << endl;
        for(int i = 0; i < rs_vec[tail]->oram_finished_vec_for_read.size(); i++){
            // cout << "Loop in here are you false? " << rs_vec[tail]->oram_finished_vec_for_read[i] << endl;
            if(rs_vec[tail]->oram_finished_vec_for_read[i] == false){
                mshr_add(rs_vec[tail]->oram_lvl_addr[i], tail, i, true, "read");
                rs_vec[tail]->oram_unfinished_cnt++;
            }
        }
        rs_vec[tail]->valid = true;
        rs_vec[tail]->ready = (rs_vec[tail]->oram_unfinished_cnt == 0);
        rs_vec[tail]->is_write = false;
        rs_vec[tail]->leading_write_line_num = last_write_line;
        // rs_vec[tail]->printline(); 
        if(rs_vec[tail]->ready && tail == head){
            rs_line_commit_attempt();
        }
        tail = (tail + 1) % num_of_entries;
    }

};


struct task{
    long long original_address = 0xdeadbeef;
    long long block_id = -1;
    long long node_id = -1;
    int offset = -1;
    string type = "";
    void print_task(){
        cout << "Type | orig | block | node | offset " << endl;
        cout << type << std::hex << " | 0x" << original_address << std::dec << " | " << block_id << " | " << node_id << " | " << offset << endl;
    }
};

class posmap_and_stash{
    public:
    long long total_num_blocks;
    int max_num_levels;
    vector<ring_bucket>* ring_bucket_vec;
    long long valid_leaf_left;
    long long valid_leaf_right;
    long long total_num_of_original_addr_space;
    // map<long long, pos_map_line> posMap;        // address -> node, offset
    // map<long long, long long> stash;            // address -> leaf (not assigned to binary tree yet so we do not have offset)

    deque<task> pending_q;
    rs* myrs;
    long long ring_counter = 0;
    long long rw_counter = 0;
    map<int, int> wb_lvl_useful_count; 
    
    posmap_and_stash(long long n_total_num_blocks, rs* n_myrs){
        total_num_blocks = n_total_num_blocks;
        max_num_levels = ceil(log(float(total_num_blocks)) / log(float(KTREE))) + 1;
        myrs = n_myrs;
        ring_bucket_vec = myrs->ring_bucket_vec;
        for(int i = 0; i < max_num_levels; i++){
            wb_lvl_useful_count[i] = 0;
        }
        valid_leaf_left = (pow(KTREE, (max_num_levels-1)) - 1) / (KTREE - 1);
        valid_leaf_right = (pow(KTREE, (max_num_levels)) - 1) / (KTREE - 1);
        total_num_of_original_addr_space = (valid_leaf_right - valid_leaf_left) * Z;
        cout << "Address space: 0 - " << total_num_of_original_addr_space * EMBED_DIM / 1024.0 / 1024.0 / 1024.0 << " GB" << endl;
        posmap_init_writeback();
    }
    
    ~posmap_and_stash(){
        ;
    }

    long long get_ring_counter(bool print=false){
        int bits = ceil(log2(float(total_num_blocks)));
        long long ring_cp = ring_counter;
        long long ret = 0;
        for(int i = bits - 1; i >= 0; i--){
            ret |= (ring_cp & 1) <<i;
            ring_cp>>=1;
        }
        ring_counter++;
        if(print){
            cout << "ring_counter: " << ret << endl;
        }
        return ret;
    }
    
    void printposmap(){
        cout << "Posmap: address -> block, node, offset" << std::endl;
        for(auto it=myrs->posMap.begin(); it!=myrs->posMap.end(); ++it){
            cout << std::hex << it->first << std::dec << " | " << (it->second).block_id << " , " << (it->second).node_id << " , " << (it->second).offset << std::endl;
        }
    }

    void printstash(){
        cout << "Stash: address |-> leaf" << std::endl;
        for(auto it=myrs->stash.begin(); it!=myrs->stash.end(); ++it){
            cout << std::hex << it->first << std::dec << " | " << it->second << std::endl;
        }
    }

    bool push_to_pending_read(long long access_addr, bool print=false){
        // cout << "Access addr: " << std::hex << access_addr << std::endl;
        if(access_addr > 0){
            access_addr = access_addr % total_num_of_original_addr_space;
        }
        task to_push;
        to_push.original_address = access_addr;
        to_push.type = "read";
        // cout << "Issuing read " << std::hex << access_addr << std::dec << std::endl;
        // cout << "not in posmap? " << (posMap.find(access_addr) == posMap.end()) << std::endl;
        // if(posMap.find(access_addr) != posMap.end()){
        //     cout << "address pending? " << posMap[access_addr].pending << std::endl;
        // }
        if(myrs->posMap.find(access_addr) == myrs->posMap.end() || myrs->posMap[access_addr].pending){
            long long random_block = rand() % total_num_blocks;
            to_push.block_id = random_block;
            if(myrs->struct_hazard(evaluate_next_task(to_push.block_id))){
                // cout << "Hazard detected. " << endl;
                return false;
            }
            to_push.node_id = -1;
            to_push.offset = -1;
        }
        else{
            pos_map_line line = myrs->posMap[access_addr];
            to_push.block_id = line.block_id;
            if(myrs->struct_hazard(evaluate_next_task(to_push.block_id))){
                // cout << "Hazard detected. " << endl;
                return false;
            }
            to_push.node_id = line.node_id;
            to_push.offset = line.offset;
            myrs->posMap[access_addr].pending = true;
        }
        if(print){
            to_push.print_task();
        }

        // if(myrs->struct_hazard(evaluate_next_task(to_push.block_id))){
        //     // cout << "Hazard detected. " << endl;
        //     return false;
        // }

        pending_q.push_back(to_push);
        return true;
    }

    int evaluate_next_task(long long block_id){
        int prospective_lines_added = 1;
        if((rw_counter + 2) % (A + 1) == 0){
            prospective_lines_added+=2;
        }
        
        for(int i = 0; i < max_num_levels; i++){
            long long node_id = myrs->P(block_id, i, max_num_levels);
            if((*ring_bucket_vec)[node_id].accessed_times == S - 1){
                prospective_lines_added+=2;
            }
        }
        // if(prospective_lines_added >= 20){
        //     cout << "Prospective lines added: " << prospective_lines_added << endl;
        // }
        return prospective_lines_added;
    }

    int issue_pending_task(long long access_addr, bool print_flag=false){          // is there a stash writeback? 0: nothing. 1: early 2: write
        // in the future, hook it to the ramualtor request generation
        // myrs->mshr_print();
        // myrs->print_allline();
        // printstash();
        // printposmap();
        if(push_to_pending_read(access_addr, print_flag) == false){
            return 0;
        }

        // cout << "Working on " << std::hex << access_addr << std::dec << endl;

        myrs->ramualtor_input_vec.clear();        
        int ret_val = 1;
        task head;
        if(pending_q.size() == 0){
            long long random_block = rand() % total_num_blocks;
            head.block_id = random_block;
            string type = "read";
        }
        else{
            head = pending_q.front();
            pending_q.pop_front();
        }
        myrs->rs_read_rs_lines++;

        assert(head.type == "read");
        rw_counter++;
        long long wb_block_id = -1;
        if((rw_counter + 1) % (A + 1) == 0){
            wb_block_id = get_ring_counter();
        }
        // cout << "Reading from a path: " << head.block_id << " head node id: " << head.node_id << " offset: " << head.offset << " wb block id: " << wb_block_id << endl;
        myrs->convert_to_ring_path_read(head.block_id, head.node_id, head.offset, wb_block_id);
        myrs->rs_line_register_read(head.original_address, head.node_id, head.offset);
        
        
        long long intended_addr = myrs->id_convert_to_address(head.node_id, head.offset);
        // cout << "Intended address: " << std::hex << intended_addr << std::dec << " with head node id: " << head.node_id << " and offset id: " << head.offset << endl;
        if(myrs->addr_to_origaddr.find(intended_addr) != myrs->addr_to_origaddr.end()){
            // cout << "Map removing #1: " << std::hex << intended_addr << "->" << myrs->addr_to_origaddr[intended_addr].original_address << std::dec << endl;
            if(myrs->addr_to_origaddr[intended_addr].original_address != head.original_address){
                cout << "Violating intended addr: " << std::hex << intended_addr << endl;
                cout << "Violating myrs->addr_to_origaddr[intended_addr].original_address: " << std::hex << myrs->addr_to_origaddr[intended_addr].original_address << endl;
                cout << "Violating head.original_address: " << std::hex << head.original_address << endl;
            }
            assert(myrs->addr_to_origaddr[intended_addr].original_address == head.original_address);
            myrs->addr_to_origaddr.erase(intended_addr);
            int erased_num = myrs->posMap.erase(head.original_address);
            if(erased_num == 0){
                assert(head.original_address == -1);
            }
        }
        
        if(head.original_address >= 0){
            myrs->stash[head.original_address] = rand() % total_num_blocks;
        }
        // cout << "Stash creating: " << std::hex << head.original_address << "->" << myrs->stash[head.original_address] << std::dec << endl;

        if((rw_counter + 1) % (A + 1) == 0){
            rw_counter++;
            myrs->convert_to_ring_path_write(wb_block_id);
            // cout << "Trying to wb: " << wb_block_id << endl;
            myrs->rs_line_register_write();
            piggy_writeback(wb_block_id);
            ret_val+=2;
            myrs->rs_write_rs_lines+=2;
        }

        ret_val += (myrs->early_reshuffle_nodes.size() * 2);
        myrs->rs_early_rs_lines+=(myrs->early_reshuffle_nodes.size() * 2);
        if(myrs->early_reshuffle_nodes.size()){
            for(int i = 0; i < myrs->early_reshuffle_nodes.size(); i++){
                long long node_id = myrs->early_reshuffle_nodes[i].first;
                // cout << "Early issue node id: " << node_id << endl;
                int node_lvl = myrs->early_reshuffle_nodes[i].second;
                myrs->rs_line_register_early_reshuffle_r(node_id, node_lvl);
                // myrs->lvl_counter[node_lvl]++;
                // piggy_one(node_id, node_lvl);
            }
            // myrs->early_reshuffle_nodes.clear();
        }
        if(myrs->early_reshuffle_nodes.size()){
            for(int i = 0; i < myrs->early_reshuffle_nodes.size(); i++){
                long long node_id = myrs->early_reshuffle_nodes[i].first;
                // cout << "Early issue node id: " << node_id << endl;
                int node_lvl = myrs->early_reshuffle_nodes[i].second;
                myrs->rs_line_register_early_reshuffle_w(node_id, node_lvl);
                myrs->lvl_counter[node_lvl]++;
                piggy_one(node_id, node_lvl);
            }
            myrs->early_reshuffle_nodes.clear();
        }
        // cout << "Served " << std::hex << access_addr << std::dec << endl;
        // myrs->print_allline();
        return ret_val;
    }
    
    void piggy_one(long long wb_node_id, int wb_node_lvl){
        int curr_stash_size = myrs->stash.size();
        if(curr_stash_size > myrs->stash_size_max){
            myrs->stash_size_max = curr_stash_size;
        }
        myrs->stash_size_total += curr_stash_size;
        myrs->stash_sample_times++;
        (*ring_bucket_vec)[wb_node_id] = ring_bucket(wb_node_id);
        // cout << "Erasing and writing back to node# " << wb_node_id << endl;

        int early_useful_count = 0;

        assert(wb_node_id >= 0);
        long long metadata_addr = myrs->posmap_nodeid_access(wb_node_id);
        if(metadata_addr >= 0){
            myrs->update_nodeid_ddr++;
            myrs->ramualtor_input_vec.push_back(ramulator_packet(metadata_addr, false, true, myrs->name));
        }

        vector<long long> to_erase;
        for(auto it=myrs->stash.begin(); it!=myrs->stash.end(); ++it){
            if(myrs->is_node_on_block_path(wb_node_id, wb_node_lvl, it->second)){
                // std::cout << "Early writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                if((*ring_bucket_vec)[wb_node_id].free_z_index < Z){
                    early_useful_count++;
                    int assigned_offset = (*ring_bucket_vec)[wb_node_id].assign_z_off();
                    // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
                    pos_map_line tmp;
                    tmp.original_address = it->first;
                    tmp.block_id = it->second;
                    tmp.node_id = wb_node_id;
                    tmp.offset = assigned_offset;
                    tmp.pending = false;
                    myrs->posMap[it->first] = tmp;
                    to_erase.push_back(it->first);
                    myrs->addr_to_origaddr[myrs->id_convert_to_address(wb_node_id, assigned_offset)] = tmp;
                    // cout << "Map adding: " << std::hex << myrs->id_convert_to_address(wb_node_id, assigned_offset) << "->" << tmp.original_address << std::dec << endl;
                }
                if((*ring_bucket_vec)[wb_node_id].free_z_index == Z){
                    break;
                }
            }
        }

        for(int i = 0; i < to_erase.size(); i++){
            // cout << "Stash removing: " << std::hex << to_erase[i] << "->" << myrs->stash[to_erase[i]] << std::dec << endl;
            myrs->stash.erase(to_erase[i]);
        }

        if(wb_node_lvl >= myrs->lowest_uncached_lvl){
            myrs->useful_early_push_ddr += early_useful_count;
            myrs->dummy_early_push_ddr += (Z + S - early_useful_count);
        }
    }

    void piggy_writeback(long long block_id){
        int curr_stash_size = myrs->stash.size();
        if(curr_stash_size > myrs->stash_size_max){
            myrs->stash_size_max = curr_stash_size;
        }
        myrs->stash_size_total += curr_stash_size;
        myrs->stash_sample_times++;
        vector<long long> node_id_vec;
        node_id_vec.resize(max_num_levels, -1);
        for(int i = 0; i < max_num_levels; i++){
            long long node_id = myrs->P(block_id, i, max_num_levels);
            node_id_vec[i] = node_id;
        }

        for(int i = max_num_levels - 1; i >= 0; i--){
            long long wb_node_id = node_id_vec[i];
            (*ring_bucket_vec)[wb_node_id] = ring_bucket(wb_node_id);

            assert(wb_node_id >= 0);
            long long metadata_addr = myrs->posmap_nodeid_access(wb_node_id);
            if(metadata_addr >= 0){
                myrs->update_nodeid_ddr++;
                myrs->ramualtor_input_vec.push_back(ramulator_packet(metadata_addr, true, true, myrs->name));
            }

        }

        // cout << "Erasing and writing back to leaf# " << node_id_vec.back() << endl;
        for(int i = 0; i < max_num_levels; i++){
            wb_lvl_useful_count[i] = 0;
        }

        vector<long long> to_erase;
        for(auto it=myrs->stash.begin(); it!=myrs->stash.end(); ++it){
            int deepest_lvl = -1;
            for(int i = 0; i < max_num_levels; i++){
                if(myrs->P(block_id, i, max_num_levels) == myrs->P(it->second, i, max_num_levels)){
                    deepest_lvl = i;
                }
                else{
                    assert(i > 0);
                    break;
                }
            }
            assert(deepest_lvl >= 0);
            // long long leaf = myrs->P(it->second, max_num_levels - 1, max_num_levels);
            // // std::cout << "Checking stash: 0x" << std::hex << it->first << std::dec << " => " << leaf << '\n';
            // long long xor_result = (leaf + 1) ^ (node_id_vec.back() + 1);
            // // cout << "Xor result: " << xor_result << endl;
            // int deepest_lvl = max_num_levels - 1;
            // for(int i = 0; i < max_num_levels; i++){
            //     bool is_same = xor_result & (0x1 << (max_num_levels - 1 - i));
            //     if(is_same == 0){
            //         // cout << "Piggy same @ lvl " << i << endl;
            //     }
            //     else{
            //         deepest_lvl = i - 1;
            //         break;
            //     }
            // }
            // cout << "Match up to " << deepest_lvl << " lvl" << endl;
            for(int i = deepest_lvl; i >= 0; i--){
                long long wb_node_id = node_id_vec[i];
                if((*ring_bucket_vec)[wb_node_id].free_z_index < Z){
                    wb_lvl_useful_count[i]++;
                    int assigned_offset = (*ring_bucket_vec)[wb_node_id].assign_z_off();
                    // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                    // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
                    // cout << "writing back to node: " << wb_node_id << " offset: " << assigned_offset << endl;
                    pos_map_line tmp;
                    tmp.original_address = it->first;
                    tmp.block_id = it->second;
                    tmp.node_id = wb_node_id;
                    tmp.offset = assigned_offset;
                    tmp.pending = false;
                    myrs->posMap[it->first] = tmp;
                    to_erase.push_back(it->first);
                    myrs->addr_to_origaddr[myrs->id_convert_to_address(wb_node_id, assigned_offset)] = tmp;
                    // cout << "Map adding: " << std::hex << myrs->id_convert_to_address(wb_node_id, assigned_offset) << "->" << tmp.original_address << std::dec << endl;
                    break;
                }
            }
        }

        for(int i = 0; i < to_erase.size(); i++){
            // cout << "Stash removing: " << std::hex << to_erase[i] << "->" << myrs->stash[to_erase[i]] << std::dec << endl;
            myrs->stash.erase(to_erase[i]);
        }
        
        for(int i = myrs->lowest_uncached_lvl; i < max_num_levels; i++){
            myrs->useful_write_push_ddr += wb_lvl_useful_count[i];
            myrs->dummy_write_push_ddr += (Z + S - wb_lvl_useful_count[i]);
        }
    }


    void posmap_init_writeback(){
        std::vector<long long> reshuff_addr;
        for(long long i = 0; i < total_num_of_original_addr_space; i++){
            reshuff_addr.push_back(i);
        }
        std::random_shuffle ( reshuff_addr.begin(), reshuff_addr.end() );
        long long iter = 0;
        for(long long wb_node_id = valid_leaf_left; wb_node_id < valid_leaf_right; wb_node_id++){
            while((*ring_bucket_vec)[wb_node_id].free_z_index < Z){
                int assigned_offset = (*ring_bucket_vec)[wb_node_id].assign_z_off();
                // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
                // cout << "writing back to node: " << wb_node_id << " offset: " << assigned_offset << endl;
                pos_map_line tmp;
                tmp.original_address = reshuff_addr[iter++];
                tmp.block_id = wb_node_id - valid_leaf_left;
                tmp.node_id = wb_node_id;
                tmp.offset = assigned_offset;
                tmp.pending = false;
                myrs->posMap[tmp.original_address] = tmp;
                myrs->addr_to_origaddr[myrs->id_convert_to_address(wb_node_id, assigned_offset)] = tmp;
                // cout << "Init Map adding: " << std::hex << myrs->id_convert_to_address(wb_node_id, assigned_offset) << "->" << tmp.original_address << std::dec << endl;
            }
        }
        // assert(iter == total_num_of_original_addr_space / 64);
    }
};

