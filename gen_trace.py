
import random

length = 20000000

file_path = "random_addr_" + str(length) + ".trace"  # Replace with your file path
file = open(file_path, "w")  # Open the file in write mode
for i in range(0, length):
    rand = int(random.random() * 0xdeadbeaf)
    rand = hex(rand)
    if random.random() >= 0.5:
        file.write(rand + ' R\n')
    else:
        file.write(rand + ' W\n')
file.close()

file_path = "stream_addr_" + str(length) + ".trace"  # Replace with your file path
file = open(file_path, "w")  # Open the file in write mode
for i in range(0, length):
    rand = int(i)
    rand = hex(rand)
    if random.random() >= 0.5:
        file.write(rand + ' R\n')
    else:
        file.write(rand + ' W\n')
file.close()