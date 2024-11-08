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
#include <functional>
#include <limits.h>
#include <set>
#include <cmath>
#include <queue>
#include "../src/ring_bucket.h"

// #define PRINT_FLAG

#define KTREE 2
// #define CACHED_NUM_NODE  16384               // 4 MB cache: 4*1024*1024 / 64
#define TOTAL_NUM_TOP_NODE  67108864 //16GB       // 8 GB Mem (16 GB with 50% dummy)
#define STASH_MAITANENCE  32
#define CUT_LVL 14
#define CUTDOWN_LVL 1
#define EVCT_TIMES 1
#define WORKLOAD 10000000
// #define SUBTREE_HEIGHT 8
#define SET_NUM 2

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

struct pos_map_line{
    long long original_address = 0xdeadbeef;
    long long block_id = -1;
    long long node_id = -1;
    int offset = -1;
    bool pending = false;
};

long long total_num_blocks = TOTAL_NUM_TOP_NODE;
int max_num_levels = ceil(log(float(total_num_blocks)) / log(float(KTREE))) + 1;
int num_of_lvls = max_num_levels;
long long valid_leaf_left = (pow(KTREE, (max_num_levels-1)) - 1) / (KTREE - 1);
long long valid_leaf_right = (pow(KTREE, (max_num_levels)) - 1) / (KTREE - 1);
long long total_num_of_original_addr_space = (valid_leaf_right - valid_leaf_left) * Z;

std::vector<long long> lvl_base_lookup;
std::vector<long long> leaf_division;

map<long long, pos_map_line> posMap; 
map<long long, pos_map_line> addr_to_origaddr;
map<long long, long long> stash;  
vector<ring_bucket> ring_bucket_vec; 
vector<pair<long long, int>> early_reshuffle_nodes;

long long ring_counter = 0;
long long rw_counter = 0;

int stash_size_max = 0;
long long stash_size_total = 0;
long long stash_sample_times = 0;
long long stash_violation = 0;


vector<long long> node_id_vec;
vector<int> offset_vec;

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


void printstash(){
    cout << "Stash: address |-> leaf; total of " << stash.size() << std::endl;
    for(auto it=stash.begin(); it!=stash.end(); ++it){
        cout << std::hex << it->first << std::dec << " | " << it->second << std::endl;
    }
}


long long id_convert_to_address(long long node_id, int offset){
    if(node_id < 0){
        return -1;
    }
    long long block_id = node_id * (Z + S) + offset;
    return (long long)(block_id);
}

void print_posmap(){
    cout << posMap.size() << endl;
    // return;
    for(auto it = posMap.cbegin(); it != posMap.cend(); ++it){
        cout << "Posmap[ " << it->first << " ]: mapped to block ID: " << it->second.block_id << " node ID: " << it->second.node_id << " offset: " << it->second.offset << endl;
    }
}

long long P(long long leaf, int level, int max_num_levels) {
    /*
    * This function should be deterministic. 
    * INPUT: leaf in range 0 to num_leaves - 1, level in range 0 to num_levels - 1. 
    * OUTPUT: Returns the location in the storage of the bucket which is at the input level and leaf.
    */
    // return (long long)((1<<level) - 1 + (leaf >> (max_num_levels - level - 1)));
    assert(level < CUT_LVL || level >= (CUT_LVL + CUTDOWN_LVL));
    if((KTREE & (KTREE - 1)) == 0){
        return (lvl_base_lookup[level] + (leaf >> leaf_division[level] ));
    }
    return (lvl_base_lookup[level] + leaf / leaf_division[level]);
}

long long P1(long long leaf, int level, int max_num_levels) {
    /*
    * This function should be deterministic. 
    * INPUT: leaf in range 0 to num_leaves - 1, level in range 0 to num_levels - 1. 
    * OUTPUT: Returns the location in the storage of the bucket which is at the input level and leaf.
    */
    return (long long)((1<<level) - 1 + (leaf >> (max_num_levels - level - 1)));
}
bool is_node_on_block_path(long long node_id, int node_lvl, long long block_id){
    return (P(block_id, node_lvl, num_of_lvls) == node_id);
    // return ((P(block_id, num_of_lvls - 1, num_of_lvls) + 1) >> (num_of_lvls - node_lvl - 1) == (node_id + 1));
}

