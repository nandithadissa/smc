/* Copyright 2017 Advanced Detector Centre, Department of Electronic and
   Electrical Engineering, University of Sheffield, UK.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.*/

/*
   device_properties.cpp contains device_properties() which calculates the device properties for a given device in a given material.

   Takes user input for divisions per transit time, injection condition, simulation time and number of trials.
   Takes material input from main.cpp
   Reads in applied biasses from the user generated file bias_input.txt
   Uses the read in device structure from the user generated file doping_profile.txt contained in the device class.

   Uses the Classes SMC, Carrier, Device & tools.
   Also uses functions.h which contains common functions used in all three modes.
   Also uses dev_prop_func.h which contains functions unique to the device properties mode.

   Prototyped in model.h

   Calculates Gain, Excess Noise Factor, Breakdown Probability and Timing Statistics.

   Jonathan Petticrew, University of Sheffield, 2017.
 */

/*
	Fieldlookup: git branch handled the issue of the applied voltage transient. A new array Vnum was created.
	As each electron moves within some time slices, a current is created which is recorded in Inum and the voltage is added on to
	the Vnum. This Inum and Vnum are written as files

	04-02-2020
	fixedcap: git branch will introduce a fixed capacitor in parallel to the diode. This will satisfy the following equation:
		
		Vd = Vbias - ( Ispad(Vd) + C.dVd/dt ) * R

		where he Vd is the diode bias, C is the fixed cap, R is the quench resister and Vbias is the constant bias voltage on the circuit.
		
		The dVd/dt is calculated by taking the 2 adjacent time slots in the Vnum array.
*/

#include "model.h"
#include "SMC.h"
#include "device.h"
#include "functions.h"
#include "dev_prop_func.h"
#include "tools.h"
#include "carrier.h"
#include <stdio.h>
//#include <tchar.h>
#include <math.h>

#include <iostream>
#include <string.h>
#include <fstream>

#include <algorithm>
#include <map>
#include <iterator>


#define TRAILS_TO_SAVE 50

#define CAP 20 //femto Fareds

double cap_current=0;

