
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
#include "reservation.h"

using namespace std;




class task_2d{
    public:
    long long id;
    string level;
    int next_step;
    long long addr;
    long long* block_id;
    long long* node_id;
    int* offset;
    task* head;
    long long set_id;
    long long set_cnt;
    vector<string> step_string = {"posmap_read", "node_meta_early_pull", "pull", "maintain"};

    task_2d(long long n_id, string n_level, int n_step, long  long n_addr, int num_of_levels, long long n_set_id, long long n_set_cnt){
        id = n_id;
        level = n_level;
        next_step = n_step;
        addr = n_addr;
        head = new task(num_of_levels);
        set_id = n_set_id;
        set_cnt = n_set_cnt;
    }

    void print_task(){
        cout << "This task has ID: " << id << " level: " << level << " step: " << next_step << " addr: " << addr << " set_id: " << set_id << endl;
    }
};

struct task_2dComparator {
    bool operator()(const task_2d* item1, const task_2d* item2) const {
        if (item1->id < item2->id) {
            return false;
        } else if (item1->id > item2->id) {
            return true;
        } else {
            return item1->level < item2->level;
        }
    }
    // bool operator()(const task_2d* item1, const task_2d* item2) const {
    //     if (item1->level > item2->level) {
    //         return false;
    //     } else if (item1->level < item2->level) {
    //         return true;
    //     } else {
    //         return item1->id < item2->id;
    //     }
    // }
};

class m_task_deque{
    public:
    long long current_rdy_to_commit = 0;
    int depth = 8;
    bool trace_end = false;
    map<pair<long long, string>, task_2d*> task_deque;
    // std::priority_queue<task_2d*, std::vector<task_2d*>, task_2dComparator> execution_pool;
    set<task_2d*, task_2dComparator> execution_pool;
    rs* myrs;
    posmap_and_stash* pos_st;
    rs* myrs_pos1;
    posmap_and_stash* pos_st_pos1;
    rs* myrs_pos2;
    posmap_and_stash* pos_st_pos2;
    m_task_deque(){
        ;
    }



};
