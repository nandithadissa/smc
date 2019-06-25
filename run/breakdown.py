import matplotlib.pyplot as plt
import numpy as np

def p_file(fname):
    d=np.genfromtxt(fname,delimiter=' ',skip_header=1)
    t=d[:,1]
    plt.hist(t/1e-12,bins='auto')
    return 


files = ["25time_to_breakdown.txt"]

for f in files:
    p_file(f)

plt.xlabel("Time (ps)")
plt.ylabel("Counts")
plt.savefig("breakdown_time_histogram.png")