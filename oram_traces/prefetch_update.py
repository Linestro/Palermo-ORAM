





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
parser.add_argument('-p', required=True)

args = parser.parse_args()


tmp = (args.i).split(".")
prefetch_len = int(args.p)
output_filename = tmp[0] + "_prefetch_" + str(prefetch_len) + "." + tmp[1]
out_file = open(output_filename, "w")

last_fetch_addr = None

print("Reading from file", args.i)
with open(args.i) as fp:
    for ln in fp:
        addr, op = ln.split(' ')
        
        this_fetch_addr = int(ln.split(' ')[0][2:], 16) // prefetch_len

        if this_fetch_addr == last_fetch_addr:
            continue

        out_file.write(hex(int(ln.split(' ')[0][2:], 16) // prefetch_len) + ' ' + op)

        last_fetch_addr = int(ln.split(' ')[0][2:], 16) // prefetch_len

                
        