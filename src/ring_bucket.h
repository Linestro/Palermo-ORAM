#include <vector>
#include <iostream>
#include <functional>

#define Z 16
#define S 27
#define A 20


#define PBUCKET Z

using namespace std;

struct ring_bucket{
    public:
    long long node_id;
    vector<int> z_arr;
    vector<int> s_arr;
    int free_z_index = 0;
    int accessed_times = 0;
    ring_bucket(long long n_id){
        z_arr.clear();
        s_arr.clear();
        node_id = n_id;
        accessed_times= 0;
        std::vector<int> tmp;
        for(int i=0; i<Z+S; i++){
            tmp.push_back(i);
        }
        std::random_shuffle ( tmp.begin(), tmp.end() );
        for(int i=0; i<Z;i++){
            z_arr.push_back(tmp[i]);
        }
        for(int i=Z; i<Z+S;i++){
            s_arr.push_back(tmp[i]);
        }
    }

    int assign_z_off(){
        assert(z_arr.size());
        int ret = z_arr[free_z_index];
        free_z_index++;
        assert(s_arr.size() <= S);
        return ret;
    }

    void flush_offset(vector<int> &flushed_offset){
        for(int i = 0; i < z_arr.size(); i++){
            flushed_offset.push_back(z_arr[i]);
        }
        for(int i = 0; i < s_arr.size(); i++){
            flushed_offset.push_back(s_arr[i]);
        }
        assert(s_arr.size() <= S);
    }

    std::pair<int, bool> next_s(){
        int ret_s = s_arr.back();
        accessed_times++;
        s_arr.pop_back();
        assert(s_arr.size() <= S);
        return std::make_pair(ret_s, (accessed_times == S));
    }

    std::pair<int, bool> take_z(int intended_offset){
        int ret = -1;
        for(int i = 0; i < z_arr.size(); i++){
            if(intended_offset == z_arr[i]){
                ret = z_arr[i];
                z_arr.erase(z_arr.begin() + i);
                break;
            }
        }
        accessed_times++;
        assert(ret >= 0);
        assert(s_arr.size() <= S);
        return std::make_pair(ret, (accessed_times == S));
    }

    void print_permutation(){
        // cout << std::dec << "Node ID: " << node_id << endl;
        // cout << "Z arr: ";
        for(int i=0; i<z_arr.size();i++){
            cout << z_arr[i] << " ";
        }
        cout << endl;
        // cout << "S arr: ";
        // cout << "S arr size: " << s_arr.size() << endl;
        assert(s_arr.size() <= S);
        for(int i=0; i<s_arr.size();i++){
            cout << s_arr[i] << " ";
        }
        cout << endl;
    }
};