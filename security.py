


import random
perf = []
ks = []
for i in range(26, 27, 1):
    ks.append(2 ** i)

cnt = 0 
# for k in ks:
f = open("results_all/" + "oram_redis.log", "r")
# f = open("special_stream.log", "r")

ret = 0
latency = []
mask = [False] * 1000000

for line in f.readlines():
    # if "Max stash size is:" in line:
    #     stash.append(line.split()[-1])

    # if len(line.split()) == 1 and line.split()[0].isnumeric():
    #     if int(line.split()[0]) > 2500 and int(line.split()[0]) < 10000:
    #         if cnt > 20:
    #             print(int(line.split()[0]),end='\n')
    #             ret += 1
    #     cnt += 1
    # if ret > 100:
    #     exit(0)
        
    if len(line.split()) == 1 and line.split()[0].isnumeric():
        for i in range(0, 20):
        # print(line)
            latency.append(float(line.split()[0]))
    

    if "HitID" in line:
        hit_id = line.split(",")[1:-1]
        for id in hit_id:
            mask[int(id)] = True

def median(arr):
    arr.sort()
    n = len(arr)
    mid = n // 2
    if n % 2 == 0:
        return (arr[mid - 1] + arr[mid]) / 2
    else:
        return arr[mid]

print(len(latency))
mean = median(latency) # sum(latency) / len(latency)

# print(mean)

in_stash_long = 0
in_stash_short = 0
out_stash_long = 0
out_stash_short = 0


short = 0
long = 0

for i in range(0, len(latency)):
    if latency[i] < mean:
        short += 1
        if mask[i]:
            in_stash_short += 1
        else:
            out_stash_short += 1
    else:        
        long += 1
        if mask[i]:
            in_stash_long += 1
        else:
            out_stash_long += 1

# print(in_stash_short, out_stash_short, in_stash_long, out_stash_long)

# print(short, long)

p1 = in_stash_short / (in_stash_short + in_stash_long)
p2 = out_stash_short / (out_stash_short + out_stash_long)

import math
print(p1)
print(p2)


m = p1/2*math.log2(2*p1/(p1+p2)) + \
    p2/2*math.log2(2*p2/(p1+p2)) \
   + (1-p1)/2*math.log2(2*(1-p1)/(2-p1-p2)) + \
      (1-p2)/2*math.log2(2*(1-p2)/(2-p1-p2))
print('m=', m)