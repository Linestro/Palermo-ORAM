#ifndef __MEMORY_H
#define __MEMORY_H

#include "Config.h"
#include "DRAM.h"
#include "Request.h"
#include "Controller.h"
#include "SpeedyController.h"
#include "Statistics.h"
#include "GDDR5.h"
#include "HBM.h"
#include "LPDDR3.h"
#include "LPDDR4.h"
#include "WideIO2.h"
#include "DSARP.h"
#include <vector>
#include <functional>
#include <cmath>
#include <cassert>
#include <tuple>

using namespace std;

typedef vector<unsigned int> MapSrcVector;
typedef map<unsigned int, MapSrcVector > MapSchemeEntry;
typedef map<unsigned int, MapSchemeEntry> MapScheme;

namespace ramulator
{

class MemoryBase{
public:
    MemoryBase() {}
    virtual ~MemoryBase() {}
    virtual double clk_ns() = 0;
    virtual void tick() = 0;
    virtual bool send(Request req) = 0;
    virtual int pending_requests() = 0;
    virtual void finish(void) = 0;
    virtual long page_allocator(long addr, int coreid) = 0;
    virtual void record_core(int coreid) = 0;
    virtual void set_high_writeq_watermark(const float watermark) = 0;
    virtual void set_low_writeq_watermark(const float watermark) = 0;
};

template <class T, template<typename> class Controller = Controller >
class Memory : public MemoryBase
{
protected:
  ScalarStat dram_capacity;
  ScalarStat num_dram_cycles;
  ScalarStat num_incoming_requests;
  VectorStat num_read_requests;
  VectorStat num_write_requests;
  ScalarStat ramulator_active_cycles;
  VectorStat incoming_requests_per_channel;
  VectorStat incoming_read_reqs_per_channel;

  ScalarStat physical_page_replacement;
  ScalarStat maximum_bandwidth;
  ScalarStat in_queue_req_num_sum;
  ScalarStat in_queue_read_req_num_sum;
  ScalarStat in_queue_write_req_num_sum;
  ScalarStat in_queue_req_num_avg;
  ScalarStat in_queue_read_req_num_avg;
  ScalarStat in_queue_write_req_num_avg;

#ifndef INTEGRATED_WITH_GEM5
  VectorStat record_read_requests;
  VectorStat record_write_requests;
#endif

  long max_address;
  MapScheme mapping_scheme;
  
public:
    enum class Type {
        ChRaBaRoCo,
        RoBaRaCoCh,
        MAX,
    } type = Type::RoBaRaCoCh;

    enum class Translation {
      None,
      Random,
      MAX,
    } translation = Translation::None;

    std::map<string, Translation> name_to_translation = {
      {"None", Translation::None},
      {"Random", Translation::Random},
    };

    vector<int> free_physical_pages;
    long free_physical_pages_remaining;
    map<pair<int, long>, long> page_translation;

    vector<Controller<T>*> ctrls;
    T * spec;
    vector<int> addr_bits;
    string mapping_file;
    bool use_mapping_file;
    bool dump_mapping;
    
    int tx_bits;