void posmap_init_writeback(){
    std::vector<long long> reshuff_addr;
    
    long long total_num_blocks = TOTAL_NUM_TOP_NODE;
    int max_num_levels = ceil(log(float(total_num_blocks)) / log(float(KTREE))) + 1;
    long long valid_leaf_left = (pow(KTREE, (max_num_levels-1)) - 1) / (KTREE - 1);
    long long valid_leaf_right = (pow(KTREE, (max_num_levels)) - 1) / (KTREE - 1);
    long long total_num_of_original_addr_space = (valid_leaf_right - valid_leaf_left) * Z;

    for(long long i = 0; i < total_num_of_original_addr_space; i++){
        reshuff_addr.push_back(i);
    }
    std::random_shuffle ( reshuff_addr.begin(), reshuff_addr.end() );

    long long iter = 0;
    for(long long wb_node_id = valid_leaf_left; wb_node_id < valid_leaf_right; wb_node_id++){
        while((ring_bucket_vec)[wb_node_id].free_z_index < Z){
            int assigned_offset = (ring_bucket_vec)[wb_node_id].assign_z_off();
            // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
            // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
            // cout << "writing back to node: " << wb_node_id << " offset: " << assigned_offset << endl;
            pos_map_line tmp;
            tmp.original_address = reshuff_addr[iter++];
            tmp.block_id = wb_node_id - valid_leaf_left;
            tmp.node_id = wb_node_id;
            tmp.offset = assigned_offset;
            tmp.pending = false;
            posMap[tmp.original_address] = tmp;
            addr_to_origaddr[id_convert_to_address(wb_node_id, assigned_offset)] = tmp;
            // cout << "Init Map adding: " << std::hex << myrs->id_convert_to_address(wb_node_id, assigned_offset) << "->" << tmp.original_address << std::dec << endl;
        }
    }
}

void read_pull(long long original_address, long long block_id, long long intended_node, int intended_offset, vector<long long> masking_writeback_block_id){
    if(intended_node != -1){
        // cout << "Repeated block detected" << endl;
    }
    for(int i = 0; i < num_of_lvls; i++){
        if(i < CUT_LVL || i >= (CUT_LVL + CUTDOWN_LVL)){
            long long node_id = P(block_id, i, num_of_lvls);
            node_id_vec[i] = node_id;
            std::pair<int, bool> offset_lookup;
            if(intended_node == node_id){
                offset_lookup = (ring_bucket_vec)[node_id].take_z(intended_offset);
            }
            else{
                offset_lookup = (ring_bucket_vec)[node_id].next_s();
            }
            offset_vec[i] = offset_lookup.first;
            if(offset_lookup.second){
                bool early = true;
                for(int i =0; i < EVCT_TIMES; i ++){
                    if((masking_writeback_block_id[i] >= 0) && is_node_on_block_path(node_id, i, masking_writeback_block_id[i]) == true){
                        early = false;
                        // cout << "Node ID: " << node_id << " lvl: " << i << " masking_writeback_block_id: " << masking_writeback_block_id << " returns:" << is_node_on_block_path(node_id, i, masking_writeback_block_id) << endl;
                    }
                }
                if(early){
                    early_reshuffle_nodes.push_back(make_pair(node_id, i));
                }
            }

        }
        
        // assert(node_id >= 0);

        
    }

    long long intended_addr = id_convert_to_address(intended_node, intended_offset);
    // cout << "Intended address: " << std::hex << intended_addr << std::dec << " with head node id: " << head.node_id << " and offset id: " << head.offset << endl;
    if(addr_to_origaddr.find(intended_addr) != addr_to_origaddr.end()){
        // assert(addr_to_origaddr[intended_addr].original_address == original_address);
        addr_to_origaddr.erase(intended_addr);
        int erased_num = posMap.erase(original_address);
        // assert(erased_num > 0);
        if(erased_num == 0){
            assert(original_address == -1);
        }
    }
    else{
        assert(intended_node == -1);
    }
    if(original_address >= 0){
        stash[original_address] = rand() % total_num_blocks;
    }
}

void early_reshuffle_pull(long long node_id, int node_lvl){
    assert(node_lvl < CUT_LVL || node_lvl >= (CUT_LVL + CUTDOWN_LVL));
    for(int j = 0; j < Z + S; j++){
        long long pull_addr = id_convert_to_address(node_id, j);
        if(addr_to_origaddr.find(pull_addr) != addr_to_origaddr.end()){
            pos_map_line line_info = addr_to_origaddr[pull_addr];
            // assert(stash.find(line_info.original_address) == stash.end());
            // cout << "Stash holding: " << std::hex << line_info.original_address << "->" << line_info.block_id << std::dec << endl;
            // cout << "Map removing: " << std::hex << pull_addr << "->" << addr_to_origaddr[pull_addr].original_address << std::dec << endl;
            // assert(line_info.original_address >= 0);
            stash[line_info.original_address] = line_info.block_id;
            addr_to_origaddr.erase(pull_addr);
            posMap.erase(line_info.original_address);
        }
    }
}

