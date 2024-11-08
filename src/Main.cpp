#include "Processor.h"
#include "Config.h"
#include "Controller.h"
#include "SpeedyController.h"
#include "Memory.h"
#include "DRAM.h"
#include "Statistics.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>
#include <functional>
#include <map>

/* Standards */
#include "Gem5Wrapper.h"
#include "DDR3.h"
#include "DDR4.h"
#include "DSARP.h"
#include "GDDR5.h"
#include "LPDDR3.h"
#include "LPDDR4.h"
#include "WideIO.h"
#include "WideIO2.h"
#include "HBM.h"
#include "SALP.h"
#include "ALDRAM.h"
#include "TLDRAM.h"
#include "STTMRAM.h"
#include "PCM.h"

using namespace std;
using namespace ramulator;

bool ramulator::warmup_complete = false;
bool print_flag = false;

template<typename T>
void run_oramtrace(const Config& configs, Memory<T, Controller>& memory, const char* tracename) {

    /* initialize DRAM trace */
    Trace trace(tracename);

    std::hash<const char*> hasher;
    int seed = hasher(tracename);
    srand(seed);

    /* run simulation */
    bool stall = false, end = false;
    // long long reads = 0, writes = 0, clks = 0;
    long addr = 0;
    Request::Type type = Request::Type::READ;
    map<int, int> latencies;
    auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

    Request req(addr, type, read_complete);

    long long total_num_blocks = TOTAL_NUM_TOP_NODE;
    int max_num_levels = ceil(log(float(total_num_blocks)) / log(float(KTREE))) + 1;
    long long valid_leaf_left = (pow(KTREE, (max_num_levels-1)) - 1) / (KTREE - 1);
    long long valid_leaf_right = (pow(KTREE, (max_num_levels)) - 1) / (KTREE - 1);
    cout << "Valid leaf ID range: " << valid_leaf_left << ":" << valid_leaf_right - 1 << endl;

    cout << "Num of top nodes: " << TOTAL_NUM_TOP_NODE << endl;
    cout << "Data level num of ways: " << stoll(configs["num_of_ways"]) << endl;
    cout << "Z: " << Z << endl;
    cout << "S: " << S << endl;
    cout << "A: " << A << endl;
        
    assert(S > 0);
    assert(Z > 0);
    assert(stoll(configs["num_of_ways"]) <= TOTAL_NUM_TOP_NODE);

    int num_start_lvls = max_num_levels - 1 - int(log((KTREE - 1) * stoll(configs["num_of_ways"])) / log(KTREE));
    int num_cached_lvls = max(num_start_lvls - 1, 0);
    long long cached_total = 0;
    for(int i = num_start_lvls; i < max_num_levels; i++){
      cached_total += (1 << i);
      cout << "cached_total: " << cached_total << endl;
      if(cached_total > CACHED_NUM_NODE){
        break;
      }
      else{
        num_cached_lvls = i + 1;
      }
    }

    cout << "Total number of ORAM protected cache line blocks: " << total_num_blocks << endl;
    cout << "Cached number of levels: " << num_cached_lvls << endl;
    cout << "Max number of levels: " << max_num_levels << endl;
    cout << "Start lvl is: " << num_start_lvls << endl;
    rs* myrs = new rs(1024, stoll(configs["num_of_ways"]), total_num_blocks, num_cached_lvls, "data");
    posmap_and_stash pos_st(total_num_blocks, myrs);
    rs* myrs_pos1 = new rs(1024, max(stoll(configs["num_of_ways"]) / 8, (long long)1), total_num_blocks / 8, max(3, num_cached_lvls - 3), "pos1");
    posmap_and_stash pos_st_pos1(total_num_blocks / 8, myrs_pos1);
    rs* myrs_pos2 = new rs(1024, max(stoll(configs["num_of_ways"]) / 64, (long long)1), total_num_blocks / 64, max(3, num_cached_lvls - 6), "pos2");
    posmap_and_stash pos_st_pos2(total_num_blocks / 64, myrs_pos2);
    long long cnt = 0;
    memory.init(myrs, myrs_pos1, myrs_pos2, &pos_st, &pos_st_pos1, &pos_st_pos2, req, &cnt);
    Request::Type prev_type = Request::Type::READ;

    map<string, long long> step_time_map;
    step_time_map["data"] = 0;
    step_time_map["pos1"] = 0;
    step_time_map["pos2"] = 0;
    long long start_time = 0;

    while (true){       
        end = !trace.get_dramtrace_request(addr, type);
        cnt++;
        
        int replay = 0;
        // cout << "Stash size is : " << myrs->stash.size() << endl;
        while(myrs->stash.size() >= STASH_MAITANENCE * myrs->total_num_blocks / myrs->num_of_ways){
          // if(pos_st.rw_counter % (A + 1) == 0){
              myrs->stash_violation++;
              cout << "Stash size is : " << myrs->stash.size() << endl;
              cout << "Stash size is above or equal to maintanence threshold: " << myrs->stash.size() << " >= " <<  STASH_MAITANENCE * myrs->total_num_blocks / myrs->num_of_ways << endl;
              cout << "Inserting dummy combination" << endl;
          // }
          memory.serve_until_need_address(-1, end);
          if(myrs->stash.size() < STASH_MAITANENCE * myrs->total_num_blocks / myrs->num_of_ways){
              break;
          }
          if(pos_st.rw_counter % (A + 1) == 0){
              replay++;
          }
          if(replay > 5){
              cout << "Replay time is:" << replay << ". Stash is having problem converging. Check the config is correct " << endl;
              break;
          }
        }
            
        if(cnt % 10000 == 0){
          cout << "Clk@ " << std::dec << memory.clks << " Working on the # " << cnt << " th memory instruction" << endl;
        }
        // cout << "Serving " << cnt << " th addr: " << std::hex << addr << std::dec << endl;
        memory.serve_until_need_address(addr, end);
        if(cnt == trace.number_of_lines / 2){
          // warm up stats done
          cout << "Warm up done after line access count: " << cnt << " out of " << trace.number_of_lines << " (" << cnt * 100.0 / trace.number_of_lines  << " %)" << endl;
          myrs->reset_stats();
        }
        if(end && !memory.pending_requests()){
          break;
        }
        if(end && memory.pending_requests()){
              memory.set_high_writeq_watermark(0.0f); // make sure that all write requests in the
                                                      // write queue are drained
          memory.tick();
          memory.clks ++;
if(memory.clks % 1000000 == 0) {myrs->print_allline(); cout << "Clk0 @ " << std::dec << memory.clks << " Finished " << std::dec << cnt << " instructions " << endl;}
          Stats::curTick++; // memory clock, global, for Statistics
          continue;
        }
        
        
        // start_time = memory.clks;
        // memory.serve_one_address((long) POS2_METADATA_START + addr / 64, myrs_pos2, pos_st_pos2, memory.clks, cnt, myrs_pos2->stall_reason, reads, writes, print_flag);
        // step_time_map["pos2"] += (memory.clks - start_time);

        // start_time = memory.clks;
        // memory.serve_one_address((long) POS1_METADATA_START + addr / 8, myrs_pos1, pos_st_pos1, memory.clks, cnt, myrs_pos1->stall_reason, reads, writes, print_flag);
        // step_time_map["pos1"] += (memory.clks - start_time);

        // start_time = memory.clks;
        // memory.serve_one_address(addr, myrs, pos_st, memory.clks, cnt, myrs->stall_reason, reads, writes, print_flag);
        // step_time_map["data"] += (memory.clks - start_time);

//         memory.tick();
//         memory.clks ++;
// if(memory.clks % 1000000 == 0) {myrs->print_allline(); cout << "Clk4 @ " << std::dec << memory.clks << " Finished " << std::dec << cnt << " instructions " << endl;}
//         Stats::curTick++; // memory clock, global, for Statistics
    }
    while(myrs->ready_to_pop_task.size()){
      myrs->ready_to_pop_task.front().print_task_rs_on_finish();
      myrs->ready_to_pop_task.pop();
    }
    while(myrs_pos1->ready_to_pop_task.size()){
      myrs_pos1->ready_to_pop_task.front().print_task_rs_on_finish();
      myrs_pos1->ready_to_pop_task.pop();
    }
    while(myrs_pos2->ready_to_pop_task.size()){
      myrs_pos2->ready_to_pop_task.front().print_task_rs_on_finish();
      myrs_pos2->ready_to_pop_task.pop();
    }
    // This a workaround for statistics set only initially lost in the end
    memory.finish();
    Stats::statlist.printall();

    cout << "\n===*** Data ***====" << endl;
    myrs->print_stats();
    cout << "\n====*** pos1 ***====" << endl;
    myrs_pos1->print_stats();
    cout << "\n====*** pos2 ***====" << endl;
    myrs_pos2->print_stats();

    cout << "=================" << endl;
    cout << "The final DRAM clk is: " << std::dec << memory.clks << " ticks." << endl;
    cout << "Idle waiting clk is " << memory.idle_clks << ", which is " << memory.idle_clks * 100.0 / memory.clks << " %" << endl;
    cout << "DRAM Frequency is: " << (memory.spec)->speed_entry.freq << "MHz" << endl;
    cout << "The final time in ns is: " << std::dec << memory.clks * 1000.0 / (memory.spec)->speed_entry.freq << " ns." << endl;

    
    cout << "Time distribution: " << endl;
    cout << "Data pull time is: " << myrs->stall_reason["rw_swtich"] << endl;
    for (const auto& kvp : step_time_map) {
        std::cout << kvp.first << ": " << kvp.second << std::endl;
    }

    cout << "Data distribution: " << endl;
    cout << "Pos2 data: " << myrs_pos2->rs_read_ddr_lines + myrs_pos2->rs_write_ddr_lines + myrs_pos2->rs_early_ddr_lines << endl;
    cout << "Pos1 data: " << myrs_pos1->rs_read_ddr_lines + myrs_pos1->rs_write_ddr_lines + myrs_pos1->rs_early_ddr_lines << endl;
    cout << "Data: " << myrs->rs_read_ddr_lines + myrs->rs_write_ddr_lines + myrs->rs_early_ddr_lines << endl;

    cout << "Node access DDR: " <<  2 * myrs->check_nodeid_ddr << endl;
    cout << "Pull DDR: " << myrs->rs_read_ddr_lines << endl;
    cout << "WB DDR: " << myrs->rs_write_ddr_lines << endl;
    cout << "Early DDR: " << myrs->rs_early_ddr_lines << endl;

    cout << "Achieved BW is: " << std::dec << 64 * (memory.spec)->speed_entry.freq / 1000.0 * (memory.reads+memory.writes) / (1.0 * memory.clks) << " GB/s" << endl;
    cout << "Ideal BW is: " << std::dec << configs.get_channels() * (memory.spec)->speed_entry.freq * 2 / 1000.0 * (memory.spec)->channel_width / 8.0 << " GB/s" << endl;
    cout << "Metadata cache conclusion: " << endl;
    myrs->metadata_cache->cache_conclude();
    // for(int i = 0; i < myrs->latency_track.size(); i++){
    //   if(myrs->latency_track[i] > 0){
    //     cout << myrs->latency_track[i] << endl;
    //   } 
    // }
    // cout << "==== Stash hit ID ==== " << endl;
    // cout << "HitID,";
    // for(int i = 0; i < myrs_pos2->stash_hit_track.size(); i++){
    //   if(myrs_pos2->stash_hit_track[i]){
    //     cout << i << ",";
    //   } 
    // }
    // cout << endl;
}

