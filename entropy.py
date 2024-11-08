import math
from math import log2

capacity = 16 * 1024 * 1024 * 1024
granularity = 64

num_of_blocks = capacity // granularity

print("num_of_blocks:", num_of_blocks)

# num_of_blocks = 32

prefetch_range = 1024

set_num = num_of_blocks // prefetch_range


entropy = 0

elt_per_set = num_of_blocks * 1.0 / set_num

for i in range(0, set_num):
    entropy += ( -1.0 * elt_per_set / num_of_blocks) * log2(elt_per_set / num_of_blocks)

print("entropy:", entropy)