void writeback_pull(long long block_id){
    for(int i = 0; i < num_of_lvls; i++){
        if(i < CUT_LVL || i >= (CUT_LVL + CUTDOWN_LVL)){
        long long node_id = P(block_id, i, num_of_lvls);
        for(int j = 0; j < Z + S; j++){
            long long pull_addr = id_convert_to_address(node_id, j);
            if(addr_to_origaddr.find(pull_addr) != addr_to_origaddr.end()){
                pos_map_line line_info = addr_to_origaddr[pull_addr];
                if(stash.find(line_info.original_address) != stash.end()){
                    cout << "Violating addr: " << std::hex << line_info.original_address << std::dec << endl;
                    cout << "Orig -> mapped: " << std::hex << line_info.original_address << "->" << pull_addr << std::dec << endl;
                }
                // assert(stash.find(line_info.original_address) == stash.end());
                // cout << "Stash holding: " << std::hex << line_info.original_address << "->" << line_info.block_id << std::dec << endl;
                // cout << "Map removing: " << std::hex << pull_addr << "->" << addr_to_origaddr[pull_addr].original_address << std::dec << endl;
                // assert(line_info.original_address >= 0);
                stash[line_info.original_address] = line_info.block_id;
                addr_to_origaddr.erase(pull_addr);
                posMap.erase(line_info.original_address);
            }
        }
    }
    }
}

void piggy_writeback(long long block_id){
    int curr_stash_size = stash.size();
    if(curr_stash_size > stash_size_max){
        stash_size_max = curr_stash_size;
    }
    stash_size_total += curr_stash_size;
    stash_sample_times++;
    
    for(int i = 0; i < max_num_levels; i++){
        if(i < CUT_LVL || i >= (CUT_LVL + CUTDOWN_LVL)){
            long long node_id = P(block_id, i, max_num_levels);
            node_id_vec[i] = node_id;
        }
    }

    for(int i = max_num_levels - 1; i >= 0; i--){
        if(i < CUT_LVL || i >= (CUT_LVL + CUTDOWN_LVL)){
            long long wb_node_id = node_id_vec[i];
            (ring_bucket_vec)[wb_node_id] = ring_bucket(wb_node_id);
        }
    }
    int subtree_height = max_num_levels  - (CUT_LVL + CUTDOWN_LVL);
    vector<long long> to_erase;
    for(auto it=stash.begin(); it!=stash.end(); ++it){
        int deepest_lvl = -1;
        for(int i = 0; i < max_num_levels; i++){
            if(i < CUT_LVL){
                if(P(block_id, i, max_num_levels) == P(it->second, i, max_num_levels)){
                    deepest_lvl = i;
                }
                else{
                    assert(i > 0);
                    break;
                }
            }
            else if(i >= (CUT_LVL + CUTDOWN_LVL)){
                int sub_lvl = i - (CUT_LVL + CUTDOWN_LVL);
                if(((long long)(block_id/(SET_NUM * (long long)pow(2,subtree_height-1))) == (long long)(it->second/(SET_NUM * (long long)pow(2,subtree_height-1)))) && (P1((block_id % (long long)pow(2,max_num_levels - (CUT_LVL + CUTDOWN_LVL) - 1)), sub_lvl, subtree_height) == P1((it->second % (long long)pow(2,max_num_levels - (CUT_LVL + CUTDOWN_LVL) - 1)), sub_lvl, subtree_height))){
                    deepest_lvl = i;
                }
                else{
                    assert(i > 0);
                    break;
                }
            }
            // if(i < CUT_LVL || i >= (CUT_LVL + CUTDOWN_LVL)){
            //     if(P(block_id, i, max_num_levels) == P(it->second, i, max_num_levels)){
            //         deepest_lvl = i;
            //     }
            //     else{
            //         assert(i > 0);
            //         break;
            //     }
            // }
        }
        assert(deepest_lvl >= 0);
        // cout <<"deepest:" << deepest_lvl << endl;
        for(int i = deepest_lvl; i >= 0; i--){
            if(i < CUT_LVL){
                long long wb_node_id = node_id_vec[i];
                if((ring_bucket_vec)[wb_node_id].free_z_index < Z){
                    int assigned_offset = (ring_bucket_vec)[wb_node_id].assign_z_off();
                    // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                    // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
                    // cout << "writing back to node: " << wb_node_id << " offset: " << assigned_offset << endl;
                    pos_map_line tmp;
                    tmp.original_address = it->first;
                    tmp.block_id = it->second;
                    tmp.node_id = wb_node_id;
                    tmp.offset = assigned_offset;
                    tmp.pending = false;
                    posMap[it->first] = tmp;
                    to_erase.push_back(it->first);
                    addr_to_origaddr[id_convert_to_address(wb_node_id, assigned_offset)] = tmp;
                    // cout << "Map adding: " << std::hex << myrs->id_convert_to_address(wb_node_id, assigned_offset) << "->" << tmp.original_address << std::dec << endl;
                    break;
                }
            }
            else if(i >= (CUT_LVL + CUTDOWN_LVL)){
                long long wb_node_id = node_id_vec[i];
                if((ring_bucket_vec)[wb_node_id].free_z_index < Z){
                    int assigned_offset = (ring_bucket_vec)[wb_node_id].assign_z_off();
                    // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                    // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
                    // cout << "writing back to node: " << wb_node_id << " offset: " << assigned_offset << endl;
                    pos_map_line tmp;
                    tmp.original_address = it->first;
                    tmp.block_id =block_id + ((int)(it->second % (long)pow(2,max_num_levels - (CUT_LVL + CUTDOWN_LVL) - 1)) - (int)(block_id % (long)pow(2,max_num_levels - (CUT_LVL + CUTDOWN_LVL) - 1)));
                    tmp.node_id = wb_node_id;
                    tmp.offset = assigned_offset;
                    tmp.pending = false;
                    posMap[it->first] = tmp;
                    to_erase.push_back(it->first);
                    addr_to_origaddr[id_convert_to_address(wb_node_id, assigned_offset)] = tmp;
                    // cout << "Map adding: " << std::hex << myrs->id_convert_to_address(wb_node_id, assigned_offset) << "->" << tmp.original_address << std::dec << endl;
                    break;
                }
            }
        }
    }
    // cout <<"erase size :" << to_erase.size() << endl;
    for(int i = 0; i < to_erase.size(); i++){
        stash.erase(to_erase[i]);
    }
    
}