    Memory(const Config& configs, vector<Controller<T>*> ctrls)
        : ctrls(ctrls),
          spec(ctrls[0]->channel->spec),
          addr_bits(int(T::Level::MAX))
    {
        // make sure 2^N channels/ranks
        // TODO support channel number that is not powers of 2
        int *sz = spec->org_entry.count;
        assert((sz[0] & (sz[0] - 1)) == 0);
        assert((sz[1] & (sz[1] - 1)) == 0);
        // validate size of one transaction
        int tx = (spec->prefetch_size * spec->channel_width / 8);
        tx_bits = calc_log2(tx);
        assert((1<<tx_bits) == tx);
        
        // Parsing mapping file and initialize mapping table
        use_mapping_file = false;
        dump_mapping = false;
        if (spec->standard_name.substr(0, 4) == "DDR3"){
            if (configs["mapping"] != "defaultmapping"){
              init_mapping_with_file(configs["mapping"]);
              // dump_mapping = true;
              use_mapping_file = true;
            }
        }
        // If hi address bits will not be assigned to Rows
        // then the chips must not be LPDDRx 6Gb, 12Gb etc.
        if (type != Type::RoBaRaCoCh && spec->standard_name.substr(0, 5) == "LPDDR")
            assert((sz[int(T::Level::Row)] & (sz[int(T::Level::Row)] - 1)) == 0);

        max_address = spec->channel_width / 8;

        for (unsigned int lev = 0; lev < addr_bits.size(); lev++) {
          addr_bits[lev] = calc_log2(sz[lev]);
            max_address *= sz[lev];
        }

        addr_bits[int(T::Level::MAX) - 1] -= calc_log2(spec->prefetch_size);

        // Initiating translation
        if (configs.contains("translation")) {
          translation = name_to_translation[configs["translation"]];
        }
        if (translation != Translation::None) {
          // construct a list of available pages
          // TODO: this should not assume a 4KB page!
          free_physical_pages_remaining = max_address >> 12;

          free_physical_pages.resize(free_physical_pages_remaining, -1);
        }

        dram_capacity
            .name("dram_capacity")
            .desc("Number of bytes in simulated DRAM")
            .precision(0)
            ;
        dram_capacity = max_address;

        num_dram_cycles
            .name("dram_cycles")
            .desc("Number of DRAM cycles simulated")
            .precision(0)
            ;
        num_incoming_requests
            .name("incoming_requests")
            .desc("Number of incoming requests to DRAM")
            .precision(0)
            ;
        num_read_requests
            .init(configs.get_core_num())
            .name("read_requests")
            .desc("Number of incoming read requests to DRAM per core")
            .precision(0)
            ;
        num_write_requests
            .init(configs.get_core_num())
            .name("write_requests")
            .desc("Number of incoming write requests to DRAM per core")
            .precision(0)
            ;
        incoming_requests_per_channel
            .init(sz[int(T::Level::Channel)])
            .name("incoming_requests_per_channel")
            .desc("Number of incoming requests to each DRAM channel")
            ;
        incoming_read_reqs_per_channel
            .init(sz[int(T::Level::Channel)])
            .name("incoming_read_reqs_per_channel")
            .desc("Number of incoming read requests to each DRAM channel")
            ;

        ramulator_active_cycles
            .name("ramulator_active_cycles")
            .desc("The total number of cycles that the DRAM part is active (serving R/W)")
            .precision(0)
            ;
        physical_page_replacement
            .name("physical_page_replacement")
            .desc("The number of times that physical page replacement happens.")
            .precision(0)
            ;
        maximum_bandwidth
            .name("maximum_bandwidth")
            .desc("The theoretical maximum bandwidth (Bps)")
            .precision(0)
            ;
        in_queue_req_num_sum
            .name("in_queue_req_num_sum")
            .desc("Sum of read/write queue length")
            .precision(0)
            ;
        in_queue_read_req_num_sum
            .name("in_queue_read_req_num_sum")
            .desc("Sum of read queue length")
            .precision(0)
            ;
        in_queue_write_req_num_sum
            .name("in_queue_write_req_num_sum")
            .desc("Sum of write queue length")
            .precision(0)
            ;
        in_queue_req_num_avg
            .name("in_queue_req_num_avg")
            .desc("Average of read/write queue length per memory cycle")
            .precision(6)
            ;
        in_queue_read_req_num_avg
            .name("in_queue_read_req_num_avg")
            .desc("Average of read queue length per memory cycle")
            .precision(6)
            ;
        in_queue_write_req_num_avg
            .name("in_queue_write_req_num_avg")
            .desc("Average of write queue length per memory cycle")
            .precision(6)
            ;
#ifndef INTEGRATED_WITH_GEM5
        record_read_requests
            .init(configs.get_core_num())
            .name("record_read_requests")
            .desc("record read requests for this core when it reaches request limit or to the end")
            ;

        record_write_requests
            .init(configs.get_core_num())
            .name("record_write_requests")
            .desc("record write requests for this core when it reaches request limit or to the end")
            ;
#endif

    }

    ~Memory()
    {
        for (auto ctrl: ctrls)
            delete ctrl;
        delete spec;
    }

    double clk_ns()
    {
        return spec->speed_entry.tCK;
    }

    void record_core(int coreid) {
#ifndef INTEGRATED_WITH_GEM5
      record_read_requests[coreid] = num_read_requests[coreid];
      record_write_requests[coreid] = num_write_requests[coreid];
#endif
      for (auto ctrl : ctrls) {
        ctrl->record_core(coreid);
      }
    }

    void init(rs* myrs, rs* myrs_pos1 = nullptr, rs* myrs_pos2 = nullptr){
        for (auto ctrl : ctrls) {
            ctrl->myrs = myrs;
            ctrl->myrs_pos1 = myrs_pos1;
            ctrl->myrs_pos2 = myrs_pos2;
        }
    }

