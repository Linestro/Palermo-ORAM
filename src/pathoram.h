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
#include <functional>
#include <limits.h>
#include <set>
#include <cmath>
#include <queue>
#include <deque>


using namespace std;

struct path_bucket{
    long long node_id;
    int filled_idx = 0;
    path_bucket(long long n_id){
        node_id = n_id;
        filled_idx = 0;
    }
};

class pathoram{
    public:
    int num_cached_lvls = 0;
    int num_of_lvl = 0;
    long long valid_leaf_left = -1;
    long long valid_leaf_right = -1;
    long long total_num_of_original_addr_space = -1;
    long long total_num_blocks = 0;
    int tree_move_factor = log2(KTREE);
    std::vector<long long> lvl_base_lookup;
    std::vector<long long> leaf_division;

    vector<path_bucket>* path_bucket_vec = new vector<path_bucket>();

    pathoram(int n_num_cached_lvls, int n_num_of_lvl){
        num_cached_lvls = n_num_cached_lvls;
        num_of_lvl = n_num_of_lvl;
        valid_leaf_left = (pow(KTREE, (num_of_lvl-1)) - 1) / (KTREE - 1);
        valid_leaf_right = (pow(KTREE, (num_of_lvl)) - 1) / (KTREE - 1);
        total_num_of_original_addr_space = (valid_leaf_right - valid_leaf_left) * Z;
        total_num_blocks = valid_leaf_right - valid_leaf_left;
        for(long long i = 0; i < valid_leaf_right; i++){
            path_bucket_vec->push_back(path_bucket(i));
        }
        for(int i = 0; i < num_of_lvl; i++){
            // assert(KTREE > 0);
            if((KTREE & (KTREE - 1)) == 0){     // if KTREE is power of 2
                leaf_division.push_back(((num_of_lvl - i - 1)*tree_move_factor));
                lvl_base_lookup.push_back((long long)(((1<<(i*tree_move_factor)) - 1) / (KTREE - 1)));
            }
            else{
                leaf_division.push_back(pow(KTREE, num_of_lvl - i - 1));
                lvl_base_lookup.push_back((long long)((pow(KTREE, i) - 1) / (KTREE - 1)));
            }
        }
        init_oram_tree();
    }

    map<long long, pos_map_line> addr_to_origaddr;
    map<long long, pos_map_line> posMap;
    map<long long, long long> stash;

    void printstash(){
        cout << "Stash: address |-> leaf" << std::endl;
        for(auto it=stash.begin(); it!=stash.end(); ++it){
            cout << std::hex << it->first << std::dec << " | " << it->second << std::endl;
        }
    }

