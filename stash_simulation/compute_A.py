

import numpy as np
from scipy.stats import poisson

z_list = [4, 5, 8, 16, 32]
# z_list = [4]

for Z in z_list:

    for A in range(1, 100):
        # print(Z * np.log(2 * Z * 1.0 / A) + A/2 - Z - np.log(4))
        if Z * np.log(2 * Z * 1.0 / A) + A/2 - Z - np.log(4) < 0:
            print('Z = ', Z,':', 'A = ', A - 1)
            # for S in range(0, 10):
            #     res = (2 * Z + S) * 1.0 * (1 + poisson.cdf(S,A-1))
            #     print("S: ", S, res)
            break
            # pass




print(poisson.cdf(5,3))