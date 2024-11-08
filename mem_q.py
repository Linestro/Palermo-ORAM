



perf = []
ks = []
for i in range(26, 27, 1):
    ks.append(2 ** i)

cnt = 0 
# for k in ks:
f = open("results_all/" + "oram_orca.log", "r")

ret = 0
for line in f.readlines():
    # if "Max stash size is:" in line:
    #     stash.append(line.split()[-1])
    if "Total mem q size" in line:
        print(int(line.split(':')[-1]),end=' ')
    if "mem_q_sample_times" in line:
        print(int(line.split(':')[-1]),end='\n')
    # print(line)

# assert(len(stash) == len(perf))
# print("DRAM base time:", perf[-1])

# scale = perf[0]
# for i in range(0, len(perf)):
#     perf[i] /= scale

# for i in range(0, len(perf)):
#     print(ks[i], ' ', perf[i], ' vs. Base:', perf[i] / perf[0])
    