



# perf = []
# ks = []
# for i in range(0, 27, 1):
#     ks.append(2 ** i)
ks = ["mcf",	"lbm",	"pagerank",	"graphsearch",	"criteo",	"dblp",	"orca"	,"redis",	"stream_addr",	"random_addr"]

for k in ks:
    f = open("results_all/" + "oram_" + k + ".log", "r")

    for line in f.readlines():
        # if "Max stash size is:" in line:
        #     stash.append(line.split()[-1])
        if "The final time in ns is:" in line:
            # perf.append(float(line.split()[-2]))
            print(float(line.split()[-2]),end=' ')
        # print(line)

# assert(len(stash) == len(perf))
# print("DRAM base time:", perf[-1])
print()
# scale = perf[0]
# for i in range(0, len(perf)):
#     perf[i] /= scale

# for i in range(0, len(perf)):
#     print(ks[i], ' ', perf[i], ' vs. Base:', perf[i] / perf[0])
    