    void access_addr(long long addr){
        addr = addr % total_num_of_original_addr_space;
        long long leaf = (posMap.find(addr) == posMap.end()) ? rand() % total_num_blocks : posMap[addr].block_id;
        for(int i = 0; i < num_of_lvl; i++){
            long long read_node = P(leaf, i, num_of_lvl);
            
            for(int j = 0; j < Z; j++){
                long long pull_addr = id_convert_to_address(read_node, j);
                
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
            (*path_bucket_vec)[read_node] = path_bucket(read_node);
        }
        if(addr >= 0){
            stash[addr] = rand() % total_num_blocks;
        }

        // printstash();

        vector<long long> to_erase;
        for(auto it=stash.begin(); it!=stash.end(); ++it){
            int deepest_lvl = -1;
            for(int i = 0; i < num_of_lvl; i++){
                if(P(leaf, i, num_of_lvl) == P(it->second, i, num_of_lvl)){
                    deepest_lvl = i;
                }
                else{
                    // assert(i > 0);
                    break;
                }
            }
            // assert(deepest_lvl >= 0);
            for(int i = deepest_lvl; i >= 0; i--){
                long long wb_node_id = P(leaf, i, num_of_lvl);
                if((*path_bucket_vec)[wb_node_id].filled_idx < Z){
                    int assigned_offset = (*path_bucket_vec)[wb_node_id].filled_idx;
                    (*path_bucket_vec)[wb_node_id].filled_idx++;
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
        }

        for(int i = 0; i < to_erase.size(); i++){
            // cout << "Stash removing: " << std::hex << to_erase[i] << "->" << myrs->stash[to_erase[i]] << std::dec << endl;
            stash.erase(to_erase[i]);
        }

        // printstash();
        // cout << "Stash remain size: " << stash.size() << endl;


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
    long long id_convert_to_address(long long node_id, int offset){
        long long block_id = node_id * (Z) + offset;
        return (long long)(block_id);
    }

    void init_oram_tree(){
        std::vector<long long> reshuff_addr;
        for(long long i = 0; i < total_num_of_original_addr_space; i++){
            reshuff_addr.push_back(i);
        }
        std::random_shuffle ( reshuff_addr.begin(), reshuff_addr.end() );
        long long iter = 0;
        for(long long wb_node_id = valid_leaf_left; wb_node_id < valid_leaf_right; wb_node_id++){
            for(int i = 0; i < Z; i++){
                int assigned_offset = i;
                // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                // // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
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


};



class proram{
    public:
    int num_cached_lvls = 0;
    int num_of_lvl = 0;
    long long valid_leaf_left = -1;
    long long valid_leaf_right = -1;
    long long total_num_of_original_addr_space = -1;
    long long total_num_blocks = 0;
    int tree_move_factor = log2(KTREE);
    std::vector<long long> lvl_base_lookup;
    std::vector<long long> leaf_division;

    int granularity = 8;
    map<long long, long long> prefetch_lead; // addr -> leaf

    vector<path_bucket>* path_bucket_vec = new vector<path_bucket>();

    proram(int n_num_cached_lvls, int n_num_of_lvl){
        num_cached_lvls = n_num_cached_lvls;
        num_of_lvl = n_num_of_lvl;
        valid_leaf_left = (pow(KTREE, (num_of_lvl-1)) - 1) / (KTREE - 1);
        valid_leaf_right = (pow(KTREE, (num_of_lvl)) - 1) / (KTREE - 1);
        total_num_of_original_addr_space = (valid_leaf_right - valid_leaf_left) * Z;
        total_num_blocks = valid_leaf_right - valid_leaf_left;
        for(long long i = 0; i < valid_leaf_right; i++){
            path_bucket_vec->push_back(path_bucket(i));
        }
        for(int i = 0; i < num_of_lvl; i++){
            // assert(KTREE > 0);
            if((KTREE & (KTREE - 1)) == 0){     // if KTREE is power of 2
                leaf_division.push_back(((num_of_lvl - i - 1)*tree_move_factor));
                lvl_base_lookup.push_back((long long)(((1<<(i*tree_move_factor)) - 1) / (KTREE - 1)));
            }
            else{
                leaf_division.push_back(pow(KTREE, num_of_lvl - i - 1));
                lvl_base_lookup.push_back((long long)((pow(KTREE, i) - 1) / (KTREE - 1)));
            }
        }
        init_oram_tree();
    }

    map<long long, pos_map_line> addr_to_origaddr;
    map<long long, pos_map_line> posMap;
    map<long long, long long> stash;

    void printstash(){
        cout << "Stash: address |-> leaf" << std::endl;
        for(auto it=stash.begin(); it!=stash.end(); ++it){
            cout << std::hex << it->first << std::dec << " | " << it->second << std::endl;
        }
    }

    long long access_addr(long long addr){
        addr = addr % total_num_of_original_addr_space;
        long long leaf = (posMap.find(addr) == posMap.end()) ? rand() % total_num_blocks : posMap[addr].block_id;
        for(int i = 0; i < num_of_lvl; i++){
            long long read_node = P(leaf, i, num_of_lvl);
            
            for(int j = 0; j < Z; j++){
                long long pull_addr = id_convert_to_address(read_node, j);
                
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
            (*path_bucket_vec)[read_node] = path_bucket(read_node);
        }

        long long ret = -1;
        if(addr >= 0){
            ret = (addr % granularity == 0) ? rand() % total_num_blocks : prefetch_lead[addr / granularity * granularity];
            stash[addr] = ret;
        }

        // printstash();

        vector<long long> to_erase;
        for(auto it=stash.begin(); it!=stash.end(); ++it){
            int deepest_lvl = -1;
            for(int i = 0; i < num_of_lvl; i++){
                if(P(leaf, i, num_of_lvl) == P(it->second, i, num_of_lvl)){
                    deepest_lvl = i;
                }
                else{
                    // assert(i > 0);
                    break;
                }
            }
            // assert(deepest_lvl >= 0);
            for(int i = deepest_lvl; i >= 0; i--){
                long long wb_node_id = P(leaf, i, num_of_lvl);
                if((*path_bucket_vec)[wb_node_id].filled_idx < Z){
                    int assigned_offset = (*path_bucket_vec)[wb_node_id].filled_idx;
                    (*path_bucket_vec)[wb_node_id].filled_idx++;
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
        }

        for(int i = 0; i < to_erase.size(); i++){
            // cout << "Stash removing: " << std::hex << to_erase[i] << "->" << myrs->stash[to_erase[i]] << std::dec << endl;
            stash.erase(to_erase[i]);
        }

        // printstash();
        // cout << "Stash remain size: " << stash.size() << endl;

        return ret;
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
    long long id_convert_to_address(long long node_id, int offset){
        long long block_id = node_id * (Z) + offset;
        return (long long)(block_id);
    }

    void init_oram_tree(){
        std::vector<long long> reshuff_addr;
        for(long long i = 0; i < total_num_of_original_addr_space; i++){
            reshuff_addr.push_back(i);
        }
        std::random_shuffle ( reshuff_addr.begin(), reshuff_addr.end() );
        long long iter = 0;
        for(long long wb_node_id = valid_leaf_left; wb_node_id < valid_leaf_right; wb_node_id++){
            for(int i = 0; i < Z; i++){
                int assigned_offset = i;
                // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                // // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
                // cout << "writing back to node: " << wb_node_id << " offset: " << assigned_offset << endl;
                pos_map_line tmp;
                if(reshuff_addr[iter] % granularity == 0){
                    prefetch_lead[reshuff_addr[iter]] = wb_node_id - valid_leaf_left;
                }
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


};





class iroram{
    public:
    int num_cached_lvls = 0;
    int num_of_lvl = 0;
    long long valid_leaf_left = -1;
    long long valid_leaf_right = -1;
    long long total_num_of_original_addr_space = -1;
    long long total_num_blocks = 0;
    int tree_move_factor = log2(KTREE);
    std::vector<long long> lvl_base_lookup;
    std::vector<long long> leaf_division;

    vector<path_bucket>* path_bucket_vec = new vector<path_bucket>();

    iroram(int n_num_cached_lvls, int n_num_of_lvl){
        num_cached_lvls = n_num_cached_lvls;
        num_of_lvl = n_num_of_lvl;
        valid_leaf_left = (pow(KTREE, (num_of_lvl-1)) - 1) / (KTREE - 1);
        valid_leaf_right = (pow(KTREE, (num_of_lvl)) - 1) / (KTREE - 1);
        total_num_of_original_addr_space = (valid_leaf_right - valid_leaf_left) * Z;
        total_num_blocks = valid_leaf_right - valid_leaf_left;
        for(long long i = 0; i < valid_leaf_right; i++){
            path_bucket_vec->push_back(path_bucket(i));
        }
        for(int i = 0; i < num_of_lvl; i++){
            // assert(KTREE > 0);
            if((KTREE & (KTREE - 1)) == 0){     // if KTREE is power of 2
                leaf_division.push_back(((num_of_lvl - i - 1)*tree_move_factor));
                lvl_base_lookup.push_back((long long)(((1<<(i*tree_move_factor)) - 1) / (KTREE - 1)));
            }
            else{
                leaf_division.push_back(pow(KTREE, num_of_lvl - i - 1));
                lvl_base_lookup.push_back((long long)((pow(KTREE, i) - 1) / (KTREE - 1)));
            }
        }
        init_oram_tree();
    }

    map<long long, pos_map_line> addr_to_origaddr;
    map<long long, pos_map_line> posMap;
    map<long long, long long> stash;

    void printstash(){
        cout << "Stash: address |-> leaf" << std::endl;
        for(auto it=stash.begin(); it!=stash.end(); ++it){
            cout << std::hex << it->first << std::dec << " | " << it->second << std::endl;
        }
    }

    void access_addr(long long addr){
        addr = addr % total_num_of_original_addr_space;
        long long leaf = (posMap.find(addr) == posMap.end()) ? rand() % total_num_blocks : posMap[addr].block_id;
        for(int i = 0; i < num_of_lvl; i++){
            long long read_node = P(leaf, i, num_of_lvl);

            int buckiet_sz = (i == num_of_lvl - 1) ? Z : (i >= 10 && i <= 15) ? 1 : (i >= 16 && i <= 18) ? 2 : Z;
            for(int j = 0; j < buckiet_sz; j++){
                long long pull_addr = id_convert_to_address(read_node, j);
                
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
            (*path_bucket_vec)[read_node] = path_bucket(read_node);
        }
        if(addr >= 0){
            stash[addr] = rand() % total_num_blocks;
        }

        // printstash();

        vector<long long> to_erase;
        for(auto it=stash.begin(); it!=stash.end(); ++it){
            int deepest_lvl = -1;
            for(int i = 0; i < num_of_lvl; i++){
                if(P(leaf, i, num_of_lvl) == P(it->second, i, num_of_lvl)){
                    deepest_lvl = i;
                }
                else{
                    // assert(i > 0);
                    break;
                }
            }
            // assert(deepest_lvl >= 0);
            for(int i = deepest_lvl; i >= 0; i--){
                long long wb_node_id = P(leaf, i, num_of_lvl);
                int buckiet_sz = (i == num_of_lvl - 1) ? Z : (i >= 10 && i <= 15) ? 1 : (i >= 16 && i <= 18) ? 2 : Z;
                if((*path_bucket_vec)[wb_node_id].filled_idx < buckiet_sz){
                    int assigned_offset = (*path_bucket_vec)[wb_node_id].filled_idx;
                    (*path_bucket_vec)[wb_node_id].filled_idx++;
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
        }

        for(int i = 0; i < to_erase.size(); i++){
            // cout << "Stash removing: " << std::hex << to_erase[i] << "->" << myrs->stash[to_erase[i]] << std::dec << endl;
            stash.erase(to_erase[i]);
        }

        // printstash();
        // cout << "Stash remain size: " << stash.size() << endl;


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
    long long id_convert_to_address(long long node_id, int offset){
        long long block_id = node_id * (Z) + offset;
        return (long long)(block_id);
    }

    void init_oram_tree(){
        std::vector<long long> reshuff_addr;
        for(long long i = 0; i < total_num_of_original_addr_space; i++){
            reshuff_addr.push_back(i);
        }
        std::random_shuffle ( reshuff_addr.begin(), reshuff_addr.end() );
        long long iter = 0;
        for(long long wb_node_id = valid_leaf_left; wb_node_id < valid_leaf_right; wb_node_id++){
            for(int i = 0; i < Z; i++){
                int assigned_offset = i;
                // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                // // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
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


};







class pageoram{
    public:
    int num_cached_lvls = 0;
    int num_of_lvl = 0;
    long long valid_leaf_left = -1;
    long long valid_leaf_right = -1;
    long long total_num_of_original_addr_space = -1;
    long long total_num_blocks = 0;
    int tree_move_factor = log2(KTREE);
    std::vector<long long> lvl_base_lookup;
    std::vector<long long> leaf_division;

    vector<path_bucket>* path_bucket_vec = new vector<path_bucket>();

    pageoram(int n_num_cached_lvls, int n_num_of_lvl){
        num_cached_lvls = n_num_cached_lvls;
        num_of_lvl = n_num_of_lvl;
        valid_leaf_left = (pow(KTREE, (num_of_lvl-1)) - 1) / (KTREE - 1);
        valid_leaf_right = (pow(KTREE, (num_of_lvl)) - 1) / (KTREE - 1);
        total_num_of_original_addr_space = (valid_leaf_right - valid_leaf_left) * (Z - 1);
        total_num_blocks = valid_leaf_right - valid_leaf_left;
        for(long long i = 0; i < valid_leaf_right; i++){
            path_bucket_vec->push_back(path_bucket(i));
        }
        for(int i = 0; i < num_of_lvl; i++){
            // assert(KTREE > 0);
            if((KTREE & (KTREE - 1)) == 0){     // if KTREE is power of 2
                leaf_division.push_back(((num_of_lvl - i - 1)*tree_move_factor));
                lvl_base_lookup.push_back((long long)(((1<<(i*tree_move_factor)) - 1) / (KTREE - 1)));
            }
            else{
                leaf_division.push_back(pow(KTREE, num_of_lvl - i - 1));
                lvl_base_lookup.push_back((long long)((pow(KTREE, i) - 1) / (KTREE - 1)));
            }
        }
        init_oram_tree();
    }

    map<long long, pos_map_line> addr_to_origaddr;
    map<long long, pos_map_line> posMap;
    map<long long, long long> stash;

    void printstash(){
        cout << "Stash: address |-> leaf" << std::endl;
        for(auto it=stash.begin(); it!=stash.end(); ++it){
            cout << std::hex << it->first << std::dec << " | " << it->second << std::endl;
        }
    }

    void access_addr(long long addr){
        addr = addr % total_num_of_original_addr_space;
        long long leaf = (posMap.find(addr) == posMap.end()) ? rand() % total_num_blocks : posMap[addr].block_id;
        for(int i = 0; i < num_of_lvl; i++){
            long long read_node = P(leaf, i, num_of_lvl);
            
            for(int j = 0; j < Z - 1; j++){
                long long pull_addr = id_convert_to_address(read_node, j);
                
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
            (*path_bucket_vec)[read_node] = path_bucket(read_node);
            
            if(i % 2 == 1){            
                long long sibling_node = (read_node % 2 == 0) ? read_node - 1 : read_node + 1;
                for(int j = 0; j < Z - 1; j++){
                    long long pull_addr = id_convert_to_address(sibling_node, j);
                    
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
                (*path_bucket_vec)[sibling_node] = path_bucket(sibling_node);
            }
        }
        if(addr >= 0){
            stash[addr] = rand() % total_num_blocks;
        }

        // printstash();

        vector<long long> to_erase;
        for(auto it=stash.begin(); it!=stash.end(); ++it){
            int deepest_lvl = -1;
            for(int i = 0; i < num_of_lvl; i++){
                if(P(leaf, i, num_of_lvl) == P(it->second, i, num_of_lvl)){
                    deepest_lvl = i;
                }
                else{
                    // assert(i > 0);
                    break;
                }
            }
            // assert(deepest_lvl >= 0);
            for(int i = deepest_lvl + 1; i >= 0; i--){
                if(i == num_of_lvl){
                    continue;
                }
                long long wb_node_id = P(leaf, i, num_of_lvl);
                if(wb_node_id == P(it->second, i, num_of_lvl) && (*path_bucket_vec)[wb_node_id].filled_idx < Z - 1){
                    int assigned_offset = (*path_bucket_vec)[wb_node_id].filled_idx;
                    (*path_bucket_vec)[wb_node_id].filled_idx++;
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

                
                if(i % 2 == 1){            
                    long long sibling_node = (wb_node_id % 2 == 0) ? wb_node_id - 1 : wb_node_id + 1;
                    if(sibling_node == P(it->second, i, num_of_lvl) && (*path_bucket_vec)[sibling_node].filled_idx < Z - 1){
                        int assigned_offset = (*path_bucket_vec)[sibling_node].filled_idx;
                        (*path_bucket_vec)[sibling_node].filled_idx++;
                        // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                        // cout << std::hex << it->first << std::dec << " finally pick node: " << sibling_node << " offset: " << assigned_offset << endl;
                        // cout << "writing back to node: " << sibling_node << " offset: " << assigned_offset << endl;
                        pos_map_line tmp;
                        tmp.original_address = it->first;
                        tmp.block_id = it->second;
                        tmp.node_id = sibling_node;
                        tmp.offset = assigned_offset;
                        tmp.pending = false;
                        posMap[it->first] = tmp;
                        to_erase.push_back(it->first);
                        addr_to_origaddr[id_convert_to_address(sibling_node, assigned_offset)] = tmp;
                        // cout << "Map adding: " << std::hex << myrs->id_convert_to_address(sibling_node, assigned_offset) << "->" << tmp.original_address << std::dec << endl;
                        break;
                    }
                }
            }
        }

        for(int i = 0; i < to_erase.size(); i++){
            // cout << "Stash removing: " << std::hex << to_erase[i] << "->" << myrs->stash[to_erase[i]] << std::dec << endl;
            stash.erase(to_erase[i]);
        }

        // printstash();
        // cout << "Stash remain size: " << stash.size() << endl;

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
    long long id_convert_to_address(long long node_id, int offset){
        long long block_id = node_id * (Z - 1) + offset;
        return (long long)(block_id);
    }

    void init_oram_tree(){
        std::vector<long long> reshuff_addr;
        for(long long i = 0; i < total_num_of_original_addr_space; i++){
            reshuff_addr.push_back(i);
        }
        std::random_shuffle ( reshuff_addr.begin(), reshuff_addr.end() );
        long long iter = 0;
        for(long long wb_node_id = valid_leaf_left; wb_node_id < valid_leaf_right; wb_node_id++){
            for(int i = 0; i < Z - 1; i++){
                int assigned_offset = i;
                // std::cout << "Normal writing back stash: 0x" << std::hex << it->first << std::dec << " => " << it->second << '\n';
                // // cout << std::hex << it->first << std::dec << " finally pick node: " << wb_node_id << " offset: " << assigned_offset << endl;
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


};


