
import argparse
parser = argparse.ArgumentParser(description='A simple command-line argument parser example')
    
    # Adding positional argument
parser.add_argument('-i', help='Your name')
args = parser.parse_args()


f = open(args.i, "r")

hits = 0
misses = 0
conflicts = 0

for line in f.readlines():
    if "ramulator.row_hits_channel" in line:
        hits += int(line.split()[1])
    if "ramulator.row_misses_channel" in line:
        misses += int(line.split()[1])
    if "ramulator.row_conflicts_channel" in line:
        conflicts += int(line.split()[1])

print(hits, misses, conflicts)
print("Row buffer hit rate:", hits * 100.0 / (hits + misses + conflicts))
print("Row Conflict rate:", conflicts * 100.0 / (hits + misses + conflicts))
print("Row Miss rate:", misses * 100.0 / (hits + misses + conflicts))