void piggy_one(long long wb_node_id, int wb_node_lvl){
    assert(wb_node_lvl < CUT_LVL || wb_node_lvl >= (CUT_LVL + CUTDOWN_LVL));
    int curr_stash_size = stash.size();
    if(curr_stash_size > stash_size_max){
        stash_size_max = curr_stash_size;
    }
    stash_size_total += curr_stash_size;
    stash_sample_times++;
    (ring_bucket_vec)[wb_node_id] = ring_bucket(wb_node_id);
    // cout << "Erasing and writing back to node# " << wb_node_id << endl;


    assert(wb_node_id >= 0);

    vector<long long> to_erase;
    for(auto it=stash.begin(); it!=stash.end(); ++it){
        if(is_node_on_block_path(wb_node_id, wb_node_lvl, it->second)){
            // std::cout << "Early writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
            if((ring_bucket_vec)[wb_node_id].free_z_index < Z){
                int assigned_offset = (ring_bucket_vec)[wb_node_id].assign_z_off();
                // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
                pos_map_line tmp;
                tmp.original_address = it->first;
                tmp.block_id = it->second;
                tmp.node_id = wb_node_id;
                tmp.offset = assigned_offset;
                tmp.pending = false;
                posMap[it->first] = tmp;
                to_erase.push_back(it->first);
                addr_to_origaddr[id_convert_to_address(wb_node_id, assigned_offset)] = tmp;
            }
            if((ring_bucket_vec)[wb_node_id].free_z_index == Z){
                break;
            }
        }
    }

    for(int i = 0; i < to_erase.size(); i++){
        stash.erase(to_erase[i]);
    }
}

void print_results(){    
    cout << "Max stash size is: " << stash_size_max << endl;
    
    if(stash_sample_times){
      cout << "Average stash size is: " << stash_size_total * 1.0 / stash_sample_times << endl;
    }
    else{
      cout << "Trace too short. To report stash size average have a longer trace. " << endl;
    }
    cout << "Stash violation number of times: " << stash_violation<< endl;
}

