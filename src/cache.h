#include <iostream>
#include <cmath>
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
#include <omp.h>
#include <functional>
#include <limits.h>
#include <set>
#include <math.h>
using namespace std;

// class triangle_spad{
//   public:
//     long cache_size;
//     long long overall_total = 0;
//     long long overall_hit = 0;
//     std::ofstream out;
//     void cache_conclude(){
//       out << "overall_hit: " << overall_hit << " overall_total: " << overall_total << endl;
//       out << "Cache Size " << cache_size * 1.0 / 1024 << " KB Overall Hit rate: " << overall_hit * 100.00 / overall_total << " %. " << endl;
//       out.close();
//     };
//     triangle_spad(int _size, string filename): cache_size(_size){
//       out.open(filename + ".dummyspad." + to_string(cache_size/1024) + ".report");
//       overall_total += (cache_size / 64 - 1);
//       // out << "initialized cache size of: " << cache_size << " associativity: " << set_assoc << endl;
//     };
//     void cache_access(long accessed_addr, int access_type=1);
// };


// void triangle_spad::cache_access(long access_addr, int access_type){
//   access_addr = access_addr - access_addr % 64;
//   overall_total++;
//   if(access_addr / 64 < cache_size / 64 - 1){
//     overall_hit++;
//   }
//   return;
// }

class set_cache{
  public:
    long cache_size;
    int set_assoc;
    vector<long long> cache_total_separate;
    vector<long long> cache_hit_separate;
    long long cache_total = 0;
    long long cache_hit = 0;

    long long usefulness_total = 0;
    long long usefulness_hit = 0;

    long long overall_total = 0;
    long long overall_hit = 0;
    std::ofstream out;
    map<long, vector<int>> track_usefuleness;
    map<long, bool> temp_stream_usefulness;
    
    std::vector<std::vector<std::pair<long, int>> > caches;
    void print_cache(){
      cout << cache_size << endl;
      cout << set_assoc << endl;
    }
    
    void cache_conclude(){
      cout << "overall_hit: " << overall_hit << " overall_total: " << overall_total << endl;
      cout << "Cache Size " << cache_size * 1.0 / 1024 << " KB Overall Hit rate: " << overall_hit * 100.00 / overall_total << " %. " << endl;
    }

    set_cache(int _size, int _assoc): cache_size(_size), set_assoc(_assoc){
      cache_total_separate.resize(3, 0);
      cache_hit_separate.resize(3, 0);
      for(int i = 0; i < cache_size / 64 / set_assoc; i++){
        std::vector<std::pair<long, int>> tmp;
        tmp.resize(set_assoc, make_pair(0, -1));
        caches.push_back(tmp);
      }
    //   out.open(filename + "." + to_string(cache_size/1024) + ".report");
      // out << "initialized cache size of: " << cache_size << " associativity: " << set_assoc << endl;
    }

    
    bool cache_access(long access_addr, int access_type=1){
    access_addr = access_addr - access_addr % 64;
    int set_id = (access_addr / 64) % caches.size();
    auto it = find(caches[set_id].begin(), caches[set_id].end(), make_pair(access_addr, access_type));
    cache_total++;
    overall_total++;
    cache_total_separate[access_type]++;
    if(it != caches[set_id].end()){
        caches[set_id].erase(it);
        caches[set_id].insert(caches[set_id].begin(), make_pair(access_addr, access_type));
        cache_hit++;
        overall_hit++;
        cache_hit_separate[access_type]++;

        //   if(access_type == 1){
        //     track_usefuleness[access_addr][(orig_addr % 64) / 4] = 1;
        //   }
        return true;
    }
    else{
        caches[set_id].insert(caches[set_id].begin(), make_pair(access_addr, access_type));
        
        //   if(access_type == 1){
        //     track_usefuleness[access_addr] = vector<int>();
        //     track_usefuleness[access_addr].resize(16, 0);
        //     track_usefuleness[access_addr][(orig_addr % 64) / 4] = 1;
        //     temp_stream_usefulness[access_addr] = true;
        //   }

        if(int(caches[set_id].size()) >= set_assoc){
            //   if(access_type == 1){
            //     long evict_addr = caches[set_id][caches[set_id].size() - 1].first;
            //     track_usefuleness.erase(evict_addr);
            //     usefulness_hit += std::count(track_usefuleness[evict_addr].begin(), track_usefuleness[evict_addr].end(), 1);
            //     usefulness_total += track_usefuleness[evict_addr].size();
            //     temp_stream_usefulness.erase(evict_addr);
            //   }
            
            caches[set_id].pop_back();
        }
        return false;
        }
        assert(0);
        return false;
    }

};
