// vikar.h
// Converted by FortranConvert v0.1
// Wed Feb 12 19:55:17 2014

#include <fstream>
#include <iostream>
#include <time.h>

#include "TFile.h"
#include "TTree.h"

#include "vikar_core.h"
#include "materials.h"
#include "detectors.h"
#include "structures.h"

#define VERSION "1.13d"

struct debugData{
	double var1, var2, var3;
	
	void Set(double v1, double v2, double v3){
		var1 = v1; var2 = v2; var3 = v3;
	}
};

// Get a random vector inside a cone approximating the beam shape
// spot_ is the beamspot size in m
// thick_ is the target thickness in m
// Zoffset_ is the distance from the center of the target to the beam focus point in cm
// beam is the vector pointing from the beam focus to the intersect point at the surface of the target
void RandomCone(double spot_, double Zoffset_, double thick_, Vector3 &beam){
	double ranR = std::sqrt(frand()) * (spot_/2.0); // Random distance from the beam axis
	double ranT = 2*pi*frand(); // Random angle about the beam axis
	beam = Vector3(ranR*std::cos(ranT), ranR*std::sin(ranT), Zoffset_-thick_/2.0);
}

// Get a random vector inside a perfectly cylindrical beam
// spot_ is the beamspot size in m
// thick_ is the target thickness in m
// beam is a 2d vector (z=0) pointing from the z-axis to the target surface intersect
void RandomCylinder(double spot_, Vector3 &beam){
	double ranR = std::sqrt(frand()) * (spot_/2.0); // Random distance from the beam axis
	double ranT = 2*pi*frand(); // Random angle about the beam axis
	beam = Vector3(ranR*std::cos(ranT), ranR*std::sin(ranT), -100);
}

// Get a random point on a circle
// spot_ is the beamspot size in m
// offset_ is the offset in the negative z-direction (in m)
// beam is a 2d vector in the xy-plane (z=0) pointing from the origin to a point inside the target beamspot
void RandomCircle(double spot_, double offset_, Vector3 &beam){
	double ranR = std::sqrt(frand()) * (spot_/2.0); // Random distance from the beam axis
	double ranT = 2*pi*frand(); // Random angle about the beam axis
	beam = Vector3(ranR*std::cos(ranT), ranR*std::sin(ranT), -offset_);
}

bool SetBool(std::string input_, std::string text_, bool &output){
	int idummy = atoi(input_.c_str());
	if(idummy == 1){ output = true; }
	else{ output = false; }
	std::cout << text_;
	if(output){ std::cout << ": Yes\n"; }
	else{ std::cout << ": No\n"; }
	return output;
}

bool Prompt(std::string prompt_){
	std::string temp_input;
	while(true){
		std::cout << prompt_ << " (yes/no) "; std::cin >> temp_input;
		if(temp_input == "yes" || temp_input == "y"){ return true;; }
		else if(temp_input == "no" || temp_input == "n"){ return false; }
		else{ std::cout << "  Type yes or no\n"; }
	}
}