void read_routine(long long access_addr){
    task to_push;
    to_push.original_address = access_addr;
    to_push.type = "read";
    if(posMap.find(access_addr) == posMap.end() || posMap[access_addr].pending){
        long long random_block = rand() % total_num_blocks;
        to_push.block_id = random_block;
        to_push.node_id = -1;
        to_push.offset = -1;
    }
    else{
        pos_map_line line = posMap[access_addr];
        to_push.block_id = line.block_id;
        to_push.node_id = line.node_id;
        to_push.offset = line.offset;
        posMap[access_addr].pending = true;
    }

    rw_counter++;  

    vector <long long> wb_block_id (EVCT_TIMES, -1);
    if((rw_counter) % (A) == 0){
        for(int i =0; i < EVCT_TIMES; i ++){
            wb_block_id[i] = get_ring_counter();
        }
    }
    read_pull(to_push.original_address, to_push.block_id, to_push.node_id, to_push.offset, wb_block_id);
    #ifdef PRINT_FLAG
    cout << "Accessed addr: " << access_addr << " block ID: " << to_push.block_id << " accessed Node ID: " << to_push.node_id << " accessed offset: "  << to_push.offset << endl;
    #endif

    if((rw_counter) % (A) == 0){
        // rw_counter++; 
        for(int i = 0; i< EVCT_TIMES; i ++){
            writeback_pull(wb_block_id[i]); 
            piggy_writeback(wb_block_id[i]);
            #ifdef PRINT_FLAG
            cout << "WB of " << wb_block_id[i] << " completed" << endl;
            #endif
        }
    }

    if(early_reshuffle_nodes.size()){
        for(int i = 0; i < early_reshuffle_nodes.size(); i++){
            long long node_id = early_reshuffle_nodes[i].first;
            // cout << "Early issue node id: " << node_id << endl;
            int node_lvl = early_reshuffle_nodes[i].second;
            early_reshuffle_pull(node_id, node_lvl);
            piggy_one(node_id, node_lvl);
            #ifdef PRINT_FLAG
            cout << "ER of " << node_id << " completed" << endl;
            #endif
        }
        early_reshuffle_nodes.clear();
    }
    
    // printstash();
}

int main(int argc, char** argv)
{
    cout << "Total num of original addr space: " << total_num_of_original_addr_space << endl;
    cout << "This is equivalent to: " << total_num_of_original_addr_space * 64 / 1024.0 / 1024.0 / 1024.0 << " GB " << endl;
    cout << "Security params: Z, S, A: " << Z << ", " << S << ", " << A << endl; 
    node_id_vec.resize(num_of_lvls, -1);
    offset_vec.resize(num_of_lvls, -1);
    assert((CUT_LVL + CUTDOWN_LVL) <= (num_of_lvls -1));
    int tree_move_factor = log2(KTREE);
    for(int i = 0; i < num_of_lvls; i++){
        if((KTREE & (KTREE - 1)) == 0){     // if KTREE is power of 2
            leaf_division.push_back(((num_of_lvls - i - 1)*tree_move_factor));
            lvl_base_lookup.push_back((long long)(((1<<(i*tree_move_factor)) - 1) / (KTREE - 1)));
        }
        else{
            leaf_division.push_back(pow(KTREE, num_of_lvls - i - 1));
            lvl_base_lookup.push_back((long long)((pow(KTREE, i) - 1) / (KTREE - 1)));
        }
    }

    std::ifstream myfile; 
    myfile.open(argv[1]);
    long long cnt = 0;
    std::string mystring, garbage;
    
    for(long long i = 0; i < valid_leaf_right; i++){
        ring_bucket_vec.push_back(ring_bucket(i));
    }
    posmap_init_writeback();
    // print_posmap();
    
    long long interval = 0.001 * WORKLOAD;
    while(cnt < WORKLOAD){
    // while(myfile) {
    //     myfile >> mystring >> garbage;
    //     if(!myfile){
    //         break;
    //     }
        cnt++;
        if(cnt % interval == 0) {
            cout << "Served " << cnt << " inst so far" << " ( " << cnt * 100.0 / WORKLOAD << " %)" << endl;
            print_results();
        }

        // std::stringstream ss;
        // long long access_addr = -1;
        // ss << std::hex << mystring;
        // ss >> access_addr;
        // assert(access_addr >= 0);
        long long access_addr = rand();
        access_addr = access_addr % total_num_of_original_addr_space;
        // std::cout << access_addr << endl;


        read_routine(access_addr);


        while(stash.size() >= STASH_MAITANENCE){
            stash_violation++;
            read_routine(-1);
        }
    }





    // myfile.close();
    
    
    print_results();
    cout << "Program exit normally" << endl;


}