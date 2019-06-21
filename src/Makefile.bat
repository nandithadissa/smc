del *.o
del *.dll
del *.exe

del ..\run\smc.exe

g++ -c carrier_class.cpp
g++ -c device_properties.cpp
g++ -c dev_prop_func.cpp
g++ -c drift_velocity.cpp
g++ -c functions.cpp
g++ -c histogram_class.cpp
g++ -c ii_coef.cpp
g++ -c main.cpp
g++ -c SMC_class.cpp
g++ -c tools_class.cpp
g++ -c device_class.cpp


g++ -o smc.exe main.o device_class.o carrier_class.o device_properties.o dev_prop_func.o drift_velocity.o functions.o histogram_class.o ii_coef.o SMC_class.o tools_class.o

mkdir ..\run
copy *.exe ..\run 
del *.o
del *.exe
echo "completed. smc.exe in ..\run folder"
