

import numpy as np
from scipy.stats import poisson


NUM_L = 6
base = [0] * (NUM_L)
ret = [0] * (NUM_L)

split_L = 2
E = 8

leaf = 0

if split_L >= NUM_L:
    for lvl in range(0, NUM_L):
        base[lvl] = (1 << lvl) - 1        
        ret[lvl] = (leaf >> (NUM_L - lvl - 1))

    top_left = base[NUM_L - 1]
    top_right = (1 << (lvl + 1)) - 1 - 1

else:
    for lvl in range(0, NUM_L):
        if lvl < split_L:
            base[lvl] = (1 << lvl) - 1        
            ret[lvl] = ((leaf >> (NUM_L - split_L - 1)) // E) >> (split_L - lvl - 1)

        elif lvl == split_L:
            base[lvl] = (1 << lvl) - 1      
            ret[lvl] = (leaf >> (NUM_L - lvl - 1))

        else:
            base[lvl] = base[lvl - 1] + (E * ((1 << (split_L - 1))) ) * (1 << (lvl - split_L - 1))
            ret[lvl] = (leaf >> (NUM_L - lvl - 1))

    top_left = base[NUM_L - 1]
    top_right = top_left + (E * ((1 << (split_L - 1))) ) * (1 << (NUM_L - split_L - 1)) - 1
            

# print(top_left)
# print(top_right)
print("Range:", top_right - top_left)

print("Accessed node ID:")
for i in range(0, len(ret)):
    print(ret[i] + base[i])

# print(ret)
# print(base)