    void serve_one_address_iroram(long addr, rs* myrs, posmap_and_stash &pos_st, long long &clks, long long cnt, map<string, long long>& stall_reason, long long &reads, long long &writes, bool print_flag=false){
            
        bool stall = false, end = false;
        Request::Type type = Request::Type::READ;
        map<int, int> latencies;
        auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

        Request req(addr, type, read_complete);

        Request::Type prev_type = Request::Type::READ;
        
        long long random_block = rand() % pos_st.total_num_blocks;
        std::vector<long> pathoram_addr_vec;
        for(int i = myrs->lowest_uncached_lvl; i < myrs->num_of_lvls; i++){
          long long node_id = myrs->P(random_block, i, myrs->num_of_lvls);
          int buckiet_sz = (i == myrs->num_of_lvls - 1) ? Z : (i >= 10 && i <= 15) ? 1 : (i >= 16 && i <= 18) ? 2 : Z;
          for(int j = 0; j < buckiet_sz; j++){
            pathoram_addr_vec.push_back(64 * (node_id * PBUCKET + j));
          }
        }

        while(true){
          int index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::READ;
              stall = !send(req);
              if (!stall){
                  if (req.type == Request::Type::READ) reads++;
                  else if (req.type == Request::Type::WRITE) writes++;
                  index++;
              }
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          set_high_writeq_watermark(0.0f);
          while(pending_requests()){
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
              stall_reason["rw_swtich"]++;
          }
          set_high_writeq_watermark(0.8f);
          index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::WRITE;
              stall = !send(req);
              if (!stall){
                  if (req.type == Request::Type::READ) reads++;
                  else if (req.type == Request::Type::WRITE) writes++;
                  index++;
              }
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
        //   set_high_writeq_watermark(0.0f);
        //   while(pending_requests()){
        //       tick();
        //       clks ++;
        //       Stats::curTick++; // memory clock, global, for Statistics
        //       stall_reason["rw_swtich"]++;
        //   }
        //   set_high_writeq_watermark(0.8f);
          break;
        }
        
    }

    void serve_one_address_pageoram(long addr, rs* myrs, posmap_and_stash &pos_st, long long &clks, long long cnt, map<string, long long>& stall_reason, long long &reads, long long &writes, bool print_flag=false){
            
        bool stall = false, end = false;
        Request::Type type = Request::Type::READ;
        map<int, int> latencies;
        auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

        Request req(addr, type, read_complete);

        Request::Type prev_type = Request::Type::READ;
        
        long long random_block = rand() % pos_st.total_num_blocks;
        std::vector<long> pathoram_addr_vec;
        for(int i = myrs->lowest_uncached_lvl; i < myrs->num_of_lvls; i++){
          long long node_id = myrs->P(random_block, i, myrs->num_of_lvls);
          for(int j = 0; j < PBUCKET - 1; j++){
            pathoram_addr_vec.push_back(64 * (node_id * (PBUCKET - 1) + j));
          }
          if(i % 2 == 1){            
            long long sibling_node = (node_id % 2 == 0) ? node_id - 1 : node_id + 1;
            for(int j = 0; j < PBUCKET - 1; j++){
                pathoram_addr_vec.push_back(64 * (sibling_node * (PBUCKET - 1) + j));
            }
          }
        }

        while(true){
          int index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::READ;
              stall = !send(req);
              if (!stall){
                  if (req.type == Request::Type::READ) reads++;
                  else if (req.type == Request::Type::WRITE) writes++;
                  index++;
              }
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          set_high_writeq_watermark(0.0f);
          while(pending_requests()){
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
              stall_reason["rw_swtich"]++;
          }
          set_high_writeq_watermark(0.8f);
          index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::WRITE;
              stall = !send(req);
              if (!stall){
                  if (req.type == Request::Type::READ) reads++;
                  else if (req.type == Request::Type::WRITE) writes++;
                  index++;
              }
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
        //   set_high_writeq_watermark(0.0f);
        //   while(pending_requests()){
        //       tick();
        //       clks ++;
        //       Stats::curTick++; // memory clock, global, for Statistics
        //       stall_reason["rw_swtich"]++;
        //   }
        //   set_high_writeq_watermark(0.8f);
          break;
        }
    }

