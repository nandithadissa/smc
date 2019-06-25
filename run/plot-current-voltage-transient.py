import matplotlib.pyplot as plt
import numpy as np

def p_file(cfname,vfname,ax1,ax2):
    d=np.genfromtxt(cfname,delimiter=',',skip_header=1)
    vol=np.genfromtxt(vfname,delimiter=',',skip_header=1)
    t=d[:,0]
    
    c=d[:,1]
    v=vol[:,1]

    #condition the voltage data
    #if the voltage point is zero use the previous voltage point
    v1=v[0]
    for i in range(np.size(v)):
        if v[i] == 0:
            v[i]=v1
        else:
            v1=v[i]

    #condition the current data
    #if a current is zero use the previous data
    i1=c[0]
    for i in range(np.size(c)):
        if c[i] == 0:
            c[i] = i1
        else:
            i1=c[i]
    
    #cc=c[c>0]
    #tc=t[c>0]

    #plt.plot(v,c,'o')
    
    #ax2.plot(t/1e-12,v)
    #ax1.plot(t/1e-12,np.log(c/1e-3))

    #plt.semilogy(tc,cc,'--')
    #plt.plot(tc,cc,'--')
    #plt.plot(t,c,'--')
    return t,c,v



#plot current and voltage with time
plt.close()
fig,ax1=plt.subplots()
ax1.set_xlabel('time (ps)')
ax2=ax1.twinx()
ax1.set_ylabel('current (mA)')
ax2.set_ylabel('voltage (V)')

cfiles = ["%s_trial_current.txt"%i for i in range(1,10)]
vfiles = ["%s_trial_voltage.txt"%i for i in range(1,10)]

for cf,cv in zip(cfiles,vfiles):
    p_file(cf,cv,ax1,ax2)


#average the time results from 10 trials
C=np.zeros(np.size(p_file(cfiles[0],vfiles[0],ax1,ax2)[1]))
V=np.zeros(np.size(p_file(cfiles[0],vfiles[0],ax1,ax2)[2]))
T=p_file(cfiles[0],vfiles[0],ax1,ax2)[0]

for cf,vf in zip(cfiles,vfiles):
    C=np.add(C,p_file(cf,vf,ax1,ax2)[1])
    V=np.add(V,p_file(cf,vf,ax1,ax2)[2])

AVG_C = np.divide(C,9)
AVG_V = np.divide(V,9)


ax1.semilogy(T/1E-12,AVG_C/1E-3,'r.')
ax2.plot(T/1E-12,AVG_V,'b.')

plt.xlim(0,40)

#plt.xlabel("Time (ps)")
#plt.ylabel("Current (mA)")
plt.savefig("average_current_voltage_transient_with_time_M56535p1_test_22V.png")


plt.close()

plt.semilogy(AVG_V,AVG_C/1e-3)
plt.xlabel("Voltage (V)")
plt.ylabel("Current (mA)")
plt.savefig("average_current_voltage_transient_M56535p1_test_22V.png")