void device_properties(int material, double dResister=10E3){
	FILE *userin;
	if ((userin=fopen("user_inputs.txt","w"))==NULL)//Opens and error checks
	{
		printf("Error: user_inputs.txt can't be opened'\n");
	}
	if(material == 1) fprintf(userin,"Silicon\n");
	else if(material == 2) fprintf(userin,"Gallium Arsenide\n");
	else if(material ==3) fprintf(userin,"Indium Gallium Phosphide\n");
	int timearray, Highest;
	double cumulative, voltage;
	SMC constants; //SMC parameter set
	constants.mat(material); // tell constants what material to use
	SMC *pointSMC = &constants; //Used to pass constants to other classes.
	
	device diode(pointSMC); // Device Class
	
	FILE *out;
	double BreakdownCurrent=1e-4; // changed was at 1e-4 define the current threshold for avalanche breakdown as 0.1mA
	if ((out=fopen("Result_1.txt","w"))==NULL)//Opens and error checks
	{   printf("Error: Result_1.txt can't open\n");}

	//read in bias
	int bias_count=biascounter(); //counts number of voltages to be simulated
	FILE *bias;
	if ((bias=fopen("bias_input.txt","r"))==NULL)//Opens and error checks
	{
		printf("Error: bias.txt can't be opened'\n");
	}
	
	double *V = new double[bias_count];
	bias_count=0;
	while(fscanf(bias,"%lf",&voltage)>0) {
		V[bias_count]=voltage;
		bias_count++;
	}
	fclose(bias);

	//create device objects for every bias point at 0.01 V resolution for a 5V range from bias_input value
	//e.g id the bias_input is 22V, device objects are created between 17V to 22 V at 0.01 resolution
	/*	
	std::map<float,device*> diode_map;
	float bias_range = 10.0;
	float bias_res = 0.001;
	float bias_max = V[0];

	printf("creating the diode map'\n");
	for(int i=0;i< bias_range/bias_res;i++)
	{
		device* _pd = new device(pointSMC);
		printf("calculating field profile for bias:%f'\n",bias_max);
		_pd->profiler(bias_max);//generates field profile for diode
		diode_map.insert(std::pair<float,device*>(bias_max,_pd));
		bias_max -= bias_res;
	}		
	//done
	printf("done .. creating the diode map'\n");
	*/

	int timeslice = timesliceread();
	fprintf(userin,"Divisions Per Transit time: %d\n", timeslice);
	int usDevice = usDeviceread();
	if (usDevice==1) fprintf(userin, "Pure Electron Simulation\n");
	else if (usDevice==2) fprintf(userin, "Pure Hole Simulation\n");
	double simulationtime = simulationtimeread();
	fprintf(userin, "Simulation time limit: %g ps\n",simulationtime/1e-12);
	double Ntrials = trialsread();
	fprintf(userin, "Numer of Trials: %lf\n",Ntrials);
	fclose(userin);
	tools simulation(pointSMC);
	simulation.scattering_probability();//this function returns 0 if no output can be generated and the user wants to quit
	sgenrand(835800);//seeds the random number generator constant used to alow for comparison using different parameters.
	int num;
	double Efield,npha,nph,nphe,nii,nsse,Energy,z_pos,dE,kf,kxy,kz,nssh;
	double drift_t;

	// create electron and hole classes, too large for stack so have been created with new.
	carrier* electron=new carrier(pointSMC);
	carrier* hole=new carrier(pointSMC);

	
	double breakdown, Pbreakdown, Vsim, Vsimtemp;
	/**** BEGIN SIMULATION LOOP VOLTAGE ****/
	int bias_array=0;
	printf("%d %d \n", bias_count, bias_array);
	for(bias_array=0; bias_array<bias_count; bias_array++)
	{
		Vsim=V[bias_array];
		Vsimtemp=Vsim;
		
		//std::map<float,device*>::iterator it = diode_map.begin();
		//device& diode = *(it->second);
		diode.profiler(Vsim);//generates field profile for diode
			
		
		printf("Width = %e \n", diode.Get_width());
		double timestep=diode.Get_width()/((double)timeslice*1e5); //Time tracking step size in seconds
		printf("timestep = %e \n", timestep);
			
		int CurrentArray=(int)(simulationtime/timestep); // calculates number of timesteps required for simulationtime
		
		double cutofftime=(CurrentArray-5)*timestep; // prevents overflow
		int cutoff =0;


		//data structures to hold the total curret per each run
		double* ITotalCurrent=new double[TRAILS_TO_SAVE*CurrentArray]; //hold all the current data per all the trials
		double* VTransientVoltage=new double[TRAILS_TO_SAVE*CurrentArray]; //hold the transient voltage across the diode with a 10K resister in series
		
		std::cout << "size of ITotalCurrent array:" << sizeof(ITotalCurrent) << std::endl;
		double* I=new double[CurrentArray];
		double* Inum=new double[CurrentArray];
		
		//NEW 04012020
		double* Vnum=new double[CurrentArray];
		//		

		int Iarray;
		for (Iarray=0; Iarray<CurrentArray; Iarray++) {
			I[Iarray]=0;
		}
		//generate files for simulated voltage output.
		FILE *tbout;
		FILE *Mout;
		char nametb[] = "time_to_breakdown.txt";
		char nameM[]= "gain_out.txt";
		char voltagetb[8];
		snprintf(voltagetb,sizeof(voltagetb),"%g",Vsim);
		int filetb_len = strlen(voltagetb) + strlen(nametb) + 1;
		char *filetb = new char[filetb_len];
		snprintf(filetb,filetb_len,"%s%s",voltagetb,nametb);
		if ((tbout=fopen(filetb,"w"))==NULL)
		{
			printf("Error: tbout can't be opened'\n");
		}
		delete[] filetb;
		int fileM_len = strlen(voltagetb) + strlen(nameM) + 1;
		char *fileM = new char[fileM_len];
		snprintf(fileM,fileM_len,"%s%s",voltagetb,nameM);
		if ((Mout=fopen(fileM,"w"))==NULL)
		{
			printf("Error: gain out can't be opened'\n");

		}
		delete[] fileM;
		char namecounter[] = "eventcounter.txt";
		FILE *counter;
		int countname_len = strlen(namecounter) + strlen(voltagetb) + 1;
		char *countname = new char[countname_len];
		snprintf(countname,countname_len,"%s%s",voltagetb,namecounter);
		if((counter=fopen(countname,"w"))==NULL) {
			printf("Error: Can't open counter dump file \n'");

		}
		delete[] countname;
		Highest=0;
		cumulative=0;

		breakdown=0;
		double gain,Ms,F,tn,time;
		double dt,dx;
		Ms=0;
		F=0;
		gain=0;
		double globaltime=0;
		int num_electron,num_hole,prescent_carriers, pair;
		


		//Originally the voltage was held constant
		//Assuming a 10K resister the voltage is now varied as the current increases per each trail
		double Resister = dResister; //500e3;

		//data structure to hold the breakdown time step for each trial
		int trial_bd_times[(int)Ntrials];
		double trial_bd_minvoltage[(int)Ntrials];

		/**** BEGIN SIMULATION LOOP TRIALS****/
		for(num=1; num<=Ntrials; num++)
		{
			//
			printf("trial:%d/%d",num,Ntrials);
			
			//start with the bias value
			Vsimtemp=Vsim;
			
			//std::map<float,device*>::iterator it = diode_map.begin();
			//diode = *(it->second);
			diode.profiler(Vsimtemp);

			trial_bd_times[num]=CurrentArray-1;

			for (Iarray=0; Iarray<CurrentArray; Iarray++) 
			{
				Inum[Iarray]=0;
				Vnum[Iarray]=Vsim; //INITIALIZE 040102020
			}

			num_electron=1;
			num_hole=1;
			tn=1;
			prescent_carriers=2;
			kf=0;
			kxy=0;
			kz=0;
			dE=0;
			Energy=0;
			z_pos=0;
			drift_t=0;
			time=0;
			dt=0;
			dx=0;
			nph=0;//total phonon interactions
			npha=0;//photon absorption counter
			nphe=0;//photon emission counter
			nii=0;//impact ionization event counter
			nsse=0;//scattering event counter
			nssh=0;
			int cut2=0;
			globaltime=timestep;


			/* Device 1-PIN
			   Device 2-NIP*/

			if (usDevice==1) {
				electron->Input_pos(1,diode.Get_xmin()+1e-10);
				hole->Input_pos(1,-1);
				prescent_carriers=1;
			}
			if (usDevice==2) {
				electron->Input_pos(1,(diode.Get_xmax()+1e-10));
				hole->Input_pos(1,(diode.Get_xmax()-1e-10));
				prescent_carriers=1;
			}
			//carrierlimit is a threshold to end the simulation early  - RAMO's theorm
			double carrierlimit=BreakdownCurrent*diode.Get_width()/(5*constants.Get_q()*1e5);

			/****TRACKS CARRIERS WHILE IN DIODE****/
			while(prescent_carriers>0 && cut2==0)
			{ /****LOOPS OVER ALL PAIRS ****/

				//original
				//double *pmax_current_now = std::max_element(Inum,Inum+CurrentArray);
				//int max_current_pos = pmax_current_now - &Inum[0];
				//Vsimtemp = Vsim - Resister*(*pmax_current_now);
				//

				//if (num < TRAILS_TO_SAVE){
				//std::cout<< "max current array position:" << max_current_pos << std::endl;
						//VTransientVoltage[num*CurrentArray+max_current_pos] = Vsimtemp;
				//		VTransientVoltage[num*CurrentArray+current_pos] = Vsimtemp; //changed
				//}
				//
				

				for(pair=1; pair<=num_electron; pair++)
				{
					int flag=0;
					// ELECTRON PROCESS
					z_pos=electron->Get_pos(pair);
					time=electron->Get_time(pair);
					dt=electron->Get_dt(pair);
					dx=electron->Get_dx(pair);
					if(z_pos<diode.Get_xmin()) z_pos=diode.Get_xmin()+1e-10; // resets a bad trial where the electron drifted out the device the wrong way (extremly rare but causes program to hang)

					//If flag stays 0 for all the devices it means that there are no carriers behind globaltime and globaltime can be advanced
					//Doing this limits the program to only be simulating the carriers in the same timebin at the same time (Important for calculating instentanious current)
					if(z_pos<diode.Get_xmax() && time<globaltime)//checks inside field
					{    flag++;//used to advance globaltime
						 Energy=electron->Get_Egy(pair);
						 if((electron->Get_scattering(pair)==0))//if not selfscattering scatters in random direction
						 {   electron->scatter(pair,0);}

						 kxy=electron->Get_kxy(pair);
						 kz=electron->Get_kz(pair);

						//electron drift process starts
						//drifts for a random time
						 double random1;
						 random1=genrand();
						 drift_t= -log(random1)/(simulation.Get_rtotal());
						 time+=drift_t;
						 dt+=drift_t;

						//updates parameters based on random drift time
						 Efield=diode.Efield_at_x(z_pos);
						 kz+=(constants.Get_q()*drift_t*Efield)/(constants.Get_hbar());
						 dE=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_e_mass()))*(kxy+kz*kz)-Energy;
						 Energy=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_e_mass()))*(kxy+kz*kz);
						 z_pos+=dE/(constants.Get_q()*Efield);
						 dx+=dE/(constants.Get_q()*Efield);
						 if(time>cutofftime) {
							 //cuts off electron and removes it from device if user spec. timelimit exceeded
							 z_pos=diode.Get_xmax()+10;
							 cutoff=1;
						 }
						 if(dt>=timestep) {
							 //calc current  if time since last calculated  >timestep
							 timearray=(int)floor(time/timestep);
							 int previous;
							 previous=electron->Get_timearray(pair);
							 int test;
							 for(test=(previous+1); test<(timearray+1); test++) {
								 //Uses Ramos Theorem Here
								 I[test]+=constants.Get_q()*dx/(dt*diode.Get_width());
								 Inum[test]+=constants.Get_q()*dx/(dt*diode.Get_width());

								//******************
								//******************

								//THIS IS THE PLACE THAT THE GIVES THE EXACT TIME SLICE OF THE CURRENT
								//USE THIS CURRENT TO CALCULATE THE VOLTAGE DROP AND PUT IT IN A BIN CALLED THE Vnum[]
								//CHANGE THE Vnum AND USE THE Vnum TO MAKE CHANGES TO THE FIELD
								//FINALLY WRITE THE Vnum TO A FILE AS BEFORE
								
								//Vnum[test] -= (Resister*Inum[test]);  //no capacitive effect is considered

								///Vd = Vbias - ( Ispad(Vd) + C.dVd/dt ) * R
								//THE CAP CANNOT RESPOND FASTER THAN ITS DIELECTRIC RELAXTION TIME 
								//FOR A 100K R THE TAU = 20 (fFared) * 100K = 2ns
								//SO, USE 2nS as the denominator in the below equation

								//printf("cap_voltage:%f\n",Vnum[test]-Vnum[test-1]);
					
								//THIS IS WITHOUT SELF CONSISTANT SOLVING OF THE VOLTAGE - WRONG - CAP CURRENT IS TOO HIGH
								//if(test>=1) //the first element is neglected								
								//	Vnum[test] = Vsim -  (Resister*Inum[test] + Resister*CAP*1e-15*(Vnum[test]-Vnum[test-1])/2e-9);
						

								//040820
								//The diode dynamics are considered based on the generated current. The diode resistance is calculated base
								//on the diode current. Initially we consider just 3 regions of the current. Before breakdown, at breakdown and
								//after breakdown.

								//first find if the diode reached breakdown
								bool is_bd=false;				   //did breakdown happen
								int  timestep_bd = CurrentArray-1; //this is the timestep BD happend
								bool is_closetobd=false;    //near the breakdown
								int  timestep_closetobd = CurrentArray-1;
								//double bd_currentrange = 1e-6; //within the breakdown current
								bool is_recharge=false;
								double voltage_bd=0.0;	//final voltage of the diode at the breakdown							

								double breakdowncurrent = BreakdownCurrent*1; 
								double bd_currentrange = breakdowncurrent*0.1;//1e-6; //within the breakdown current
							
								//find the pre-breakdown point	
								for(int i=0;i<CurrentArray;i++)
								{
									if((Inum[i]<breakdowncurrent)&&(Inum[i]>breakdowncurrent-bd_currentrange))
									{
										//printf("found close to bd point at current step:%d\n",i);
										is_closetobd=true;
										timestep_closetobd=i;
										break;
									} 
								}

								//find the breakdown point
								for(int i=timestep_closetobd;i<CurrentArray;i++)
								{
									if (Inum[i]>breakdowncurrent)
									{
										//printf("found bd at current step:%d\n",i);
										is_bd=true;
										timestep_bd=i;
										//printf("bd at:%d\n",i);	
										break;
									}
								}
							
								if(is_bd)
								{	
									//printf("trial_timestep_bd:%d and run timestep_bd:%d\n",trial_bd_times[num],timestep_bd);	
									if (trial_bd_times[num] > timestep_bd)
										trial_bd_times[num] = timestep_bd;
								}

								//get the min position as the trial_breakdown time pos
								//if (true)//(trial_timestep_bd > timestep_bd)
								//{
								//	printf("trial timestep for bd:%d\n",trial_timestep_bd);	
								//	trial_timestep_bd = timestep_bd;
								//}	
						
								double diodeR_nobd 	 = 1E9; //Ohm very large 
								double diodeR_nearbd = 1E5; //Ohm low
								double diodeR_bd 	 = 1;   //Ohm very low
							
								//check the range
								if((!is_closetobd)&&(!is_bd)) //no breakdown
								{
									Vnum[test] = Vsim - (Resister*Inum[test]);  //no capacitive effect is considered : Tau is very large
								}
								
								if (is_closetobd)
								{
									//Vnum[test] = Vsim - (Resister*Inum[test]);  //no capacitive effect is considered : first order calc before selfconsistant
								
									//SELF CONSISTANT SOLVING THE VOLTAGE
									//Vd = Vbias - ( Ispad(Vd) + C.dVd/dt ) * R
									double vdelta = 1E-4;
									double diff=0.0;
									for(int i=0;i<1000;i++)
									{
										Vnum[test] = Vsim -  vdelta*i;
										diff = Vnum[test] - (Vsim - Resister*(Inum[test] + (CAP*1e-15*(Vnum[test]-Vnum[test-1])/(diodeR_nearbd*CAP*1e-15))));
										if (std::abs(diff) < 1e-3)
										{ 
											//printf("tria:%d,diff=%e,converge:%f,i=%d\n",num,diff,Vnum[test],i);
											break;
										}
									}
								}
	
								if (is_bd)
								{
									Vnum[test] -= (Inum[test]+Inum[test-1])*0.5*timestep/(CAP*1e-15);					//Vd(t) = Vdo - 1/C(Intergral[ic.dt])	
									//printf("at bd Vnum:%lf\n",Vnum[test]);												//trapizoidal rule used	
									voltage_bd=Vnum[test];
									//trial_bd_minvoltage[num] = Vnum[timestep_bd]; //min vol
								}

								//put the minimum value in to the breakdown voltage
								double minvol=Vsim;
								for(int  i=0;i<CurrentArray;i++)
								{	
									if (minvol > Vnum[i])
									{
										minvol=Vnum[i];
									}
								}
								//
								trial_bd_minvoltage[num] = minvol; //min vol

								
								//040720 test
								//simulate a breakdown 	
								//BREAK the SIM in to 3 parts:
								// 1. Before breakdown the time constant is very large. The cap discharge from the diode so it will be very slow.
								/*    

									2. Whent he spad fired the current discharges the cap very fast and this happens in sub pico second times. 
										This is because the diode resistance is now about 1 ohm. So use this to simulate the diode voltage drop.
			
									3. After the diode is  breakdown the cap gets recahrged from the source. This happens in ns times.

									4. calculate the voltage profile independently for each case and stich it to gether to get the voltage profile.

								*/

								//feed back to the field
								if((Vsim - Vnum[test]) > 0.05) //dont' change the field.
								{
									//if bd is hit kill it
									if(is_bd)
									{
										//printf("trail:%d,bd is hit.. killing the avalanch process by changing the field\n",num);
										diode.profiler(10.0);
									}
									else
									{
										//diode.profiler(Vnum[test]);
										//diode.profiler((float)Vnum[test]);
										int _t = (int)(Vnum[test]*1000);
										float _f = ((float)_t)/1000;
										diode.profiler(_f);
										//printf("trail:%d,cal field using bias:%lf,%lf\n",num,Vnum[test],_f);
									}
								}
								//*****************

							 }

							 electron->Input_timearray(pair,timearray);
							 dt=0;
							 dx=0;
						 }
						 electron->Input_time(pair,time);
						 electron->Input_dt(pair,dt);
						 electron->Input_dx(pair,dx);

						//electron drift process ends

						//update electron position and energy
						 electron->Input_pos(pair,z_pos);
						 electron->Input_Egy(pair,Energy);

						//electron scattering
						 if(z_pos<0) z_pos=1e-10;
						 if((z_pos<=diode.Get_xmax()))
						 { //electron scattering process starts
							 double random2;
							 int Eint;
							 Eint=(int)floor(Energy*1000.0/constants.Get_q()+0.5);
							 if (Energy>constants.Get_Emax())
							 {    Eint= constants.Get_NUMPOINTS();
							  random2=simulation.Get_pb(2,constants.Get_NUMPOINTS());}
							 else if (Energy == constants.Get_Emax()) random2=simulation.Get_pb(2,constants.Get_NUMPOINTS());
							 else{
								 random2=genrand();
							 }

							 if(random2<=simulation.Get_pb(0,Eint)) //phonon absorption
							 {    Energy+=constants.Get_hw();
							  npha++;
							  nph++;
							  electron->Input_scattering(pair,0);}
							 else if(random2<=simulation.Get_pb(1,Eint)) //phonon emission
							 {    Energy-=constants.Get_hw();
							  nphe++;
							  nph++;
							  electron->Input_scattering(pair,0);}
							 else if(random2<=simulation.Get_pb(2,Eint)) //impact ionization
							 {
								 Energy=(Energy-constants.Get_e_Eth())/3.0;
								 num_electron++;
								 electron->generation(num_electron,z_pos,Energy,time,0,(int)floor(time/timestep));
								 num_hole++;
								 hole->generation(num_hole,z_pos,Energy,time,0,(int)floor(time/timestep));
								 tn++;
								 nii++;
								 prescent_carriers+=2;
								 electron->Input_scattering(pair,0);
							 }
							 else if(random2>simulation.Get_pb(2,Eint)) //selfscattering
							 {    nsse++;
							  electron->Input_scattering(pair,1);
							  electron->Input_kxy(pair,kxy);
							  electron->Input_kz(pair,kz);}
							 //electron scattering process ends

						 }
						 else prescent_carriers--;

						 electron->Input_Egy(pair,Energy);
						 if(time>globaltime) flag--; }

					//HOLE PROCESS
					z_pos=hole->Get_pos(pair);
					time=hole->Get_time(pair);
					dt=hole->Get_dt(pair);
					dx=hole->Get_dx(pair);
					if(z_pos>diode.Get_xmax()) z_pos=diode.Get_xmax()-1e-10;
					if(z_pos>=diode.Get_xmin() && time<globaltime)
					{    Energy=hole->Get_Egy(pair);
						 flag++;
						 if((hole->Get_scattering(pair)==0))
						 {    hole->scatter(pair,2);}

						 kxy=hole->Get_kxy(pair);
						 kz=hole->Get_kz(pair);


						//Hole drift starts here
						 double random11;
						 random11=genrand();
						 drift_t= -log(random11)/simulation.Get_rtotal2();
						 time+=drift_t;
						 dt+=drift_t;
						 Efield=diode.Efield_at_x(z_pos);
						 kz-=((constants.Get_q()*drift_t*Efield)/(constants.Get_hbar()));
						 dE=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_h_mass()))*(kxy+kz*kz)-Energy;
						 Energy=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_h_mass()))*(kxy+kz*kz);
						 z_pos-=dE/(Efield*constants.Get_q());
						 dx+=dE/(Efield*constants.Get_q());
						 if(time>cutofftime) {
							 z_pos=diode.Get_xmin()-10;
							 cutoff=1;
						 }
						 if(dt>=timestep) {
							 timearray=(int)floor(time/timestep);
							 int previous;
							 previous=hole->Get_timearray(pair);
							 int test;
							 for(test=(previous+1); test<(timearray+1); test++) {
								 I[test]+=constants.Get_q()*dx/(dt*diode.Get_width());
								 Inum[test]+=constants.Get_q()*dx/(dt*diode.Get_width());


								//***************
								//DO THIS FOR THE HOLES
								//

							 }
							 dt=0;
							 dx=0;
							 hole->Input_timearray(pair,timearray);
						 }
						 hole->Input_time(pair,time);
						 hole->Input_dt(pair,dt);
						 hole->Input_dx(pair,dx);

						//Hole drift finishes here
						 hole->Input_pos(pair,z_pos);
						 hole->Input_Egy(pair,Energy);


						 if(z_pos>diode.Get_xmax()) z_pos=diode.Get_xmax()-1e-10;
						 if(z_pos>=diode.Get_xmin())
						 { //Hole scattering starts here
							 double random22;
							 int Eint2;
							 Eint2=(int)floor(Energy*1000.0/constants.Get_q()+0.5);
							 if (Energy>constants.Get_Emax())
							 {    Eint2=constants.Get_NUMPOINTS();
							  random22=simulation.Get_pb2(2,constants.Get_NUMPOINTS());}
							 else if (Energy==constants.Get_Emax())
							 {    random22=simulation.Get_pb2(2,constants.Get_NUMPOINTS());}
							 else {
								 random22=genrand();
							 }

							 if(random22<=simulation.Get_pb2(0,Eint2)) //phonon absorption
							 {
								 Energy+=constants.Get_hw();
								 npha++;
								 nph++;
								 hole->Input_scattering(pair,0);
							 }
							 else if(random22<=simulation.Get_pb2(1,Eint2)) //phonon emission
							 {
								 Energy-=constants.Get_hw();
								 nphe++;
								 nph++;
								 hole->Input_scattering(pair,0);
							 }
							 else if(random22<=simulation.Get_pb2(2,Eint2)) //impact ionization
							 {
								 Energy=(Energy-constants.Get_h_Eth())/3.0;
								 num_electron++;
								 electron->generation(num_electron,z_pos,Energy,time,0,(int)floor(time/timestep));
								 num_hole++;
								 hole->generation(num_hole,z_pos,Energy,time,0,(int)floor(time/timestep));
								 tn++;
								 nii++;
								 prescent_carriers+=2;
								 hole->Input_scattering(pair,0);
							 }
							 else if(random22>simulation.Get_pb2(2,Eint2)) //selfscattering
							 {
								 nssh++;
								 hole->Input_scattering(pair,1);
								 hole->Input_kxy(pair,kxy);
								 hole->Input_kz(pair,kz);
							 }
							 //hole scattering ends here

						 }
						 else prescent_carriers--;

						 hole->Input_Egy(pair,Energy);
						 if(time>globaltime) flag--; }
					Highest=(int)_max(Highest,pair);

					if(flag==0) {
						globaltime+=timestep;
						//This is where globaltime is incrimented
					}
				}
				int scan=0;
				int scanlimit=0;
				//scans current array to detect breakdown current and stops sim early
				if(pair>carrierlimit) {
					while(scan==0) {
						for(Iarray=0; Iarray<CurrentArray; Iarray++) {
							if(Inum[Iarray]>BreakdownCurrent) {
								scan=1;
								cut2=1;
							}
							if(Iarray==CurrentArray-1) scan=1;
						}
						if(Inum[Iarray]==0) ++scanlimit;
						if(Inum[Iarray]!=0) scanlimit=0;
						if(scanlimit>50) scan=1;
					}
				}
			}
			gain+=tn/Ntrials; //accumilates average gain
			Ms+=(tn*tn/Ntrials); //accumilates average Ms, used to calculate noise
			cumulative+=tn; //tracks average gain so far in simulation
			double printer=cumulative/num;

			//reset carrier arrays to 0 after trial
			electron->reset();
			hole->reset();
			//checks for breakdown at end of sim
			for (Iarray=0; Iarray<CurrentArray; Iarray++) {
				if(Inum[Iarray] > BreakdownCurrent) {
					breakdown++;
					fflush(counter);
					double tb = timestep*Iarray;
					fprintf(tbout,"%d %g\n",num,tb);
					fflush(tbout);
					break;
				}
			}

			//trapezium rule
			int arealimitnum=0;
			double totalareanum=0;
			double area,x1,x2,y1,y2;
			int i;
			for(i=0; i<(CurrentArray-1); i++)
			{
				y1=Inum[i];
				x1=timestep*i;
				y2=Inum[i+1];
				x2=timestep*(i+1);
				area=y1*(x2-x1)+0.5*(y2-y1)*(x2-x1);
				totalareanum+=area;
			}
			totalareanum=totalareanum/1.6e-19;
			fprintf(Mout, "%d %g %g\n",num, totalareanum, tn);
			fflush(Mout);
			double Pbprint=breakdown/num;
			if(!(num%100)) {
				if(cutoff==0) printf("Completed trial: %d Gain=%f Pb=%f . Max array index=%d\n",num,printer,Pbprint,Highest);
				if(cutoff==1) printf("Completed trial: %d Cutoff Pb=%f  Max array index=%d\n",num,Pbprint,Highest);
			}
		
			///PLOT THE Inum array. This gives the time vs. Current per trail.
			if (num<TRAILS_TO_SAVE)
			{
				for(int i=0; i<(CurrentArray-1); i++)
				{
					//std::cout << num << std::endl;;
					//std::cout << i << std::endl;
					ITotalCurrent[num*CurrentArray+i] = Inum[i];
					VTransientVoltage[num*CurrentArray+i] = Vnum[i]; //ADDED 040102020

					//recarge - 040720
					if (i>trial_bd_times[num])
					{
						//VTransientVoltage[num*CurrentArray+i] = Vsim*(1-exp(-(i-trial_bd_times[num])*timestep/(2e-9))); //ADDED 040720 TEST
						VTransientVoltage[num*CurrentArray+i] = trial_bd_minvoltage[num] + (Vsim-trial_bd_minvoltage[num])*(1-exp(-(i-trial_bd_times[num])*timestep/(2e-9))); //ADDED 040720 TEST
					}
				}

				
			}

		}	//trails ends

		std::cout << "trials finished" << std::endl;

		//write the total current data to a file
		std::string str1;//(100,'\0');
		for(int num=0;num<TRAILS_TO_SAVE;num++)
		{
			std::ofstream fp_transient_current;
			std::string fname(100,'\0');
			std::snprintf(&fname[0],fname.size(),"%d_trial_current.txt",num);
			std::cout << fname << std::endl;
			fp_transient_current.open(fname);
			
			for(int i=0; i<CurrentArray-1; i++)
			{
				char _t[32];
				char _c[32];
				std::snprintf(&_t[0],32,"%g",timestep*i);
				std::snprintf(&_c[0],32,"%g",ITotalCurrent[num*CurrentArray+i]);
				str1 = _t;
				str1 += ',';
				str1 += _c;
				str1 += '\n';

				//std::snprintf(&str1[0],str1.size(),"%g,%g",(timestep*i),ITotalCurrent[num*CurrentArray+i]);
				//std::cout << str1 << std::endl;
				fp_transient_current << str1;
			}
		
			fp_transient_current.close();
		}


		//write the transient voltage to file here
		std::string str2;//(100,'\0');
		for(int num=0;num<TRAILS_TO_SAVE;num++)
		{
			std::ofstream fp_transient_voltage;
			std::string fname(100,'\0');
			std::snprintf(&fname[0],fname.size(),"%d_trial_voltage.txt",num);
			std::cout << fname << std::endl;
			fp_transient_voltage.open(fname);
			
			
			for(int i=0; i<CurrentArray-1; i++)
			{
				char _t[32];
				char _c[32];
				std::snprintf(&_t[0],32,"%g",timestep*i);
				std::snprintf(&_c[0],32,"%g",VTransientVoltage[num*CurrentArray+i]);
				str2 = _t;
				str2 += ',';
				str2 += _c;
				str2 += '\n';

				//std::snprintf(&str1[0],str1.size(),"%g,%g",(timestep*i),ITotalCurrent[num*CurrentArray+i]);
				//std::cout << str1 << std::endl;
				fp_transient_voltage << str2;
			}
		
			fp_transient_voltage.close();
		}


		delete[] ITotalCurrent;
		delete[] VTransientVoltage;

		F=Ms/(gain*gain);
		Pbreakdown=breakdown/Ntrials;

		if(cutoff==0) {
			printf("V= %f M= %f, F= %f, Pb= %f \n",Vsim,gain,F,Pbreakdown);
			fprintf(out,"V= %f M= %f F= %f, Pb= %f \n",Vsim,gain,F,Pbreakdown);
		}
		else{
			printf("V= %f M= cutoff, F= cutoff, Pb= %f \n",Vsim,Pbreakdown);
			fprintf(out,"V= %f M= cutoff F= cutoff, Pb= %f \n",Vsim,Pbreakdown);
		}
		fflush(out);
		fclose(tbout);
		if(breakdown==0) {
			FILE *Iout;
			char name[] = "current.txt";
			char Vsimchar[8];
			snprintf(Vsimchar, sizeof(Vsimchar),"%g", Vsim);
			int filespec_len = strlen(name) + strlen(Vsimchar) + 1;
			char *fileSpec=new char[filespec_len];
			snprintf(fileSpec,filespec_len, "%s%s", Vsimchar, name);
			if ((Iout=fopen(fileSpec,"w"))==NULL)
			{
				printf("Error: current.txt\n");
			}
			delete[] fileSpec;
			fprintf(Iout,"V= %f \n", Vsim);
			fprintf(Iout,"time step size in %e s\n",timestep);
			fprintf(Iout,"t                I \n");
			int Ioutprint =0;
			int i;
			for(i=0; i<CurrentArray; i++) {
				double timeprint=timestep*i;
				double current = I[i]/(double)Ntrials;
				double count=I[i]/(constants.Get_q()*(double)Ntrials);
				if(I[i]>0 && Ioutprint <50) {
					fprintf(Iout,"%g %g \n",timeprint,current);
					Ioutprint=0;
				}
				else if(Ioutprint <50) {
					fprintf(Iout,"%g %g \n",timeprint,current);
					Ioutprint++;
				}
			}
			fflush(Iout);
			fclose(Iout);
		}
		delete [] I;
		delete [] Inum;
		fclose(counter);	
		fclose(Mout);
	}
	electron->~carrier();
	hole->~carrier();
	delete electron;
	delete hole;
	fclose(out);
	//postprocess(V, simulationtime, bias_count);
	delete[] V;
	//delete the diode_map
	/*
	std::map<float,device*>::iterator it;
	for(it=diode_map.begin();it!=diode_map.end();it++)
		delete it->second;
	*/
	//	
}