template<typename T>
void run_idealtrace(const Config& configs, Memory<T, Controller>& memory, const char* tracename) {

}

template<typename T>
void run_ringoramtrace(const Config& configs, Memory<T, Controller>& memory, const char* tracename) {

}

template<typename T>
void run_pathoramtrace(const Config& configs, Memory<T, Controller>& memory, const char* tracename) {

    /* initialize DRAM trace */
    Trace trace(tracename);

    /* run simulation */
    bool stall = false, end = false;
    long long reads = 0, writes = 0, clks = 0;
    long addr = 0;
    Request::Type type = Request::Type::READ;
    map<int, int> latencies;
    auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

    Request req(addr, type, read_complete);

    long long total_num_blocks = TOTAL_NUM_TOP_NODE;
    int max_num_levels = ceil(log(float(total_num_blocks)) / log(float(KTREE))) + 1;
    cout << "Total number of ORAM protected cache line blocks: " << total_num_blocks << endl;
    cout << "Max number of levels: " << max_num_levels << endl;
    long long valid_leaf_left = (pow(KTREE, (max_num_levels-1)) - 1) / (KTREE - 1);
    long long valid_leaf_right = (pow(KTREE, (max_num_levels)) - 1) / (KTREE - 1);
    cout << "Valid leaf ID range: " << valid_leaf_left << ":" << valid_leaf_right - 1 << endl;
        
    assert(PBUCKET > 0);

    int num_cached_lvls = int(log(CACHED_NUM_NODE + 1) / log(KTREE));
    int num_start_lvls = max_num_levels - 1 - int(log((KTREE - 1) * stoll(configs["num_of_ways"])) / log(KTREE));
    // rs* myrs = new rs((A + 1) * 16, max_num_levels, num_cached_lvls, "data");
    // myrs->type = "pathoram";
    // posmap_and_stash pos_st(total_num_blocks, myrs);
    // memory.init(myrs);
    
    rs* myrs = new rs(32, total_num_blocks, total_num_blocks, num_cached_lvls, "data");
    posmap_and_stash pos_st(total_num_blocks, myrs);
    myrs->type = "pathoram";
    // rs* myrs_pos1 = new rs(32, total_num_blocks / 8, max(0, num_cached_lvls - 3), "pos1");
    // posmap_and_stash pos_st_pos1(total_num_blocks / 8, myrs_pos1);
    // rs* myrs_pos2 = new rs(32, total_num_blocks / 64, max(0, num_cached_lvls - 6), "pos2");
    // posmap_and_stash pos_st_pos2(total_num_blocks / 64, myrs_pos2);
    long long cnt = 0;
    memory.init(myrs, nullptr, nullptr, &pos_st, nullptr, nullptr, req, &cnt);

    while (true){       
        end = !trace.get_dramtrace_request(addr, type);
        if(end && !memory.pending_requests()){
          break;
        }
        if(end && memory.pending_requests()){
              memory.set_high_writeq_watermark(0.0f); // make sure that all write requests in the
                                                      // write queue are drained
          memory.tick();
          clks ++;
          Stats::curTick++; // memory clock, global, for Statistics
          continue;
        }

        long long random_block = rand() % total_num_blocks;
        std::vector<long> pathoram_addr_vec;
        for(int i = num_cached_lvls; i < max_num_levels; i++){
          long long node_id = myrs->P(random_block, i, max_num_levels);
          for(int j = 0; j < PBUCKET; j++){
            pathoram_addr_vec.push_back(64 * (node_id * PBUCKET + j));
          }
        }
        while(true){
          int index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::READ;
              stall = !memory.send(req);
              if (!stall){
                  if (type == Request::Type::READ) reads++;
                  else if (type == Request::Type::WRITE) writes++;
                  index++;
              }
              memory.tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          memory.set_high_writeq_watermark(0.0f);
          while(memory.pending_requests()){
              memory.tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          memory.set_high_writeq_watermark(0.8f);
          index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::WRITE;
              stall = !memory.send(req);
              if (!stall){
                  if (type == Request::Type::READ) reads++;
                  else if (type == Request::Type::WRITE) writes++;
                  index++;
              }
              memory.tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          memory.set_high_writeq_watermark(0.0f);
          while(memory.pending_requests()){
              memory.tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          memory.set_high_writeq_watermark(0.8f);
          break;
        }
    }
    // This a workaround for statistics set only initially lost in the end
    memory.finish();
    Stats::statlist.printall();
    cout << "The final DRAM clk is: " << std::dec << clks << " ticks." << endl;
    cout << "DRAM Frequency is: " << (memory.spec)->speed_entry.freq << "MHz" << endl;
    cout << "The final time in ns is: " << std::dec << clks * 1000.0 / (memory.spec)->speed_entry.freq << " ns." << endl;

    cout << "Achieved BW is: " << std::dec << 64 * (memory.spec)->speed_entry.freq / 1000.0 * (reads+writes) / (1.0 * clks) << " GB/s" << endl;
    cout << "Ideal BW is: " << std::dec << configs.get_channels() * (memory.spec)->speed_entry.freq * 2 / 1000.0 * (memory.spec)->channel_width / 8.0 << " GB/s" << endl;
    cout << "Metadata cache conclusion: " << endl;
    myrs->metadata_cache->cache_conclude();
}

template<typename T>
void run_pagetrace(const Config& configs, Memory<T, Controller>& memory, const char* tracename) {

    /* initialize DRAM trace */
    Trace trace(tracename);

    /* run simulation */
    bool stall = false, end = false;
    long long reads = 0, writes = 0, clks = 0;
    long addr = 0;
    Request::Type type = Request::Type::READ;
    map<int, int> latencies;
    auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

    Request req(addr, type, read_complete);

    long long total_num_blocks = TOTAL_NUM_TOP_NODE;
    int max_num_levels = ceil(log(float(total_num_blocks)) / log(float(KTREE))) + 1;
    cout << "Total number of ORAM protected cache line blocks: " << total_num_blocks << endl;
    cout << "Max number of levels: " << max_num_levels << endl;
    long long valid_leaf_left = (pow(KTREE, (max_num_levels-1)) - 1) / (KTREE - 1);
    long long valid_leaf_right = (pow(KTREE, (max_num_levels)) - 1) / (KTREE - 1);
    cout << "Valid leaf ID range: " << valid_leaf_left << ":" << valid_leaf_right - 1 << endl;
        
    assert(PBUCKET > 0);

    int num_cached_lvls = int(log(CACHED_NUM_NODE + 1) / log(KTREE));
    // rs* myrs = new rs((A + 1) * 16, max_num_levels, num_cached_lvls, "data");
    // myrs->type = "pathoram";
    // posmap_and_stash pos_st(total_num_blocks, myrs);
    // memory.init(myrs);
    
    int num_start_lvls = max_num_levels - 1 - int(log((KTREE - 1) * total_num_blocks) / log(KTREE));
    rs* myrs = new rs(32, total_num_blocks, total_num_blocks, num_cached_lvls, "data");
    posmap_and_stash pos_st(total_num_blocks, myrs);
    myrs->type = "pathoram";
    // rs* myrs_pos1 = new rs(32, total_num_blocks / 8, max(0, num_cached_lvls - 3), "pos1");
    // posmap_and_stash pos_st_pos1(total_num_blocks / 8, myrs_pos1);
    // rs* myrs_pos2 = new rs(32, total_num_blocks / 64, max(0, num_cached_lvls - 6), "pos2");
    // posmap_and_stash pos_st_pos2(total_num_blocks / 64, myrs_pos2);
    long long cnt = 0;
    memory.init(myrs, nullptr, nullptr, &pos_st, nullptr, nullptr, req, &cnt);

    while (true){       
        end = !trace.get_dramtrace_request(addr, type);
        if(end && !memory.pending_requests()){
          break;
        }
        if(end && memory.pending_requests()){
              memory.set_high_writeq_watermark(0.0f); // make sure that all write requests in the
                                                      // write queue are drained
          memory.tick();
          clks ++;
          Stats::curTick++; // memory clock, global, for Statistics
          continue;
        }

        long long random_block = rand() % total_num_blocks;
        std::vector<long> pathoram_addr_vec;
        for(int i = num_cached_lvls; i < max_num_levels; i++){
          long long node_id = myrs->P(random_block, i, max_num_levels);
          for(int j = 0; j < PBUCKET; j++){
            pathoram_addr_vec.push_back(64 * (node_id * PBUCKET + j));
          }
          if(i % 2 == 0){
            continue;
          }

          long long sibling_node_id = -1;
          if(node_id % 2 == 0)  {sibling_node_id = node_id - 1;}
          else{sibling_node_id = node_id + 1;}
          assert(sibling_node_id > 0);

          for(int j = 0; j < PBUCKET; j++){
            pathoram_addr_vec.push_back(64 * (sibling_node_id * PBUCKET + j));
          }
        }
        while(true){
          int index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::READ;
              stall = !memory.send(req);
              if (!stall){
                  if (type == Request::Type::READ) reads++;
                  else if (type == Request::Type::WRITE) writes++;
                  index++;
              }
              memory.tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          memory.set_high_writeq_watermark(0.0f);
          while(memory.pending_requests()){
              memory.tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          memory.set_high_writeq_watermark(0.8f);
          index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::WRITE;
              stall = !memory.send(req);
              if (!stall){
                  if (type == Request::Type::READ) reads++;
                  else if (type == Request::Type::WRITE) writes++;
                  index++;
              }
              memory.tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          memory.set_high_writeq_watermark(0.0f);
          while(memory.pending_requests()){
              memory.tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          memory.set_high_writeq_watermark(0.8f);
          break;
        }
    }
    // This a workaround for statistics set only initially lost in the end
    memory.finish();
    Stats::statlist.printall();
    cout << "The final DRAM clk is: " << std::dec << clks << " ticks." << endl;
    cout << "DRAM Frequency is: " << (memory.spec)->speed_entry.freq << "MHz" << endl;
    cout << "The final time in ns is: " << std::dec << clks * 1000.0 / (memory.spec)->speed_entry.freq << " ns." << endl;

    cout << "Achieved BW is: " << std::dec << 64 * (memory.spec)->speed_entry.freq / 1000.0 * (reads+writes) / (1.0 * clks) << " GB/s" << endl;
    cout << "Ideal BW is: " << std::dec << configs.get_channels() * (memory.spec)->speed_entry.freq * 2 / 1000.0 * (memory.spec)->channel_width / 8.0 << " GB/s" << endl;
    cout << "Metadata cache conclusion: " << endl;
    myrs->metadata_cache->cache_conclude();
}

template<typename T>
void run_dramtrace(const Config& configs, Memory<T, Controller>& memory, const char* tracename) {

    /* initialize DRAM trace */
    Trace trace(tracename);

    /* run simulation */
    bool stall = false, end = false;
    long long reads = 0, writes = 0, clks = 0;
    long addr = 0;
    Request::Type type = Request::Type::READ;
    map<int, int> latencies;
    auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

    Request req(addr, type, read_complete);

    while (!end || memory.pending_requests()){
        if (!end && !stall){
            end = !trace.get_dramtrace_request(addr, type);
        }

        if (!end){
            req.addr = addr;
            req.type = type;
            stall = !memory.send(req);
            if (!stall){
                if (type == Request::Type::READ) reads++;
                else if (type == Request::Type::WRITE) writes++;
            }
        }
        else {
            memory.set_high_writeq_watermark(0.0f); // make sure that all write requests in the
                                                    // write queue are drained
        }

        memory.tick();
        clks ++;
        Stats::curTick++; // memory clock, global, for Statistics
    }
    // This a workaround for statistics set only initially lost in the end
    memory.finish();
    Stats::statlist.printall();
    cout << "The final DRAM clk is: " << std::dec << clks << " ticks." << endl;
    cout << "DRAM Frequency is: " << (memory.spec)->speed_entry.freq << "MHz" << endl;
    cout << "The final time in ns is: " << std::dec << clks * 1000.0 / (memory.spec)->speed_entry.freq << " ns." << endl;

    cout << "Achieved BW is: " << std::dec << 64 * (memory.spec)->speed_entry.freq / 1000.0 * (reads+writes) / (1.0 * clks) << " GB/s" << endl;
    cout << "Ideal BW is: " << std::dec << configs.get_channels() * (memory.spec)->speed_entry.freq * 2 / 1000.0 * (memory.spec)->channel_width / 8.0 << " GB/s" << endl;
}

template <typename T>
void run_cputrace(const Config& configs, Memory<T, Controller>& memory, const std::vector<const char *>& files)
{
    int cpu_tick = configs.get_cpu_tick();
    int mem_tick = configs.get_mem_tick();
    auto send = bind(&Memory<T, Controller>::send, &memory, placeholders::_1);
    Processor proc(configs, files, send, memory);

    long warmup_insts = configs.get_warmup_insts();
    bool is_warming_up = (warmup_insts != 0);

    for(long i = 0; is_warming_up; i++){
        proc.tick();
        Stats::curTick++;
        if (i % cpu_tick == (cpu_tick - 1))
            for (int j = 0; j < mem_tick; j++)
                memory.tick();

        is_warming_up = false;
        for(int c = 0; c < proc.cores.size(); c++){
            if(proc.cores[c]->get_insts() < warmup_insts)
                is_warming_up = true;
        }

        if (is_warming_up && proc.has_reached_limit()) {
            printf("WARNING: The end of the input trace file was reached during warmup. "
                    "Consider changing warmup_insts in the config file. \n");
            break;
        }

    }

    warmup_complete = true;
    printf("Warmup complete! Resetting stats...\n");
    Stats::reset_stats();
    proc.reset_stats();
    assert(proc.get_insts() == 0);

    printf("Starting the simulation...\n");

    int tick_mult = cpu_tick * mem_tick;
    for (long i = 0; ; i++) {
        if (((i % tick_mult) % mem_tick) == 0) { // When the CPU is ticked cpu_tick times,
                                                 // the memory controller should be ticked mem_tick times
            proc.tick();
            Stats::curTick++; // processor clock, global, for Statistics

            if (configs.calc_weighted_speedup()) {
                if (proc.has_reached_limit()) {
                    break;
                }
            } else {
                if (configs.is_early_exit()) {
                    if (proc.finished())
                    break;
                } else {
                if (proc.finished() && (memory.pending_requests() == 0))
                    break;
                }
            }
        }

        if (((i % tick_mult) % cpu_tick) == 0) // TODO_hasan: Better if the processor ticks the memory controller
            memory.tick();

    }
    // This a workaround for statistics set only initially lost in the end
    memory.finish();
    Stats::statlist.printall();
}

template<typename T>
void start_run(const Config& configs, T* spec, const vector<const char*>& files) {
  // initiate controller and memory
  int C = configs.get_channels(), R = configs.get_ranks();
  // Check and Set channel, rank number
  spec->set_channel_number(C);
  spec->set_rank_number(R);
  std::vector<Controller<T>*> ctrls;
  for (int c = 0 ; c < C ; c++) {
    DRAM<T>* channel = new DRAM<T>(spec, T::Level::Channel);
    channel->id = c;
    channel->regStats("");
    Controller<T>* ctrl = new Controller<T>(configs, channel);
    ctrls.push_back(ctrl);
  }
  Memory<T, Controller> memory(configs, ctrls);

  assert(files.size() != 0);
  if (configs["trace_type"] == "CPU") {
    run_cputrace(configs, memory, files);
  } else if (configs["trace_type"] == "DRAM") {
    run_dramtrace(configs, memory, files[0]);
  }
  else if(configs["trace_type"] == "ORAM"){
    run_oramtrace(configs, memory, files[0]);
  }
  else if(configs["trace_type"] == "IDEAL"){
    run_idealtrace(configs, memory, files[0]);
  }
  else if(configs["trace_type"] == "RORAM"){
    run_ringoramtrace(configs, memory, files[0]);
  }
  else if(configs["trace_type"] == "PORAM"){
    run_pathoramtrace(configs, memory, files[0]);
  }
  else if(configs["trace_type"] == "PAGEORAM"){
    run_pagetrace(configs, memory, files[0]);
  }
}

int main(int argc, const char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <configs-file> --mode=cpu,dram [--stats <filename>] <trace-filename1> <trace-filename2>\n"
            "Example: %s ramulator-configs.cfg --mode=cpu cpu.trace cpu.trace\n", argv[0], argv[0]);
        return 0;
    }

    Config configs(argv[1]);

    const std::string& standard = configs["standard"];
    assert(standard != "" || "DRAM standard should be specified.");

    const char *trace_type = strstr(argv[2], "=");
    trace_type++;
    if (strcmp(trace_type, "cpu") == 0) {
      configs.add("trace_type", "CPU");
    } else if (strcmp(trace_type, "dram") == 0) {
      configs.add("trace_type", "DRAM");
    } else if (strcmp(trace_type, "oram") == 0) {
      configs.add("trace_type", "ORAM");
    } else if (strcmp(trace_type, "pathoram") == 0) {
      configs.add("trace_type", "PORAM");
    } else if (strcmp(trace_type, "pageoram") == 0) {
      configs.add("trace_type", "PAGEORAM");
    } else if (strcmp(trace_type, "ringoram") == 0) {
      configs.add("trace_type", "RORAM");
    } else if (strcmp(trace_type, "idealoram") == 0) {
      configs.add("trace_type", "IDEAL");
    } else {
      printf("invalid trace type: %s\n", trace_type);
      assert(false);
    }

    int trace_start = 3;
    string stats_out;
    if (strcmp(argv[trace_start], "--stats") == 0) {
      Stats::statlist.output(argv[trace_start+1]);
      stats_out = argv[trace_start+1];
      trace_start += 2;
    } else {
      Stats::statlist.output(standard+".stats");
      stats_out = standard + string(".stats");
    }

    if (strcmp(argv[trace_start], "--num_of_ways") == 0) {
      configs.add("num_of_ways", argv[trace_start+1]);
      trace_start += 2;
    }

    // A separate file defines mapping for easy config.
    if (strcmp(argv[trace_start], "--mapping") == 0) {
      configs.add("mapping", argv[trace_start+1]);
      trace_start += 2;
    } else {
      configs.add("mapping", "defaultmapping");
    }

    std::vector<const char*> files(&argv[trace_start], &argv[argc]);
    configs.set_core_num(argc - trace_start);

    if (standard == "DDR3") {
      DDR3* ddr3 = new DDR3(configs["org"], configs["speed"]);
      start_run(configs, ddr3, files);
    } else if (standard == "DDR4") {
      DDR4* ddr4 = new DDR4(configs["org"], configs["speed"]);
      start_run(configs, ddr4, files);
    } else if (standard == "SALP-MASA") {
      SALP* salp8 = new SALP(configs["org"], configs["speed"], "SALP-MASA", configs.get_subarrays());
      start_run(configs, salp8, files);
    } else if (standard == "LPDDR3") {
      LPDDR3* lpddr3 = new LPDDR3(configs["org"], configs["speed"]);
      start_run(configs, lpddr3, files);
    } else if (standard == "LPDDR4") {
      // total cap: 2GB, 1/2 of others
      LPDDR4* lpddr4 = new LPDDR4(configs["org"], configs["speed"]);
      start_run(configs, lpddr4, files);
    } else if (standard == "GDDR5") {
      GDDR5* gddr5 = new GDDR5(configs["org"], configs["speed"]);
      start_run(configs, gddr5, files);
    } else if (standard == "HBM") {
      HBM* hbm = new HBM(configs["org"], configs["speed"]);
      start_run(configs, hbm, files);
    } else if (standard == "WideIO") {
      // total cap: 1GB, 1/4 of others
      WideIO* wio = new WideIO(configs["org"], configs["speed"]);
      start_run(configs, wio, files);
    } else if (standard == "WideIO2") {
      // total cap: 2GB, 1/2 of others
      WideIO2* wio2 = new WideIO2(configs["org"], configs["speed"], configs.get_channels());
      wio2->channel_width *= 2;
      start_run(configs, wio2, files);
    } else if (standard == "STTMRAM") {
      STTMRAM* sttmram = new STTMRAM(configs["org"], configs["speed"]);
      start_run(configs, sttmram, files);
    } else if (standard == "PCM") {
      PCM* pcm = new PCM(configs["org"], configs["speed"]);
      start_run(configs, pcm, files);
    }
    // Various refresh mechanisms
      else if (standard == "DSARP") {
      DSARP* dsddr3_dsarp = new DSARP(configs["org"], configs["speed"], DSARP::Type::DSARP, configs.get_subarrays());
      start_run(configs, dsddr3_dsarp, files);
    } else if (standard == "ALDRAM") {
      ALDRAM* aldram = new ALDRAM(configs["org"], configs["speed"]);
      start_run(configs, aldram, files);
    } else if (standard == "TLDRAM") {
      TLDRAM* tldram = new TLDRAM(configs["org"], configs["speed"], configs.get_subarrays());
      start_run(configs, tldram, files);
    }

    printf("Simulation done. Statistics written to %s\n", stats_out.c_str());

    return 0;
}