int main(int argc, char* argv[]){ 
	// A Monte-Carlo charged particle experiment simulation program - details below
	//
	// vikar 1.0 written by S.D. Pain on some date in 2004
	//
	// vikar 2.0 updated by S.D. Pain on 5/5/2006
	//   - Updated to employ non-isotropic angular distributions
	//
	// vikar 3.0 last updated by S.D. Pain on 6/13/2010
	//   - Major structural reworking
	//   - Updated to employ charged particle processing subroutine
	//   - Fixed to set detector properties (RadLength, conv) for SRIM tables
	//   - Fixed ranges array size in SRIM tables and main to match
	//
	// vikar 3.1 last updated by S.D. Pain on 10/15/2013
	//   - Added beam energy spread (SRIM input unchecked for functionality)
	//   - Added beamspot size parameter in input file
	//   - Applied beamspot size variation to annular and cylindrical detectors (1st-order application)
	//   - Applied version check to input file read/write
	//
	// vikar 3.11 last updated by S.D. Pain on 12/11/2013
	//   - resolution_cyl updated (v1.0 to v1.1) to improve beam-spot-size effects
	//   - product_proc updated (v1.0 to v1.1) to improve beam-spot-size effects for annular detectors
	//
	// Outstanding things for future versions
	//----------------------------------------
	// Write relativistic conversion routines
	// Put in recoil breakup
	// Add energy straggling (see NIMS 117, 125 (1974))
	// Tilt annular detectors
	// Add in gamma-rays
	// Make strips selectively resistive/non-resistive
	// Allow different strips to be hit on dE and E detectors
	// Allow ejectile excitations
	// Put in user defined energy for position resolution point
	// Add user selected detector material
	// Add different detector materials
	// Fix stuck problems for detecting both ejectiles and recoils when no coincidence
	// is required
	
	// Seed randomizer
	srand(time(NULL));
	
	// Main objects
	Kindeux kind;
	Efficiency bar_eff;
	
	// temporary variables
	double hit_x, hit_y, hit_z;
	
	Vector3 Ejectile, Recoil, Gamma;
	Vector3 HitDetect1, HitDetect2;
	Vector3 EjectSphere, RecoilSphere;
	Vector3 lab_beam_focus; // The focus point for the beam. Non-cylindrical beam particles will originate from this point
	Vector3 lab_beam_start; // The originating point of the beam particle in the lab frame
	Vector3 lab_beam_trajectory; // The original trajectory of the beam particle before it enters the target
	Vector3 lab_beam_interaction; // The position of the reaction inside the target
	Vector3 lab_beam_stragtraject; // The angular straggled trajectory of the beam particle just before the reaction occurs
	Vector3 targ_surface; // The intersection point between the beam particle and the target surface (wrt beam focus)
	Vector3 interaction; // The interaction point inside the target (wrt beam focus)
	Matrix3 rotation_matrix; // The rotation matrix used to transform vectors from the beam particle frame to the lab frame

	double tof; // Neutron time of flight (s)
	double Zdepth; // Interaction depth inside of the target (cm)
	double Ereact; // Energy at which the reaction occurs (MeV)
	double range_beam;
	
	RangeTable beam_targ, eject_targ;
	RangeTable eject_det, recoil_targ;
	
	Target targ;
	
	// Physics Variables
	unsigned int NRecoilStates = 0;
	std::vector<std::string> AngDist_fname; 
	double *ExRecoilStates = NULL;
	double *totXsect = NULL; 
	double gsQvalue = 0.0;
	
	double Ebeam = 0.0, Ebeam0 = 0.0;
	double Erecoil = 0.0, Eeject = 0.0;
	double Abeam = 0.0, Zbeam = 0.0;
	double Arecoil = 0.0, Zrecoil = 0.0;
	double Aeject = 0.0, Zeject = 0.0;
	
	// Beam variables
	double beamspot = 0.0; // Beamspot diameter (m) (on the surface of the target)
	double beamEspread = 0.0; // Beam energy spread (MeV)
	double beamAngdiv = 0.0; // Beam angular divergence (radians)

	double timeRes = 3E-9;
	double BeamRate = 0.0;
	
	// Detector variables
	std::string det_fname; 
	Planar *vandle_bars;
	
	std::string output_fname_prefix = "VIKAR";
	
	// Input/output variables
	int face1, face2; // Detect which detector faces are hit
	unsigned int Ndetected = 0; // Total number of particles detected in VANDLE
	unsigned int Nwanted = 0; // Number of desired detections
	unsigned int Nsimulated = 0; // Total number of simulated particles
	unsigned int NbarHit = 0; // Total number of particles which collided with a bar
	unsigned int Nreactions = 0; // Total number of particles which react with the target
	clock_t timer;

	bool SimGamma = false;
	bool InCoincidence = true;
	bool WriteReaction = false;
	bool WriteDebug = false;

	unsigned int Ndet = 0;
	bool PerfectDet = true;
	bool DetSetup = true;
	bool ADists = false;
	bool SupplyRates = false;
	bool BeamFocus = false;
	bool TestSetup = false;

	// Error reporting variables
	std::string SRIM_fName, SRIM_fName_beam, SRIM_fName_targ_eject, SRIM_fName_targ_recoil;
	std::string SRIM_fName_det_eject, SRIM_fName_det_recoil;

	//------------------------------------------------------------------------
	//
	// Start of user input code
	//
	//------------------------------------------------------------------------

	std::cout << "\n####       #### ######## ####     ###         ###       #########      ########\n"; 
	std::cout << " ##         ##     ##     ##     ##          ## ##       ##     ##    ##      ##\n";
	std::cout << "  ##       ##      ##     ##    ##          ##   ##      ##     ##             ##\n";
	std::cout << "  ##       ##      ##     ##   ##           ##   ##      ##   ##              ##\n";
	std::cout << "   ##     ##       ##     #####            #########     #####               ##\n";
	std::cout << "   ##     ##       ##     ##   ##          ##     ##     ##   ##           ##\n";
	std::cout << "    ##   ##        ##     ##    ##        ##       ##    ##    ##         ##\n";
	std::cout << "    ##   ##        ##     ##     ##       ##       ##    ##     ##      ##\n";
	std::cout << "     ## ##         ##     ##      ##     ##         ##   ##      ##    ##\n";
	std::cout << "      ###       ######## ####      ###  ####       #### ####      ### ###########\n";

	std::cout << "\n VIKAR v " << VERSION << "\n"; 
	std::cout << " ==  ==  ==  ==  == \n\n"; 
	
	std::cout << " Welcome to NewVIKAR, the Virtual Instrumentation for Kinematics\n"; 
	std::cout << " And Reactions program, optimized for use with VANDLE bars\n\n";

	std::cout << " How about a nice cup of tea?\n"; 
	std::cout << " No? Well let me just load the input file then\n";
	std::cout << "\n ==  ==  ==  ==  == \n";
	sleep(3);

	std::ifstream input_file;
	if(argc >= 2){
		// Read an input file
		input_file.open(argv[1]);
		if(!input_file.good()){
			std::cout << " Error: Problem loading the input file\n";
			return 1;
		}
		
		// Specify the name of the output files
		if(argc >= 3){ output_fname_prefix = std::string(argv[2]); }
		
		unsigned int count = 0;
		std::string input;
		std::cout << "\n Reading from file " << argv[1] << std::endl;
		while(true){
			getline(input_file, input); input = Parse(input);
			if(input_file.eof()){ break; }
			if(input == ""){ continue; }
			
			if(count == 0){ 
				std::cout << "  Version: " << input << std::endl;
				if(input != VERSION){ 
					std::cout << "   Warning! This input file has the wrong version number. Check to make sure input is correct\n"; 
				}
			}
			else if(count == 1){ 
				Zbeam = atof(input.c_str());  
				std::cout << "  Beam-Z: " << Zbeam << std::endl;
			}
			else if(count == 2){ 
				Abeam = atof(input.c_str());  
				std::cout << "  Beam-A: " << Abeam << std::endl;
			}
			else if(count == 3){ 
				targ.SetZ((double)atof(input.c_str()));
				std::cout << "  Target-Z: " << targ.GetZ() << std::endl;
			}
			else if(count == 4){ 
				targ.SetA((double)atof(input.c_str()));
				std::cout << "  Target-A: " << targ.GetA() << std::endl;
			}
			else if(count == 5){ 
				Zeject = atof(input.c_str());
				Zrecoil = Zbeam + targ.GetZ() - Zeject;  
				std::cout << "  Ejectile-Z: " << Zeject << std::endl;
			}
			else if(count == 6){ 
				Aeject = atof(input.c_str()); 
				Arecoil = Abeam + targ.GetA() - Aeject; 
				std::cout << "  Ejectile-A: " << Aeject << std::endl;
				std::cout << "  Recoil-Z: " << Zrecoil << std::endl;
				std::cout << "  Recoil-A: " << Arecoil << std::endl;
			}
			else if(count == 7){ 
				Ebeam0 = atof(input.c_str());  
				std::cout << "  Beam Energy: " << Ebeam0 << " MeV\n";
			}
			else if(count == 8){ 
				beamspot = atof(input.c_str());  
				std::cout << "  Beam Spot Size: " << beamspot << " mm\n";
				beamspot = beamspot/1000.0; // in meters
			}
			else if(count == 9){ 
				beamAngdiv = atof(input.c_str());  
				std::cout << "  Beam Angular Divergence: " << beamAngdiv << " degrees\n";
				beamAngdiv *= deg2rad; // in radians
			}
			else if(count == 10){ 
				beamEspread = atof(input.c_str());  
				std::cout << "  Beam Spread: " << beamEspread << " MeV\n";
			}
			else if(count == 11){ 
				gsQvalue = atof(input.c_str());  
				std::cout << "  G.S. Q-Value: " << gsQvalue << " MeV\n";
			}
			else if(count == 12){ 
				// Recoil excited state information
				NRecoilStates = atoi(input.c_str()) + 1;
				std::cout << "  No. Excited States: " << NRecoilStates-1 << std::endl;
				ExRecoilStates = new double[NRecoilStates]; ExRecoilStates[0] = 0.0;
				totXsect = new double[NRecoilStates]; totXsect[0] = 0.0;
				std::cout << "   Recoil Ground State: 0.0 MeV\n";
				for(unsigned int i = 1; i < NRecoilStates; i++){
					getline(input_file, input); input = Parse(input);
					ExRecoilStates[i] = atof(input.c_str());
					std::cout << "   Recoil Excited State " << i << ": " << ExRecoilStates[i] << " MeV\n";
					totXsect[i] = 0.0;
				}
			}
			else if(count == 13){ 
				// Angular distribution information
				if(SetBool(input, "  Supply Angular Distributions", ADists)){
					for(unsigned int i = 0; i < NRecoilStates; i++){
						getline(input_file, input); input = Parse(input);
						AngDist_fname.push_back(input);
						if(i == 0){ std::cout << "   Distribution for ground state: " << AngDist_fname[i] << std::endl; }
						else{ std::cout << "   Distribution for state " << i+1 << ": " << AngDist_fname[i] << std::endl; }
					}
					
					// Supply beam rate information
					getline(input_file, input); input = Parse(input);
					if(SetBool(input, "  Calculate Rates", SupplyRates)){ 
						getline(input_file, input); input = Parse(input);
						BeamRate = atof(input.c_str());
						std::cout << "   Beam Rate: " << BeamRate << " pps\n";
					}
					else{ BeamRate = 0.0; }
				}
				else{ SupplyRates = false; }
			}
			else if(count == 14){ 
				// Target thickness
				targ.SetThickness((double)atof(input.c_str()));
				std::cout << "  Target Thickness: " << targ.GetThickness() << " mg/cm^2\n";			
			}
			else if(count == 15){ 
				// Target density
				targ.SetDensity((double)atof(input.c_str()));
				std::cout << "  Target Density: " << targ.GetDensity() << " g/cm^3\n";
			}
			else if(count == 16){ 
				// Target angle wrt beam axis
				targ.SetAngle((double)atof(input.c_str())*deg2rad);
				std::cout << "  Target Angle: " << targ.GetAngle()*rad2deg << " degrees\n";
			}
			else if(count == 17){ 
				// Target information
				unsigned int num_elements = (unsigned int)atoi(input.c_str());
				unsigned int num_per_molecule[num_elements];
				double element_Z[num_elements];
				double element_A[num_elements];
				
				std::cout << "  No. Target Elements: " << num_elements << std::endl;
				for(unsigned int i = 0; i < num_elements; i++){
					getline(input_file, input); input = Parse(input); element_Z[i] = (double)atof(input.c_str()); 
					getline(input_file, input); input = Parse(input); element_A[i] = (double)atof(input.c_str());  
					getline(input_file, input); input = Parse(input); num_per_molecule[i] = (unsigned int)atoi(input.c_str()); 
					std::cout << "   Element " << i+1 << ": " << num_per_molecule[i] << " per molecule of Z = " << element_Z[i] << ", A = " << element_A[i] << std::endl;
				}
				
				targ.Init(num_elements);
				targ.SetElements(num_per_molecule, element_Z, element_A);
				std::cout << "  Target Radiation Length: " << targ.GetRadLength() << " mg/cm^2\n";
			}
			else if(count == 18){ 
				// Load the small, medium, and large bar efficiencies
				// Efficiency index 0 is the underflow efficiency (for energies below E[0])
				// Efficiency index N is the overflow efficiency (for energies greater than E[N])
				if(!SetBool(input, "  Perfect Detector", PerfectDet)){ 
					// Load small bar efficiency data
					getline(input_file, input); input = Parse(input); 
					std::cout << "   Found " << bar_eff.ReadSmall(input.c_str()) << " small bar data points in file " << input << "\n";
					
					// Load medium bar efficiency data
					getline(input_file, input); input = Parse(input); 
					std::cout << "   Found " << bar_eff.ReadMedium(input.c_str()) << " medium bar data points in file " << input << "\n";
					
					// Load large bar efficiency data
					getline(input_file, input); input = Parse(input); 
					std::cout << "   Found " << bar_eff.ReadLarge(input.c_str()) << " large bar data points in file " << input << "\n";
				}
			}
			else if(count == 19){ 
				// Supply detector setup file?
				if(SetBool(input, "  Detector Setup File", DetSetup)){
					// Load detector setup from a file
					getline(input_file, det_fname); det_fname = Parse(det_fname);
					std::cout << "   Path: " << det_fname << std::endl;
				}
			}
			else if(count == 20){ 
				// Desired number of detections
				Nwanted = atol(input.c_str());
				std::cout << "  Desired Detections: " << Nwanted << std::endl; 
			}
			else if(count == 21){
				// Simulate prompt gamma flash?
				SetBool(input, "  Detect Prompt Gammas", SimGamma);
			}
			else if(count == 22){
				// Require ejectile and recoil particle coincidence?
				SetBool(input, "  Require particle coincidence", InCoincidence);
			}
			else if(count == 23){
				// Write Reaction data to file?
				SetBool(input, "  Write Reaction Info", WriteReaction);
			}
			else if(count == 24){
				// Write Debug data to file?
				SetBool(input, "  Write Debug Info", WriteDebug);
			}
			else if(count == 25){
				// Perform monte carlo simulation on detector setup?
				SetBool(input, "  Test Detector Setup", TestSetup);
			}
			
			count++;
		}
		
		input_file.close();
		if(count < 24){
			std::cout << " Error: The input file is invalid. Check to make sure input is correct\n";
			return 1;
		}
	}
	else{
		std::cout << " Error: Missing required variable\n";
		return 1;
	}
		
	std::cout << "\n ==  ==  ==  ==  == \n\n";

	// Make sure the input variables are correct
	if(!Prompt(" Are the above settings correct?")){
		std::cout << "  ABORTING...\n";
		return 1;
	}

	// Read VIKAR detector setup file or manually setup simple systems
	if(DetSetup){
		std::cout << "\n Reading in NewVIKAR detector setup file...\n";
		std::ifstream detfile(det_fname.c_str());
		if(!detfile.good()){ return 0; }
		
		std::vector<NewVIKARDet> detectors;
		std::string line;
	
		while(true){
			getline(detfile, line);
			if(detfile.eof()){ break; }
			if(line[0] == '#'){ continue; } // Commented line
		
			detectors.push_back(NewVIKARDet(line));
		}	
		detfile.close();
	
		// Generate the Planar bar arrays
		vandle_bars = new Planar[detectors.size()];
		
		// Fill the detector arrays
		Ndet = 0;
		for(std::vector<NewVIKARDet>::iterator iter = detectors.begin(); iter != detectors.end(); iter++){
			if(iter->subtype == "small"){ vandle_bars[Ndet].SetSmall(); }
			else if(iter->subtype == "medium"){ vandle_bars[Ndet].SetMedium(); }
			else if(iter->subtype == "large"){ vandle_bars[Ndet].SetLarge(); }
			else{ vandle_bars[Ndet].SetSize(iter->data[6],iter->data[7],iter->data[8]); }
		
			vandle_bars[Ndet].SetPosition(iter->data[0],iter->data[1],iter->data[2]); // Set the x,y,z position of the bar
			vandle_bars[Ndet].SetRotation(iter->data[3],iter->data[4],iter->data[5]); // Set the 3d rotation of the bar
			vandle_bars[Ndet].SetType(iter->type);
			vandle_bars[Ndet].SetSubtype(iter->subtype);
			if(iter->subtype == "cylinder"){ vandle_bars[Ndet].SetCylinder(); }
			Ndet++;
		}

		// Report on how many detectors were read in*/
		std::cout << " Found " << Ndet << " detectors in file " << det_fname << std::endl;

		// Check there's at least 1 detector!
		if(Ndet< 1){
			std::cout << " Error: Found no detectors. Check that the filename is correct\n"; 
			return 1;
		}
		
		if(TestSetup){
			std::cout << "  Performing Monte Carlo efficency test...\n";
			unsigned int total_found = TestDetSetup(vandle_bars, Ndet, Nwanted);
			std::cout << "  Found " << Nwanted << " events in " << total_found << " trials (" << 100.0*Nwanted/total_found << "%)\n";
			std::cout << "  Wrote lab position data to 'xyz.dat' and detector face data to 'faces.dat'\n";
			std::cout << " Finished geometric efficiency test on detector setup...\n";
			if(!Prompt(" Do you wish to continue with the simulation?")){
				std::cout << "  ABORTING...\n";
				return 1;
			}
		}
	}
	else{
		// Manual detector setup (fix later)
		std::cout << "\n Not Implemented!\n";
		return 1;
	}

	// Calculate the beam focal point (if it exists)
	if(beamAngdiv < pi/2.0){
		lab_beam_focus = Vector3(0.0, 0.0, -((beamspot/2.0)*std::tan(beamAngdiv)+targ.GetRealZthickness()/2.0));
		std::cout << " Beam focal point at Z = " << lab_beam_focus.axis[2] << " m\n";
		BeamFocus = true;
	}

	// For cylindrical beams, the beam direction is givn by the z-axis
	if(!BeamFocus){ lab_beam_trajectory = Vector3(0.0, 0.0, 1.0); }

	// Calculate the stopping power table for the beam particles in the target
	if(Zbeam > 0){ // The beam is a charged particle (not a neutron)
		std::cout << " Calculating range tables for beam in target...";
		beam_targ.Init(100, 0.1, (Ebeam0+2*beamEspread), targ.GetDensity(), targ.GetAverageA(), targ.GetAverageZ(), Abeam, Zbeam, &targ);
		std::cout << " done\n";
	}
	
	// Calculate the stopping power table for the ejectiles in the target
	if(Zeject > 0){ // The ejectile is a charged particle (not a neutron)
		std::cout << " Calculating range tables for ejectile in target...";
		eject_targ.Init(100, 0.1, (Ebeam0+2*beamEspread), targ.GetDensity(), targ.GetAverageA(), targ.GetAverageZ(), Aeject, Zeject, &targ);
		std::cout << " done\n";
		
		// NOT IMPLEMENTED!
		/*// Set detector material to Si. Add user selection at some point
		Adet = 28; 
		Zdet = 14; 
		density_det = 2.3212; 
		density_det = density_det*1000.0; // Convert density to mg/cm^3
		conv_det = 1.0e-4*density_det; 
		DetRadL = radlength(Adet,Zdet);
	
		// Calculate the stopping power table for the ejectiles in the detector
		std::cout << " Calculating range tables for ejectile in detector..."; 
		eject_det.Init(100);
		step = (Ebeam0+2*beamEspread)/99.0;
		for(unsigned int i = 0; i < 100; i++){ 
			ncdedx(tgtdens, Adet, Zdet, Aeject, Zeject, i*step, dummy1, dummy2, rangemg);
			eject_det.Set(i, i*step, rangemg/conv_det);
		} 
		std::cout << " done\n";*/
	}
	
	// Calculate the stopping power table for the recoils in the target
	if(Zrecoil > 0){ // The recoil is a charged particle (not a neutron)
		std::cout << " Calculating range tables for recoil in target...";
		recoil_targ.Init(100, 0.1, (Ebeam0+2*beamEspread), targ.GetDensity(), targ.GetAverageA(), targ.GetAverageZ(), Arecoil, Zrecoil, &targ);
		std::cout << " done\n";
	}

	//std::cout << "\n ==  ==  ==  ==  == \n\n";
	std::cout << "\n Initializing main simulation Kindeux object...\n";

	// Initialize kinematics object
	kind.Initialize(Abeam, targ.GetA(), Arecoil, Aeject, gsQvalue, NRecoilStates, ExRecoilStates, targ.GetDensity());
	if(ADists){ 
		std::cout << " Loading state angular distribution files...\n";
		if(kind.SetDist(AngDist_fname, targ.GetTotalElements(), BeamRate)){
			// Successfully set the angular distributions
			std::cout << " Successfully loaded angular distributions\n";
			kind.Print();
		}
		else{
			std::cout << "  Warning! Could not properly initialize distributions.\n";
			std::cout << "  Note: Setting all energy states to isotropic!\n";
		}
	}

	std::cout << "\n ==  ==  ==  ==  == \n\n";

	//---------------------------------------------------------------------------
	// End of Input Section
	//---------------------------------------------------------------------------
		
	// Root stuff
	TFile *file = new TFile("VIKAR.root", "RECREATE");
	TTree *VIKARtree = new TTree("VIKAR", "VIKAR output tree");
	TTree *DEBUGtree = NULL;
	
	EjectObject EJECTdata;
	RecoilObject RECOILdata;
	ReactionObject REACTIONdata;
	debugData DEBUGdata;
	
	VIKARtree->Branch("Eject", &EJECTdata);
	VIKARtree->Branch("Recoil", &RECOILdata);
	if(WriteReaction){
		VIKARtree->Branch("Reaction", &REACTIONdata);
	}
	if(WriteDebug){ 
		DEBUGtree = new TTree("DEBUG", "VIKAR debug tree");
		DEBUGtree->Branch("Debug", &DEBUGdata, "var1/D:var2/D:var3/D"); 
	}

	// Begin the simulation
	std::cout << " ---------- Simulation Setup Complete -----------\n"; 
	std::cout << "\n Beginning simulating " << Nwanted << " events....\n"; 

	//---------------------------------------------------------------------------
	// The Event Loop
	// ==  ==  ==  ==  ==  ==  == 
	// (Just to make it obvious)
	//---------------------------------------------------------------------------

	Vector3 temp_vector;
	Vector3 temp_vector_sphere;
	double dist_traveled, QDC;
	double penetration, fpath1, fpath2;
	float totTime = 0.0;
	char counter = 1;
	bool flag = false;
	bool hit = false;
	unsigned int chunk = Nwanted/10;
	unsigned int beam_stopped = 0;
	unsigned int recoil_stopped = 0;
	unsigned int eject_stopped = 0;
	timer = clock();
	
	while(Ndetected < Nwanted){
		// ****************Time Estimate**************
		if(flag && (Ndetected % chunk == 0)){
			flag = false;
			totTime = (float)(clock()-timer)/CLOCKS_PER_SEC;
			std::cout << "\n ------------------------------------------------\n"; 
			std::cout << " Number of particles Simulated: " << Nsimulated << std::endl; 
			std::cout << " Number of particles Detected: " << Ndetected << std::endl; 
			if(SupplyRates && ADists){ std::cout << " Number of Reactions: " << Nreactions << std::endl; }
			
			std::cout << " " << Ndetected*100.0/Nwanted << "% of simulation complete...\n"; 
			if(PerfectDet){ std::cout << "  Detection Efficiency: " << Ndetected*100.0/Nreactions << "%\n"; }
			else{
				std::cout << "  Geometric Efficiency: " << NbarHit*100.0/Nreactions << "%\n";
				std::cout << "  Detection Efficiency: " << Ndetected*100.0/Nreactions << "%\n"; 
			}
			if(SupplyRates){ std::cout << "  Beam Time: " << Nsimulated/BeamRate << " seconds\n"; }
			
			std::cout << "  Simulation Time: " << totTime << " seconds\n";
			std::cout << "  Time reamining: " << (totTime/counter)*(10-counter) << " seconds\n";
			counter++; 
		}
		Nsimulated++; 
		
		// Simulate a beam particle before entering the target
		// Randomly select a point uniformly distributed on the beamspot
		// Calculate where the beam particle reacts inside the target
		// beamspot as well as the distance traversed through the target
		if(BeamFocus){ 
			// In this case, lab_beam_focus is the originating point of the beam particle
			// The direction is given by the cartesian vector 'lab_beam_trajectory'
			RandomCircle(beamspot, lab_beam_focus.axis[2], lab_beam_trajectory);
			Zdepth = targ.GetInteractionDepth(lab_beam_focus, lab_beam_trajectory, targ_surface, lab_beam_interaction);
		}
		else{ 
			// In this case, lab_beam_start stores the originating point of the beam particle
			// The direction is given simply by the +z-axis
			RandomCircle(beamspot, 1.0, lab_beam_start); // The 1m offset ensures the particle originates outside the target
			Zdepth = targ.GetInteractionDepth(lab_beam_start, lab_beam_trajectory, targ_surface, lab_beam_interaction);
		}	

		// Calculate the beam particle energy, varied with energy spread (in MeV)
		Ebeam = Ebeam0 + rndgauss0(beamEspread); 

		// Calculate the beam particle range in the target (in m)
		range_beam = beam_targ.GetRange(Ebeam);
		
		// Calculate the new energy
		if(range_beam - Zdepth <= 0.0){ // The beam stops in the target (no reaction)
			if(beam_stopped == 10000){
				std::cout << " ATTENTION!\n";
				std::cout << "  A large number of beam particles (" << 100.0*beam_stopped/Nsimulated << "%) have stopped in the target!\n";
				std::cout << "  A high percentage of stopped particles could mean that the target is too thick.\n";
				std::cout << "  If this is the case, change the target thickness and restart the simulation.\n";
			}
			beam_stopped++;
			
			continue; 
		}
		Ereact = beam_targ.GetEnergy(range_beam - Zdepth);
		
		// Determine the angle of the beam particle's trajectory at the
		// interaction point, due to angular straggling and the incident trajectory.
		targ.AngleStraggling(lab_beam_trajectory, Abeam, Zbeam, Ebeam, lab_beam_stragtraject);

		// the 2 body kinematics routine to generate the ejectile and recoil
		if(kind.FillVars(Ereact, Eeject, Erecoil, EjectSphere, RecoilSphere)){ Nreactions++; }
		else{ continue; } // A reaction did not occur

		// Convert the reaction vectors to cartesian coordinates
		// EjectSphere and RecoilSphere are unit vectors (no need to normalize)
		Sphere2Cart(EjectSphere, Ejectile); 
		Sphere2Cart(RecoilSphere, Recoil);
		
		// Transform the ejectile and recoil vectors (cartesian) from the beam trajectory frame into the Lab frame
		// This transformation will overwrite the Ejectile and Recoil vectors
		//rotation_matrix.SetRotationMatrixCart(lab_beam_trajectory); // Turn OFF angular straggling effects
		rotation_matrix.SetRotationMatrixCart(lab_beam_stragtraject); // Turn ON angular straggling effects
		rotation_matrix.Transform(Ejectile);
		rotation_matrix.Transform(Recoil);

		// Calculate the energy loss for the ejectile and recoils in the target
		/*if(Zeject > 0){
			double range_eject = eject_targ.GetRange(Eeject);
		}
		if(Zrecoil > 0){
			double range_recoil = recoil_targ.GetRange(Erecoil);
		}*/
		
		if(WriteDebug){ 
			DEBUGdata.Set(Ejectile.axis[0], Ejectile.axis[1], Ejectile.axis[2]);
			DEBUGtree->Fill();
		}

		// Gammas are indistinguishable from the ejectile (to a VANDLE bar)
		/*if(SimGamma && RecoilState > 0){
			// Simulate gamma decays for excited recoils
			double gamma_theta, gamma_phi;
			UnitSphereRandom(gamma_theta, gamma_phi);
		}*/

		// Process the reaction products
		for(unsigned int bar = 0; bar < Ndet; bar++){
			if(!vandle_bars[bar].IsRecoilDet()){ // This is a detector used to detect ejectiles (VANDLE)
				hit = vandle_bars[bar].IntersectPrimitive(lab_beam_interaction, Ejectile, HitDetect1, HitDetect2, face1, face2, hit_x, hit_y, hit_z);
				if(hit){ NbarHit++; } // Geometric hit detected
			}
			else{ // This is a detector used to detect recoils (ION, SCINT, etc)
				hit = vandle_bars[bar].IntersectPrimitive(lab_beam_interaction, Recoil, HitDetect1, HitDetect2, face1, face2, hit_x, hit_y, hit_z);
			}
			
			if(hit){			
				// Check for a "true" hit
				if(!PerfectDet){
					// Detector is not perfect, hit is based on the efficiency
					if(vandle_bars[bar].IsSmall()){
						// Use the small bar efficiency data
						if(frand() > bar_eff.GetSmallEfficiency(Eeject)){ hit = false; }
					}
					else if(vandle_bars[bar].IsMedium()){
						// Use the medium bar efficiency data
						if(frand() > bar_eff.GetMediumEfficiency(Eeject)){ hit = false; }
					}
					else if(vandle_bars[bar].IsLarge()){
						// Use the large bar efficiency data
						if(frand() > bar_eff.GetLargeEfficiency(Eeject)){ hit = false; }
					}
				}
			
				if(hit){
					// The particle hit a detector and was detected
					// The time of flight is the time it takes the particle to traverse the distance
					// from the target to the intersection point inside the detector
					fpath1 = HitDetect1.Length(); // Distance from reaction to first intersection point
					fpath2 = HitDetect2.Length(); // Distance from reaction to second intersection point
					penetration = frand(); // The fraction of the bar which the neutron travels through
					temp_vector = (HitDetect2-HitDetect1); // The vector pointing from the first intersection point to the second
					dist_traveled = temp_vector.Length()*penetration; // Random distance traveled through detector

					// Calculate the total distance traveled and the interaction point inside the detector
					if(fpath1 <= fpath2){ 
						dist_traveled += fpath1; 
						temp_vector = lab_beam_interaction + HitDetect1 + temp_vector*penetration;
					}
					else{ 
						dist_traveled += fpath2; 
						temp_vector = lab_beam_interaction + HitDetect2 - temp_vector*penetration;
					}
				
					// Calculate the particle ToF (ns)
					if(!vandle_bars[bar].IsRecoilDet()){
						tof = dist_traveled*std::sqrt(kind.GetMeject()/(2*Eeject*1.60217657E-13*6.02214129E26));
						QDC = Eeject*frand(); // The ejectile may leave any portion of its energy inside the detector
					}
					else{
						tof = dist_traveled*std::sqrt(kind.GetMrecoil()/(2*Erecoil*1.60217657E-13*6.02214129E26));
						QDC = Erecoil*frand(); // The recoil may leave any portion of its energy inside the detector
					}

					// Smear ToF and Energy if the detector is not perfect
					if(!PerfectDet){
						tof += rndgauss0(timeRes); // Smear tof due to PIXIE resolution
						//QDC += rndgauss0(qdcRes); // Smear the VANDLE QDC value
						//energyRes = std::sqrt(pow((0.03/HitDetect.Length()), 2.0)+pow((timeRes/tof), 2.0));
						//Eeject += rndgauss0(energyRes*Eeject); // Smear energy due to VANDLE resolution
					}
			
					// Main output
					// X(m) Y(m) Z(m) LabTheta(deg) LabPhi(deg) QDC(MeV) ToF(ns) Bar# Face# HitX(m) HitY(m) HitZ(m)
					if(QDC >= 0.1 && QDC <= 5.0){
						if(!vandle_bars[bar].IsRecoilDet()){
							Cart2Sphere(temp_vector, EjectSphere); // Ignore normalization, we're going to throw away R anyway
							EJECTdata.Append(temp_vector.axis[0], temp_vector.axis[1], temp_vector.axis[2], EjectSphere.axis[1]*rad2deg,
											 EjectSphere.axis[2]*rad2deg, QDC, tof*(1E9), hit_x, hit_y, hit_z, bar);
						}
						else{
							Cart2Sphere(temp_vector, RecoilSphere); // Ignore normalization, we're going to throw away R anyway
							RECOILdata.Append(temp_vector.axis[0], temp_vector.axis[1], temp_vector.axis[2], EjectSphere.axis[1]*rad2deg,
											 EjectSphere.axis[2]*rad2deg, QDC, tof*(1E9), hit_x, hit_y, hit_z, bar);
						}
					}
				} // if(hit)
			} // if(vandle_bars[bar].IntersectPrimitive())
		} // for(unsigned int bar = 0; bar < Ndet; bar++)
		if(InCoincidence){ // We require coincidence between ejectiles and recoils 
			if(EJECTdata.eject_mult > 0 && RECOILdata.recoil_mult > 0){ 
				if(!flag){ flag = true; }
				if(WriteReaction){
					REACTIONdata.Append(Ereact, lab_beam_interaction.axis[0], lab_beam_interaction.axis[1], lab_beam_interaction.axis[2],
										lab_beam_stragtraject.axis[0], lab_beam_stragtraject.axis[1], lab_beam_stragtraject.axis[2]);
				}
				VIKARtree->Fill(); 
				Ndetected++;
			}
		}
		else{ // Coincidence is not required between reaction particles
			if(EJECTdata.eject_mult > 0 || RECOILdata.recoil_mult > 0){ 
				if(!flag){ flag = true; }
				if(WriteReaction){
					REACTIONdata.Append(Ereact, lab_beam_interaction.axis[0], lab_beam_interaction.axis[1], lab_beam_interaction.axis[2],
										lab_beam_stragtraject.axis[0], lab_beam_stragtraject.axis[1], lab_beam_stragtraject.axis[2]);
				}
				VIKARtree->Fill(); 
				Ndetected++;
			}
		}
		EJECTdata.Zero();
		RECOILdata.Zero();
		if(WriteReaction){ REACTIONdata.Zero(); }
	} // Main simulation loop
	// ==  ==  ==  ==  ==  ==  == 
	
	// Information output and cleanup
	std::cout << "\n ------------- Simulation Complete --------------\n";
	std::cout << " Simulation Time: " << (float)(clock()-timer)/CLOCKS_PER_SEC << " seconds\n"; 
	std::cout << " Geometric Efficiency: " << NbarHit*100.0/Nreactions << "%\n";
	std::cout << " Detection Efficiency: " << Ndetected*100.0/Nreactions << "%\n";
	if(beam_stopped > 0 || eject_stopped > 0 || recoil_stopped > 0){
		std::cout << " Particles Stopped in Target:\n";
		if(beam_stopped > 0){ std::cout << "  Beam: " << beam_stopped << " (" << 100.0*beam_stopped/Nsimulated << "%)\n"; }
		if(eject_stopped > 0){ std::cout << "  Ejectiles: " << eject_stopped << " (" << 100.0*eject_stopped/Nsimulated << "%)\n"; }
		if(recoil_stopped > 0){ std::cout << "  Recoils: " << recoil_stopped << " (" << 100.0*recoil_stopped/Nsimulated << "%)\n"; }
	}
	if(SupplyRates){ std::cout << " Beam Time: " << Nsimulated/BeamRate << " seconds\n"; }
	
	file->cd();
	VIKARtree->Write();
	if(DEBUGtree){ DEBUGtree->Write(); }
	
	std::cout << "  Wrote file " << output_fname_prefix << ".root\n";
	std::cout << "   Wrote " << VIKARtree->GetEntries() << " tree entries for VIKAR\n";
	if(WriteDebug){ std::cout << "   Wrote " << DEBUGtree->GetEntries() << " tree entries for DEBUG\n"; }
	file->Close();
	delete file;
} 
