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
   main.cpp contains the decleration of main for the Simple Monte Carlo Simulator.
   It requests material and mode inputs from the user before running the requested mode.
   Jonathan Petticrew, University of Sheffield, 2017.
 */

#include <stdio.h>
#include "model.h"
#include <stdlib.h>

//#include <conio.h>
int main(int argc, char** argv){

	if (argc <2)
	{
		printf("enter the resistance\n");
		return -1;
	}

	char *enp;

	//request material from user
	int material=1;
	//printf("Material: 1) Si, 2) GaAs, 3) InGaP\n");
	//scanf("%d",&material);

	//requests model from user
	int calc=1;
	//printf("Mode: 1) Diode Properties, 2) Drift Velocity, 3) Impact Ionization Coefficients\n");
	//scanf("%d", &calc);

	//enter the series resistance of the for the SPAD
	double resister;
	resister = strtod(argv[1],&enp);
	//printf("Enter series resister:\n");
	//scanf("%lf", &resister);
	printf("resister %lf entered\n",resister);


	//runs user specified model
	if (calc==1) device_properties(material,resister);
	else if (calc==2) drift_velocity(material);
	else if (calc==3) ii_coef(material);

	printf("Done\n");

/*
	int inputkey;
	printf("Press space to exit\n");
	while((inputkey=getc())==0);
*/
	return 0;

}
