



perf = []
ks = []
for i in range(0, 25, 1):
    ks.append(2 ** i)


for k in ks:
    f = open("results_" + str(k) + "/" + "oram.log", "r")

    for line in f.readlines():
        # if "Max stash size is:" in line:
        #     stash.append(line.split()[-1])
        if "The final time in ns is:" in line:
            perf.append(float(line.split()[-2]))
        # print(line)

# assert(len(stash) == len(perf))
# print("DRAM base time:", perf[-1])

scale = perf[0]
for i in range(0, len(perf)):
    perf[i] /= scale

for i in range(0, len(perf)):
    print(ks[i], ' ', perf[i], ' vs. Base:', perf[i] / perf[0])
    