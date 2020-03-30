import matplotlib.pyplot as plt
import numpy as np

def p_file(fname):
    d=np.genfromtxt(fname,delimiter=',',skip_header=1)
    t=d[:,0]
    c=d[:,1]
    cc=c[c>0]
    tc=t[c>0]
    #plt.semilogy(tc,cc,'--')
    #plt.plot(tc,cc,'--')
    #plt.plot(t,c,'--')
    return t,c


files = ["%s_trial_current.txt"%i for i in range(1,10)]

#average the time results from 10 trials
C=np.zeros(np.size(p_file(files[0])[1]))
T=p_file(files[0])[0]

for f in files:
    C=np.add(C,p_file(f)[1])

AVG_C = np.divide(C,120)


plt.semilogy(T/1E-12,AVG_C/1E-3)
plt.xlabel("Time (ps)")
plt.ylabel("Current (mA)")
plt.savefig("current_transient_j82067_19V_10k.png")
