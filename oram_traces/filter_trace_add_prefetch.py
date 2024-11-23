





import argparse
import sys
import re
import numpy
import os
import math
import glob
from collections import defaultdict
import matplotlib
import matplotlib.pyplot as plt
import tqdm

parser = argparse.ArgumentParser()
parser.add_argument('-i', required=True)
parser.add_argument('-e', required=True)
parser.add_argument('-l', required=True)
parser.add_argument('-p', required=True)
args = parser.parse_args()

length = int(args.l)

tmp = (args.i).split(".")
prefetch_len = int(args.p)
output_filename = tmp[0] + "_" + str(length) + "_prefetch_" + str(prefetch_len) + ".txt"
out_file = open(output_filename, "w")

embed = int(args.e)
out_cnt = 0


class cache:
    def __init__(self, size, assoc):
        self.size = size
        self.assoc = assoc
        assert(size / 64 >= assoc)
        self.cache_array = []
        for i in range(0, int(size / 64 / assoc)):
            self.cache_array.append([])
    
    def access(self, addr):
        ret = False
        addr = int(addr[2:], 16)
        set_id = (addr) % len(self.cache_array)
        if addr in self.cache_array[set_id]:
            self.cache_array[set_id].remove(addr)
            ret = True
        # print("Accessed set id: " , set_id, ' hit? ', ret)
        self.cache_array[set_id].append(addr)
        if len(self.cache_array[set_id]) > self.assoc:
            self.cache_array[set_id].pop(0)
        return ret

import math


new_emb = math.ceil(embed / prefetch_len) * prefetch_len

mycache = cache(8 * 1024 * 1024 // new_emb, 32)


print("Reading from file", args.i)
last = None
with open(tmp[0] + "_" + str(length) + ".txt") as fp:
    for ln in fp:




        target = hex(((int(ln.split(' ')[0][2:], 16) // prefetch_len)))

        if last == target:
            continue

        last = target

        # hit = mycache.access(target)
        # print(ln.split(' ')[0][2:], ' hit? ', hit)
        # if hit == True:
        #     continue
        if len(ln.split(' ')) >= 2:
            out_file.write(target + ' ' + (ln.split(' ')[1]))
        else:
            out_file.write(target + ' R\n')

        