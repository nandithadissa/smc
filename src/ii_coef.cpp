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
   ii_coef.cpp contains ii_coef() which calculates the impact ionization coefficients for a given material.

   Takes user input for Minimum,Maximum and Step size for Electric Fields.
   Takes material input from main.cpp

   Prototyped in model.h

   Uses the Classes SMC & tools.
   Also uses functions.h which contains common functions used in all three modes.
   Calculates the ionization coefficents (Alpha & Beta) by tracking the movement for an electron and a hole
   for 20000 impact ionization events.
   Distance moved between impact ionization events is output to epdf and hpdf for each Electric field strength.
   Alpha and Beta are calculated as 1/(mean between ionization events) and output to abMean.

   Jonathan Petticrew, University of Sheffield, 2017.
 */

#include "model.h"
#include "SMC.h"
#include "functions.h"
#include "tools.h"
#include <stdio.h>
#include <math.h>
//#include <tchar.h>
#include <string.h>

void ii_coef(int material){
	SMC constants; //SMC parameter set
	constants.mat(material); //Tells SMC parameter set which material
	SMC *pointSMC = &constants; //Used to pass constants to other classes.
	double minEfield, maxEfield, stepEfield;
	printf(" Minimum Electric Field (kV/cm):\n");
	scanf("%lf",&minEfield);
	printf(" Maximum Electric Field (kV/cm):\n");
	scanf("%lf",&maxEfield);
	printf("Electric Field Step Size (kV/cm):\n");
	scanf("%lf",&stepEfield);
	tools simulation(pointSMC);
	simulation.scattering_probability();//this function returns 0 if no output can be generated and the user wants to quit
	sgenrand(4358);//seeds the random number generator constant used to alow for comparison using different parameters.
	double Esim,Eloop,z_pos,kf,kxy,kz,cos_theta,Energy;
	int tn;
	int scat_e=0;
	FILE *about;
	about=fopen("alpha_beta.txt","w");
	fprintf(about,"Efield (kV/cm),  Alpha (1/m), Beta (1/m)\n");

	/*** EFIELD LOOP STARTS HERE ***/
	for(Esim=minEfield; Esim<=maxEfield; Esim+=stepEfield) {
		//Generate the output files for the electric field
		FILE *epdf;
		FILE *hpdf;
		char ename[] = "epdf.txt";
		char hname[] = "hpdf.txt";
		char Eprint[5];
		snprintf(Eprint,sizeof(Eprint),"%g",Esim);
		int efile_len = strlen(ename) + strlen(Eprint) + 1;
		char* efile = new char[efile_len];
		int hfile_len = strlen(hname) + strlen(Eprint) + 1;
		char* hfile = new char[hfile_len];
		snprintf(efile,efile_len,"%s%s",Eprint,ename);
		snprintf(hfile,hfile_len,"%s%s",Eprint,hname);
		epdf=fopen(efile,"w");
		hpdf=fopen(hfile,"w");
		delete[] efile, hfile;
		//Reset variables to 0.
		Eloop=Esim*1e5; //change elecric field from kV/cm to V/m.
		z_pos=0;
		Energy=0;
		kf=0; //Kf^2=Kx^2+Ky^2+Kz^2
		kxy=0;// combined momentum tangential to travel direction.
		kz=0; // z momentum (direction of travel)
		cos_theta=0; //scattering angle
		tn=0; //impact ionization counter
		double drift_t=0;
		double dE=0;
		double alpha_distance=0;
		double beta_distance=0;

		//electrons
		while(tn<20000) { //loops for 20000 electron impact ionization events.
			if(scat_e==0) {
				double cos_theta;
				kf=2*constants.Get_e_mass()*Energy/(constants.Get_hbar()*constants.Get_hbar());
				if(kf>=0) {
					cos_theta=2*genrand()-1;
					kz=cos_theta*sqrt(kf);
					kxy=kf*(1-cos_theta*cos_theta);
				}
			}
			//electron drift process starts
			double random1;
			random1=genrand();
			drift_t= -log(random1)/(simulation.Get_rtotal());
			kz+=(constants.Get_q()*drift_t*Eloop)/(constants.Get_hbar());
			dE=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_e_mass()))*(kxy+kz*kz)-Energy;
			Energy=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_e_mass()))*(kxy+kz*kz);
			z_pos+=dE/(constants.Get_q()*Eloop);
			//electron drift process ends

			//electron scattering process starts
			double random2;
			int Eint;
			Eint=(int)floor(Energy*1000.0/constants.Get_q()+0.5); //bins energy to compare against probability curves from tools class.
			if (Energy>constants.Get_Emax()) {
				Eint= constants.Get_NUMPOINTS();
				random2=simulation.Get_pb(2,constants.Get_NUMPOINTS());
			}
			else if (Energy == constants.Get_Emax()) random2=simulation.Get_pb(2,constants.Get_NUMPOINTS());
			else random2=genrand();

			if(random2<=simulation.Get_pb(0,Eint)) //phonon absorption
			{   Energy+=constants.Get_hw();
				scat_e=0;}
			else if(random2<=simulation.Get_pb(1,Eint)) //phonon emission
			{   Energy-=constants.Get_hw();
				scat_e=0;}
			else if(random2<=simulation.Get_pb(2,Eint)) //impact ionization
			{   Energy=(Energy-constants.Get_e_Eth())/3.0;
				tn++;
				scat_e=0;
				fprintf(epdf,"%d %e\n", tn, z_pos);
				alpha_distance+=z_pos;
				z_pos=0;}
			else if(random2>simulation.Get_pb(2,Eint)) //selfscattering
			{    scat_e=1;}
			//electron scattering process ends

		}

		//reset variables to 0
		z_pos=0;
		Energy=0;
		kf=0;
		kxy=0;
		kz=0;
		cos_theta=0;
		tn=0;
		drift_t=0;
		dE=0;

		//holes
		while(tn<20000) {// loops for 20000 impact ionization events for holes.
			if(scat_e==0) {
				double cos_theta;
				kf=2*constants.Get_h_mass()*Energy/(constants.Get_hbar()*constants.Get_hbar());
				if(kf>=0) {
					cos_theta=2*genrand()-1;
					kz=cos_theta*sqrt(kf);
					kxy=kf*(1-cos_theta*cos_theta);
				}
			}
			//hole drift process starts
			double random11;
			random11=genrand();
			drift_t= -log(random11)/simulation.Get_rtotal2();
			kz-=((constants.Get_q()*drift_t*Eloop)/(constants.Get_hbar()));
			dE=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_h_mass()))*(kxy+kz*kz)-Energy;
			Energy=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_h_mass()))*(kxy+kz*kz);
			z_pos-=dE/(Eloop*constants.Get_q());
			//hole drift process ends

			// hole scattering process starts
			double random22;
			int Eint2;
			Eint2=(int)floor(Energy*1000.0/constants.Get_q()+0.5);
			if (Energy>constants.Get_Emax()) {
				Eint2= constants.Get_NUMPOINTS();
				random22=simulation.Get_pb2(2,constants.Get_NUMPOINTS());
			}
			else if (Energy == constants.Get_Emax()) random22=simulation.Get_pb2(2,constants.Get_NUMPOINTS());
			else random22=genrand();

			if(random22<=simulation.Get_pb2(0,Eint2)) //phonon absorption
			{   Energy+=constants.Get_hw();
				scat_e=0;}
			else if(random22<=simulation.Get_pb2(1,Eint2)) //phonon emission
			{   Energy-=constants.Get_hw();
				scat_e=0;}
			else if(random22<=simulation.Get_pb2(2,Eint2)) //impact ionization
			{   Energy=(Energy-constants.Get_h_Eth())/3.0;
				tn++;
				scat_e=0;
				fprintf(hpdf,"%d %e\n", tn, -z_pos);
				beta_distance-=z_pos;
				z_pos=0;}
			else if(random22>simulation.Get_pb2(2,Eint2)) //selfscattering
			{    scat_e=1;}
			//hole scattering process ends

		}

		fclose(epdf);
		fclose(hpdf);
		// convert distance travelled to alpha, beta and output to file.
		double alpha=tn/alpha_distance;
		double beta=tn/beta_distance;
		fprintf(about, "%lf %e %e\n", Esim, alpha, beta);
	}
	fclose(about);
}