    void serve_one_address_prefetchoram(long long forced_leaf, long addr, rs* myrs, posmap_and_stash &pos_st, long long &clks, long long cnt, map<string, long long>& stall_reason, long long &reads, long long &writes, bool print_flag=false){
            
        bool stall = false, end = false;
        Request::Type type = Request::Type::READ;
        map<int, int> latencies;
        auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

        Request req(addr, type, read_complete);

        Request::Type prev_type = Request::Type::READ;
        
        long long random_block = (forced_leaf > 0) ? forced_leaf : rand() % pos_st.total_num_blocks;
        std::vector<long> pathoram_addr_vec;
        for(int i = myrs->lowest_uncached_lvl; i < myrs->num_of_lvls; i++){
          long long node_id = myrs->P(random_block, i, myrs->num_of_lvls);
          for(int j = 0; j < PBUCKET; j++){
            pathoram_addr_vec.push_back(64 * (node_id * PBUCKET + j));
          }
        }

        while(true){
          int index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::READ;
              stall = !send(req);
              if (!stall){
                  if (req.type == Request::Type::READ) reads++;
                  else if (req.type == Request::Type::WRITE) writes++;
                  index++;
              }
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          set_high_writeq_watermark(0.0f);
          while(pending_requests()){
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
              stall_reason["rw_swtich"]++;
          }
          set_high_writeq_watermark(0.8f);
          index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::WRITE;
              stall = !send(req);
              if (!stall){
                  if (req.type == Request::Type::READ) reads++;
                  else if (req.type == Request::Type::WRITE) writes++;
                  index++;
              }
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
        //   set_high_writeq_watermark(0.0f);
        //   while(pending_requests()){
        //       tick();
        //       clks ++;
        //       Stats::curTick++; // memory clock, global, for Statistics
        //       stall_reason["rw_swtich"]++;
        //   }
        //   set_high_writeq_watermark(0.8f);
          break;
        }
    }

    void serve_one_address_pathoram(long addr, rs* myrs, posmap_and_stash &pos_st, long long &clks, long long cnt, map<string, long long>& stall_reason, long long &reads, long long &writes, bool print_flag=false){
            
        bool stall = false, end = false;
        Request::Type type = Request::Type::READ;
        map<int, int> latencies;
        auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

        Request req(addr, type, read_complete);

        Request::Type prev_type = Request::Type::READ;
        
        long long random_block = rand() % pos_st.total_num_blocks;
        std::vector<long> pathoram_addr_vec;
        for(int i = myrs->lowest_uncached_lvl; i < myrs->num_of_lvls; i++){
          long long node_id = myrs->P(random_block, i, myrs->num_of_lvls);
          for(int j = 0; j < PBUCKET; j++){
            pathoram_addr_vec.push_back(64 * (node_id * PBUCKET + j));
          }
        }

        while(true){
          int index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::READ;
              stall = !send(req);
              if (!stall){
                  if (req.type == Request::Type::READ) reads++;
                  else if (req.type == Request::Type::WRITE) writes++;
                  index++;
              }
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
          set_high_writeq_watermark(0.0f);
          while(pending_requests()){
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
              stall_reason["rw_swtich"]++;
          }
          set_high_writeq_watermark(0.8f);
          index = 0;
          for(; index < pathoram_addr_vec.size();){
              req.addr = (long) pathoram_addr_vec[index];
              req.type = Request::Type::WRITE;
              stall = !send(req);
              if (!stall){
                  if (req.type == Request::Type::READ) reads++;
                  else if (req.type == Request::Type::WRITE) writes++;
                  index++;
              }
              tick();
              clks ++;
              Stats::curTick++; // memory clock, global, for Statistics
          }
        //   set_high_writeq_watermark(0.0f);
        //   while(pending_requests()){
        //       tick();
        //       clks ++;
        //       Stats::curTick++; // memory clock, global, for Statistics
        //       stall_reason["rw_swtich"]++;
        //   }
        //   set_high_writeq_watermark(0.8f);
          break;
        }
    }

