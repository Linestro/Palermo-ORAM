import numpy
import matplotlib
import matplotlib.pyplot as plt 

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42



workloads=["mcf", "lbm", "pagerank", "graphsearch", "criteo", "dblp", "orca", "redis", "stream_addr", "random_addr"]
write_file = "fig9.txt"

pathoram = []
for workload in workloads:
    t_path = None
    path = '../other_oram_results/pathoram_' + workload + ".log"
    try:
        with open(path, 'r') as file:
            for line in file.readlines():
                if "The final time in ns is" in line:
                    t_path = float(line.split()[-2])
    except FileNotFoundError:
        print(f"Error: The file '{path}' was not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    pathoram.append(t_path)

ringoram = []
for workload in workloads:
    t_ring = None
    path = '../ringoram_results/ringoram_' + workload + "_prefetch_1.log"
    try:
        with open(path, 'r') as file:
            for line in file.readlines():
                if "The final time in ns is" in line:
                    t_ring = float(line.split()[-2])
    except FileNotFoundError:
        print(f"Error: The file '{path}' was not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    ringoram.append(t_ring)

pageoram = []
for workload in workloads:
    t_page = None
    path = '../other_oram_results/pageoram_' + workload + ".log"
    try:
        with open(path, 'r') as file:
            for line in file.readlines():
                if "The final time in ns is" in line:
                    t_page = float(line.split()[-2])
    except FileNotFoundError:
        print(f"Error: The file '{path}' was not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    pageoram.append(t_page)

iroram = []
for workload in workloads:
    t_ir = None
    path = '../other_oram_results/iroram_' + workload + ".log"
    try:
        with open(path, 'r') as file:
            for line in file.readlines():
                if "The final time in ns is" in line:
                    t_ir = float(line.split()[-2])
    except FileNotFoundError:
        print(f"Error: The file '{path}' was not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    iroram.append(t_ir)

proram = []
prefetch_lens = [1, 2, 4, 8]
dummy_ratio_offset = {1:1, 2:1, 4:0.7937, 8:0.3712}  # offset = 1 - dummy_raio; dummy_ratio = {0,0,0.2063,0.6288} for prefetch len {1,2,4,8}
for workload in workloads:
    t_best = float('inf')
    t_base = None
    for prefetch_len in prefetch_lens:
        path = '../proram_results/pathoram_' + workload + "_prefetch_" + str(prefetch_len) + ".log"
        try:
            with open(path, 'r') as file:
                for line in file.readlines():
                    if "The final time in ns is" in line:
                        t_finish = float(line.split()[-2]) / dummy_ratio_offset[prefetch_len]
                        if t_finish < t_best:
                            t_best = t_finish
        except FileNotFoundError:
            print(f"Error: The file '{path}' was not found.")
        except Exception as e:
            print(f"An error occurred: {e}")
    proram.append(t_best)


palermo_sw = []
for workload in workloads:
    t_sw = None
    path = '../palermo_sw_results/palermo_sw_' + workload + "_prefetch_1.log"
    try:
        with open(path, 'r') as file:
            for line in file.readlines():
                if "The final time in ns is" in line:
                    t_sw = float(line.split()[-2])
    except FileNotFoundError:
        print(f"Error: The file '{path}' was not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    palermo_sw.append(t_sw)

palermo_base = []
palermo_prefetch = []
prefetch_lens = [1, 2, 4]
for workload in workloads:
    t_best = float('inf')
    t_base = None
    for prefetch_len in prefetch_lens:
        path = '../results/oram_' + workload + "_prefetch_" + str(prefetch_len) + ".log"
        try:
            with open(path, 'r') as file:
                for line in file.readlines():
                    if "The final time in ns is" in line:
                        t_finish = float(line.split()[-2])
                        if t_finish < t_best:
                            t_best = t_finish
                        if prefetch_len == 1:
                            t_base = t_finish
        except FileNotFoundError:
            print(f"Error: The file '{path}' was not found.")
        except Exception as e:
            print(f"An error occurred: {e}")
    palermo_base.append(t_base)
    palermo_prefetch.append(t_best)

ringoram = ([round(x / y, 3) for x,y in zip(pathoram, ringoram)])
pageoram = ([round(x / y, 3) for x,y in zip(pathoram, pageoram)])
proram = ([round(x / y, 3) for x,y in zip(pathoram, proram)])
iroram = ([round(x / y, 3) for x,y in zip(pathoram, iroram)])
palermo_sw = ([round(x / y, 3) for x,y in zip(pathoram, palermo_sw)])
palermo_base = ([round(x / y, 3) for x,y in zip(pathoram, palermo_base)])
palermo_prefetch = ([round(x / y, 3) for x,y in zip(pathoram, palermo_prefetch)])
pathoram = ([round(x / y, 3) for x,y in zip(pathoram, pathoram)])

import statistics
ringoram.append(statistics.geometric_mean(ringoram))
pageoram.append(statistics.geometric_mean(pageoram))
proram.append(statistics.geometric_mean(proram))
iroram.append(statistics.geometric_mean(iroram))
palermo_sw.append(statistics.geometric_mean(palermo_sw))
palermo_base.append(statistics.geometric_mean(palermo_base))
palermo_prefetch.append(statistics.geometric_mean(palermo_prefetch))
pathoram.append(statistics.geometric_mean(pathoram))

workload_abbv = ["mcf", "lbm", "pr", "motif", "rm1", "rm2", "llm", "redis", "stream", "random", "gm"];

f = open(write_file, "w")
for i in range(0, len(workload_abbv)):
    f.write(workload_abbv[i] + '\t')
    f.write(str(pathoram[i]) + '\t')
    f.write(str(ringoram[i]) + '\t')
    f.write(str(pageoram[i]) + '\t')
    f.write(str(proram[i]) + '\t')
    f.write(str(iroram[i]) + '\t')
    f.write(str(palermo_sw[i]) + '\t')
    f.write(str(palermo_base[i]) + '\t')
    f.write(str(palermo_prefetch[i]) + '\n')
f.close()
file = "fig9.txt"

WIN1 = []
WIN2 = []
WIN3 = []
WIN4 = []
WIN5 = []
WIN6 = []
WIN7 = []
WIN8 = []

kernels = []

with open(file) as fp:
    for ln in fp:
        if "\n" in ln:
            ln = ln[:-1]
        ln_split = ln.split("\t")
        print(ln_split)
        kernels.append(ln_split[0])
        WIN1.append(float(ln_split[1]))
        WIN2.append(float(ln_split[2]))
        WIN3.append(float(ln_split[3]))
        WIN4.append(float(ln_split[4]))
        WIN5.append(float(ln_split[5]))
        WIN6.append(float(ln_split[6]))
        WIN7.append(float(ln_split[7]))
        WIN8.append(float(ln_split[8]))


ind = numpy.arange(len(kernels))*2.5
# ind[-2] += 0.4
ind[-1] += 0.6
ax1 = plt.gca()
width = 0.25
p00 = plt.bar(ind, WIN1, width,  align='center',
              color='#feebe2', edgecolor="black", label="PathORAM")
p0 = plt.bar(ind+width, WIN2, width,  align='center',
             color='#fa9fb5', edgecolor="black", label="RingORAM")
p0 = plt.bar(ind+2*width, WIN3, width,  align='center',
             color='#c51b8a', edgecolor="black", label="PageORAM")
p0 = plt.bar(ind+3*width, WIN4, width,  align='center',
             color='#ff7f00', edgecolor="black", label="PrORAM")
p1 = plt.bar(ind+4*width, WIN5, width, align='center',
             color='#f768a1', edgecolor="black", label="IR-ORAM")
p2 = plt.bar(ind+5*width, WIN6, width, align='center',
             color='#b2df8a', edgecolor="black", label="Palermo-SW")
p3 = plt.bar(ind+6*width, WIN7, width, align='center',
             color='#8c96c6', edgecolor="black", label="Palermo")
p4 = plt.bar(ind+7*width, WIN8, width, align='center',
             color='#35978f', edgecolor="black", label="Palermo+Prefetch")

graph_font_size = 25
font0 = matplotlib.font_manager.FontProperties()
font0.set_weight('bold')
plt.xticks(ind+3.5*width, kernels, fontsize=graph_font_size-5, rotation=0)


plt.text(ind[-1]-width+0.1, WIN1[-1]+0.25,
         "{:.1f}x".format(WIN1[-1]), fontsize=graph_font_size-12,rotation=90, weight='bold')
plt.text(ind[-1]+width-0.15, WIN2[-1]+0.25,
         "{:.1f}x".format(WIN2[-1]), fontsize=graph_font_size-12, rotation=90, weight='bold')
plt.text(ind[-1]+2*width-0.15, WIN3[-1]+0.25,
         "{:.1f}x".format(WIN3[-1]), fontsize=graph_font_size-12, rotation=90, weight='bold')
plt.text(ind[-1]+3*width-0.15, WIN4[-1]+0.25,
         "{:.1f}x".format(WIN4[-1]), fontsize=graph_font_size-12, rotation=90, weight='bold')
plt.text(ind[-1]+4*width-0.12, WIN5[-1]+0.25,
         "{:.1f}x".format(WIN5[-1]), fontsize=graph_font_size-12, rotation=90, weight='bold')
plt.text(ind[-1]+5*width-0.12, WIN6[-1]+0.25,
         "{:.1f}x".format(WIN6[-1]), fontsize=graph_font_size-12, rotation=90, weight='bold')
plt.text(ind[-1]+6*width-0.11, WIN7[-1]+0.27,
         "{:.1f}x".format(WIN7[-1]), fontsize=graph_font_size-12, rotation=90, weight='bold')
plt.text(ind[-1]+7*width-0.11, WIN8[-1]+0.27,
         "{:.1f}x".format(WIN8[-1]), fontsize=graph_font_size-12, rotation=90, weight='bold')




data_split_place1 = (len(kernels) - 1) * 2.5 - width/2 + 0.
plt.axvline(x=data_split_place1, color='black', linestyle='--')

plt.legend(fontsize=graph_font_size-9, ncol=8, loc="lower center", bbox_to_anchor=(0.495, 0.965))

ax1.yaxis.set_ticks(np.arange(0, 100, 2))
for i in range(0, 12):
    if i == 0:
        continue
    ax1.axhline(0+0.5*i, color='grey', linestyle='--')
ax1.set_ylim(0, 5.4)
for axis in [ax1.yaxis]:
    axis.set_major_locator(matplotlib.ticker.MaxNLocator(integer=True))
ax1.set_xlim(-0.5, len(kernels) * 2.5 + 0.4)

font0.set_size(graph_font_size)
plt.xlabel("Datasets", fontsize=graph_font_size-8, font_properties=font0)
plt.ylabel("End-to-end\nSpeedup (x)", fontsize=graph_font_size-8,
           font_properties=font0)
plt.yticks(fontsize=graph_font_size)
fig = matplotlib.pyplot.gcf()
# fig.set_size_inches(20, 4, forward=True)
fig.set_size_inches(20, 3, forward=True)
plt.tight_layout()
fig.savefig('fig9.pdf',bbox_inches='tight')
fig.savefig('fig9.png',bbox_inches='tight')
fig.savefig('fig9.svg',bbox_inches='tight')
# plt.show()