    void serve_one_address(long addr, rs* myrs, posmap_and_stash &pos_st, long long &clks, long long cnt, map<string, long long>& stall_reason, long long &reads, long long &writes, bool print_flag=false){
            
        bool stall = false, end = false;
        Request::Type type = Request::Type::READ;
        map<int, int> latencies;
        auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

        Request req(addr, type, read_complete);

        Request::Type prev_type = Request::Type::READ;
        bool prev_req_meta = true;
        while(true){
            int replay = 0;
            // cout << "Stash size: " << myrs->stash.size() << endl;
            while(myrs->stash.size() >= STASH_MAITANENCE * myrs->total_num_blocks / myrs->num_of_ways){
            // if(pos_st.rw_counter % (A + 1) == 0){
                myrs->stash_violation++;
                cout << "Stash size is : " << myrs->stash.size() << endl;
                cout << "Stash size is above or equal to maintanence threshold: " << myrs->stash.size() << " >= " <<  STASH_MAITANENCE << endl;
                cout << "Inserting dummy combination" << endl;
            // }
            int success = pos_st.issue_pending_task(-1, print_flag);  // 0xdeadbeef is not a multiple of 64, it just serves as a dummy read from the address space
            if(success == 0){
                tick();
                clks ++;
        if(clks % 1000000 == 0) {cout << "Clk4 @ " << std::dec << clks << " Finished " << std::dec << cnt << " instructions " << endl;}
                if(clks % 1000 == 0){myrs->mem_q_sample_times++;myrs->mem_q_total+=pending_requests();}
                Stats::curTick++; // memory clock, global, for Statistics
                stall_reason["rs_hazard"]++;
                // break;
                continue;
            }
            
            int index = 0;
            for(; index < myrs->ramualtor_input_vec.size();){
                ramulator_packet oram_packet = myrs->ramualtor_input_vec[index];
                req.addr = (long) oram_packet.address;
                req.type = oram_packet.pull ? Request::Type::READ : Request::Type::WRITE;
                req.metadata = oram_packet.metadata;
                req.name = oram_packet.name;
                if((prev_type != req.type && req.type == Request::Type::WRITE) ||
                    (prev_req_meta != req.metadata && req.metadata == false)
                ){
                    // cout << "Switch type with memory request pending? " << pending_requests() << endl;
                    set_high_writeq_watermark(0.0f);
                    while(pending_requests()){
                        tick();
                        clks ++;
                        if(clks % 1000 == 0){myrs->mem_q_sample_times++;myrs->mem_q_total+=pending_requests();}
        if(clks % 1000000 == 0) {cout << "Clk5 @ " << std::dec << clks << " Finished " << std::dec << cnt << " instructions " << endl;}
                        Stats::curTick++; // memory clock, global, for Statistics
                        
                        stall_reason["rw_swtich"]++;
                    }
                    set_high_writeq_watermark(0.8f);
                }
                prev_type = req.type;
                prev_req_meta = req.metadata;
                stall = !send(req);
                if (!stall){
                    if (req.type == Request::Type::READ) reads++;
                    else if (req.type == Request::Type::WRITE) writes++;
                    index++;
                    stall_reason["normal"]++;
                }
                else{
                    stall_reason["mc_hazard"]++;
                }
                tick();
                clks ++;
                if(clks % 1000 == 0){myrs->mem_q_sample_times++;myrs->mem_q_total+=pending_requests();}
        // if(clks % 1000000 == 0) {myrs->print_allline(); cout << "Clk3 @ " << std::dec << clks << " Finished " << std::dec << cnt << " instructions " << endl;}
                Stats::curTick++; // memory clock, global, for Statistics
            }

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

            int success = pos_st.issue_pending_task(addr, print_flag);
            // cout << "ORAM access: 0x " << std::hex << addr << std::dec << "; success is: " << success << endl;
            if(success == 0){
            tick();
            clks ++;
            if(clks % 1000 == 0){myrs->mem_q_sample_times++;myrs->mem_q_total+=pending_requests();}
        if(clks % 1000000 == 0) {cout << "Clk1 @ " << std::dec << clks << " Finished " << std::dec << cnt << " instructions " << endl;}
            Stats::curTick++; // memory clock, global, for Statistics
            stall_reason["rs_hazard"]++;
            // break;
            continue;
            }
            // cout << "Req gives " << myrs->ramualtor_input_vec.size() << " dram reqs" << endl;
            
            int index = 0;
            for(; index < myrs->ramualtor_input_vec.size();){
                ramulator_packet oram_packet = myrs->ramualtor_input_vec[index];
                req.addr = (long) oram_packet.address;
                req.type = oram_packet.pull ? Request::Type::READ : Request::Type::WRITE;
                req.metadata = oram_packet.metadata;
                req.name = oram_packet.name;
                if((prev_type != req.type && req.type == Request::Type::WRITE) ||
                    (prev_req_meta != req.metadata && req.metadata == false)
                ){
                // cout << "Switch type with memory request pending? " << pending_requests() << endl;
                set_high_writeq_watermark(0.0f);
                while(pending_requests()){
                    tick();
                    clks ++;
            if(clks % 1000 == 0){myrs->mem_q_sample_times++;myrs->mem_q_total+=pending_requests();}
        if(clks % 1000000 == 0) {cout << "Clk2 @ " << std::dec << clks << " Finished " << std::dec << cnt << " instructions " << endl;}
                    Stats::curTick++; // memory clock, global, for Statistics
                    
                    stall_reason["rw_swtich"]++;
                }
                set_high_writeq_watermark(0.8f);
                }
                prev_type = req.type;
                prev_req_meta = req.metadata;
                stall = !send(req);
                if (!stall){
                    if (req.type == Request::Type::READ) reads++;
                    else if (req.type == Request::Type::WRITE) writes++;
                    index++;
                    // cout << "Issuing addr: " << std::hex << req.addr << " is metadata? " << req.metadata << std::dec << endl;
                stall_reason["normal"]++;
                }
                else{
                stall_reason["mc_hazard"]++;
                }
                tick();
                clks ++;
            if(clks % 1000 == 0){myrs->mem_q_sample_times++;myrs->mem_q_total+=pending_requests();}
        if(clks % 1000000 == 0) {cout << "Clk3 @ " << std::dec << clks << " Finished " << std::dec << cnt << " instructions " << endl;}
                Stats::curTick++; // memory clock, global, for Statistics
            }
            break;
        }
        
        set_high_writeq_watermark(0.0f);
        while(pending_requests()){
            tick();
            clks ++;
            if(clks % 1000 == 0){myrs->mem_q_sample_times++;myrs->mem_q_total+=pending_requests();}
        if(clks % 1000000 == 0) {cout << "Clk-1 @ " << std::dec << clks << " Finished " << std::dec << cnt << " instructions " << endl;}
            Stats::curTick++; // memory clock, global, for Statistics
            
            stall_reason["rw_swtich"]++;
        }
        set_high_writeq_watermark(0.8f);
    }

    void tick()
    {
        ++num_dram_cycles;
        int cur_que_req_num = 0;
        int cur_que_readreq_num = 0;
        int cur_que_writereq_num = 0;
        for (auto ctrl : ctrls) {
          cur_que_req_num += ctrl->readq.size() + ctrl->writeq.size() + ctrl->pending.size();
          cur_que_readreq_num += ctrl->readq.size() + ctrl->pending.size();
          cur_que_writereq_num += ctrl->writeq.size();
        }
        in_queue_req_num_sum += cur_que_req_num;
        in_queue_read_req_num_sum += cur_que_readreq_num;
        in_queue_write_req_num_sum += cur_que_writereq_num;

        bool is_active = false;
        for (auto ctrl : ctrls) {
          is_active = is_active || ctrl->is_active();
          ctrl->tick();
        }
        if (is_active) {
          ramulator_active_cycles++;
        }
    }

    bool send(Request req)
    {
        req.addr_vec.resize(addr_bits.size());
        long addr = req.addr;
        int coreid = req.coreid;

        // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
        clear_lower_bits(addr, tx_bits);

        if (use_mapping_file){
            apply_mapping(addr, req.addr_vec);
        }
        else {
            switch(int(type)){
                case int(Type::ChRaBaRoCo):
                    for (int i = addr_bits.size() - 1; i >= 0; i--)
                        req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
                    break;
                case int(Type::RoBaRaCoCh):
                    req.addr_vec[0] = slice_lower_bits(addr, addr_bits[0]);
                    req.addr_vec[addr_bits.size() - 1] = slice_lower_bits(addr, addr_bits[addr_bits.size() - 1]);
                    for (int i = 1; i <= int(T::Level::Row); i++)
                        req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
                    break;
                default:
                    assert(false);
            }
        }

        if(ctrls[req.addr_vec[0]]->enqueue(req)) {
            // tally stats here to avoid double counting for requests that aren't enqueued
            ++num_incoming_requests;
            if (req.type == Request::Type::READ) {
              ++num_read_requests[coreid];
              ++incoming_read_reqs_per_channel[req.addr_vec[int(T::Level::Channel)]];
            }
            if (req.type == Request::Type::WRITE) {
              ++num_write_requests[coreid];
            }
            ++incoming_requests_per_channel[req.addr_vec[int(T::Level::Channel)]];
            return true;
        }

        return false;
    }
    
    void init_mapping_with_file(string filename){
        ifstream file(filename);
        assert(file.good() && "Bad mapping file");
        // possible line types are:
        // 0. Empty line
        // 1. Direct bit assignment   : component N   = x
        // 2. Direct range assignment : component N:M = x:y
        // 3. XOR bit assignment      : component N   = x y z ...
        // 4. Comment line            : # comment here
        string line;
        char delim[] = " \t";
        while (getline(file, line)) {
            short capture_flags = 0;
            int level = -1;
            int target_bit = -1, target_bit2 = -1;
            int source_bit = -1, source_bit2 = -1;
            // cout << "Processing: " << line << endl;
            bool is_range = false;
            while (true) { // process next word
                size_t start = line.find_first_not_of(delim);
                if (start == string::npos) // no more words
                    break;
                size_t end = line.find_first_of(delim, start);
                string word = line.substr(start, end - start);
                
                if (word.at(0) == '#')// starting a comment
                    break;
                
                size_t col_index;
                int source_min, target_min, target_max;
                switch (capture_flags){
                    case 0: // capturing the component name
                        // fetch component level from channel spec
                        for (int i = 0; i < int(T::Level::MAX); i++)
                            if (word.find(T::level_str[i]) != string::npos) {
                                level = i;
                                capture_flags ++;
                            }
                        break;

                    case 1: // capturing target bit(s)
                        col_index = word.find(":");
                        if ( col_index != string::npos ){
                            target_bit2 = stoi(word.substr(col_index+1));
                            word = word.substr(0,col_index);
                            is_range = true;
                        }
                        target_bit = stoi(word);
                        capture_flags ++;
                        break;

                    case 2: //this should be the delimiter
                        assert(word.find("=") != string::npos);
                        capture_flags ++;
                        break;

                    case 3:
                        if (is_range){
                            col_index = word.find(":");
                            source_bit  = stoi(word.substr(0,col_index));
                            source_bit2 = stoi(word.substr(col_index+1));
                            assert(source_bit2 - source_bit == target_bit2 - target_bit);
                            source_min = min(source_bit, source_bit2);
                            target_min = min(target_bit, target_bit2);
                            target_max = max(target_bit, target_bit2);
                            while (target_min <= target_max){
                                mapping_scheme[level][target_min].push_back(source_min);
                                // cout << target_min << " <- " << source_min << endl;
                                source_min ++;
                                target_min ++;
                            }
                        }
                        else {
                            source_bit = stoi(word);
                            mapping_scheme[level][target_bit].push_back(source_bit);
                        }
                }
                if (end == string::npos) { // this is the last word
                    break;
                }
                line = line.substr(end);
            }
        }
        if (dump_mapping)
            dump_mapping_scheme();
    }
    
    void dump_mapping_scheme(){
        cout << "Mapping Scheme: " << endl;
        for (MapScheme::iterator mapit = mapping_scheme.begin(); mapit != mapping_scheme.end(); mapit++)
        {
            int level = mapit->first;
            for (MapSchemeEntry::iterator entit = mapit->second.begin(); entit != mapit->second.end(); entit++){
                cout << T::level_str[level] << "[" << entit->first << "] := ";
                cout << "PhysicalAddress[" << *(entit->second.begin()) << "]";
                entit->second.erase(entit->second.begin());
                for (MapSrcVector::iterator it = entit->second.begin() ; it != entit->second.end(); it ++)
                    cout << " xor PhysicalAddress[" << *it << "]";
                cout << endl;
            }
        }
    }
    
    void apply_mapping(long addr, std::vector<int>& addr_vec){
        int *sz = spec->org_entry.count;
        int addr_total_bits = sizeof(addr_vec)*8;
        int addr_bits [int(T::Level::MAX)];
        for (int i = 0 ; i < int(T::Level::MAX) ; i ++)
        {
            if ( i != int(T::Level::Row))
            {
                addr_bits[i] = calc_log2(sz[i]);
                addr_total_bits -= addr_bits[i];
            }
        }
        // Row address is an integer.
        addr_bits[int(T::Level::Row)] = min((int)sizeof(int)*8, max(addr_total_bits, calc_log2(sz[int(T::Level::Row)])));

        // printf("Address: %lx => ",addr);
        for (unsigned int lvl = 0; lvl < int(T::Level::MAX); lvl++)
        {
            unsigned int lvl_bits = addr_bits[lvl];
            addr_vec[lvl] = 0;
            for (unsigned int bitindex = 0 ; bitindex < lvl_bits ; bitindex++){
                bool bitvalue = false;
                for (MapSrcVector::iterator it = mapping_scheme[lvl][bitindex].begin() ;
                    it != mapping_scheme[lvl][bitindex].end(); it ++)
                {
                    bitvalue = bitvalue xor get_bit_at(addr, *it);
                }
                addr_vec[lvl] |= (bitvalue << bitindex);
            }
            // printf("%s: %x, ",T::level_str[lvl].c_str(),addr_vec[lvl]);
        }
        // printf("\n");
    }

    int pending_requests()
    {
        int reqs = 0;
        for (auto ctrl: ctrls)
            reqs += ctrl->readq.size() + ctrl->writeq.size() + ctrl->otherq.size() + ctrl->actq.size() + ctrl->pending.size();
        return reqs;
    }

    void set_high_writeq_watermark(const float watermark) {
        for (auto ctrl: ctrls)
            ctrl->set_high_writeq_watermark(watermark);
    }

    void set_low_writeq_watermark(const float watermark) {
    for (auto ctrl: ctrls)
        ctrl->set_low_writeq_watermark(watermark);
    }

    void finish(void) {
      dram_capacity = max_address;
      int *sz = spec->org_entry.count;
      maximum_bandwidth = spec->speed_entry.rate * 1e6 * spec->channel_width * sz[int(T::Level::Channel)] / 8;
      long dram_cycles = num_dram_cycles.value();
      for (auto ctrl : ctrls) {
        long read_req = long(incoming_read_reqs_per_channel[ctrl->channel->id].value());
        ctrl->finish(read_req, dram_cycles);
      }

      // finalize average queueing requests
      in_queue_req_num_avg = in_queue_req_num_sum.value() / dram_cycles;
      in_queue_read_req_num_avg = in_queue_read_req_num_sum.value() / dram_cycles;
      in_queue_write_req_num_avg = in_queue_write_req_num_sum.value() / dram_cycles;
    }

    long page_allocator(long addr, int coreid) {
        long virtual_page_number = addr >> 12;

        switch(int(translation)) {
            case int(Translation::None): {
              return addr;
            }
            case int(Translation::Random): {
                auto target = make_pair(coreid, virtual_page_number);
                if(page_translation.find(target) == page_translation.end()) {
                    // page doesn't exist, so assign a new page
                    // make sure there are physical pages left to be assigned

                    // if physical page doesn't remain, replace a previous assigned
                    // physical page.
                    if (!free_physical_pages_remaining) {
                      physical_page_replacement++;
                      long phys_page_to_read = lrand() % free_physical_pages.size();
                      assert(free_physical_pages[phys_page_to_read] != -1);
                      page_translation[target] = phys_page_to_read;
                    } else {
                        // assign a new page
                        long phys_page_to_read = lrand() % free_physical_pages.size();
                        // if the randomly-selected page was already assigned
                        if(free_physical_pages[phys_page_to_read] != -1) {
                            long starting_page_of_search = phys_page_to_read;

                            do {
                                // iterate through the list until we find a free page
                                // TODO: does this introduce serious non-randomness?
                                ++phys_page_to_read;
                                phys_page_to_read %= free_physical_pages.size();
                            }
                            while((phys_page_to_read != starting_page_of_search) && free_physical_pages[phys_page_to_read] != -1);
                        }

                        assert(free_physical_pages[phys_page_to_read] == -1);

                        page_translation[target] = phys_page_to_read;
                        free_physical_pages[phys_page_to_read] = coreid;
                        --free_physical_pages_remaining;
                    }
                }

                // SAUGATA TODO: page size should not always be fixed to 4KB
                return (page_translation[target] << 12) | (addr & ((1 << 12) - 1));
            }
            default:
                assert(false);
        }

    }

private:

    int calc_log2(int val){
        int n = 0;
        while ((val >>= 1))
            n ++;
        return n;
    }
    int slice_lower_bits(long& addr, int bits)
    {
        int lbits = addr & ((1<<bits) - 1);
        addr >>= bits;
        return lbits;
    }
    bool get_bit_at(long addr, int bit)
    {
        return (((addr >> bit) & 1) == 1);
    }
    void clear_lower_bits(long& addr, int bits)
    {
        addr >>= bits;
    }
    long lrand(void) {
        if(sizeof(int) < sizeof(long)) {
            return static_cast<long>(rand()) << (sizeof(int) * 8) | rand();
        }

        return rand();
    }
};

} /*namespace ramulator*/

#endif /*__MEMORY_H*/
