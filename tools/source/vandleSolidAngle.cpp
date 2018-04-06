#include <iostream>
#include <sstream>
#include <fstream>

#include "vandmc_core.h"
#include "detectors.h"
#include "materials.h"
#include "Structures.h"

#include "TFile.h"
#include "TTree.h"
#include "TH1D.h"
#include "TNamed.h"

Vector3 zero_vector(0.0, 0.0, 0.0);

class comConverter{
  public:
	comConverter() : length(0) { }

	comConverter(const char *fname) : length(0) { 
		load(fname);
	}

	bool load(const char *fname){
		com.clear();
		ejectLab.clear();
		recoilLab.clear();
		length = 0;

		std::ifstream file(fname);
		if(!file.good()) return false;

		double comVal, ejectLabVal, recoilLabVal;
		while(true){
			file >> comVal >> ejectLabVal >> recoilLabVal;
			if(file.eof()) break;
			com.push_back(comVal);
			ejectLab.push_back(ejectLabVal);
			recoilLab.push_back(recoilLabVal);
		}

		length = com.size();
		return (length != 0);
	}

	double convertEject2lab(const double &com_){
		double retval = -9999;
		Interpolate(com_, retval, com.data(), ejectLab.data(), length);
		return retval;
	}

	double convertRecoil2lab(const double &com_){
		double retval = -9999;
		Interpolate(com_, retval, com.data(), recoilLab.data(), length);
		return retval;
	}

  private:
	std::vector<double> com;
	std::vector<double> ejectLab;
	std::vector<double> recoilLab;
	size_t length;
};

class DataPack{
  public:
	TFile *file;
	TTree *tree;
	bool init;

	double offsetX, offsetY, offsetZ;
	double trajX, trajY, trajZ;
	double Ereact, Eeject, Erecoil;
	double labTheta, labPhi;
	double comAngle;
	int labBin;

	MonteCarloStructure MCARLOdata;

	DataPack(){
		file = NULL;
		tree = NULL;
		init = false;
	}
	
	DataPack(std::string fname_, unsigned int Nbins_, bool write_rxn_=false){
		Open(fname_, write_rxn_);
	}

	~DataPack(){
		if(init){ Close(); }
	}

	bool IsInit(){ return init; }

	bool Open(std::string fname_, bool write_rxn_){
		if(init){ return false; }

		file = new TFile(fname_.c_str(), "RECREATE");
		
		if(!file->IsOpen()){
			init = false;
			delete file;
		}
		
		tree = new TTree("data", "Monte carlo detector efficiency tree");
		
		tree->Branch("mcarlo", &MCARLOdata);
		if(write_rxn_){ 
			tree->Branch("labTheta", &labTheta);
			tree->Branch("labPhi", &labPhi);
			tree->Branch("comAngle", &comAngle);
			tree->Branch("labBin", &labBin); 
		}
		
		return (init = true);
	}

	bool Close(){
		if(!init){ return false; }
		
		file->cd();
		tree->Write();
		file->Close();
		
		init = false;
		
		std::cout << "  Wrote monte carlo file 'mcarlo.root'\n";
		
		return true;
	}
	
	void Zero(){
		MCARLOdata.Zero();
	}
};

double proper_value(const std::string &prompt_, const double &min_=0.0, bool ge_=false){
	double output = -1;
	while(true){
		std::cout << " " << prompt_;
		std::cin >> output;
		if(!ge_){
			if(output > min_){ break; }
			std::cout << "  Error: Invalid value! Input must be > " << min_ << ".\n";
		}
		else{
			if(output >= min_){ break; }
			std::cout << "  Error: Invalid value! Input must be >= " << min_ << ".\n";
		}
	}
	return output;
}

// Perform a monte carlo simulation on an arbitrary configuration
// of detectors from an array. Returns the number of hits detected
// Generates one output root file named 'mcarlo.root'
// fwhm_ (m) allows the use of a gaussian particle "source". If fwhm_ == 0.0, a point source is used
// angle_ (rad) allows the rotation of the particle source about the y-axis
unsigned int TestDetSetup(DataPack *pack, const std::vector<double> &barAngles, const std::vector<double> &angleBins, const double &radius_, unsigned int num_trials, bool WriteRXN_, comConverter *conv=0x0){
	if(!pack){ return 0; }
	double hitTheta=0, hitPhi;
	double comAngle;
	unsigned int count=0;
	unsigned int totalGenerated=0;
	unsigned int totalInAngleBin=0;
	Vector3 flight_path;
	Vector3 temp_ray;
	
	bool useKinematics = (conv != 0x0);
	
	unsigned int num_trials_chunk = num_trials/10;
	unsigned int chunk_num = 1;
	double t, x, y, z;

	const double width = 0.03; // m
	const double barHalfAngle = std::asin(width/(2*radius_));

	// Compute mask angles.
	const double dTheta = 1; // deg
	const double angles[3] = {barHalfAngle-dTheta*deg2rad, barHalfAngle, barHalfAngle+dTheta*deg2rad};

	while(count < num_trials){
		if(count != 0 && count == num_trials_chunk*chunk_num){ // Print a status update.
			std::cout << "  " << (chunk_num++)*10 << "% - " << "Detected " << count << " of " << totalGenerated << " total events (" << count*100.0/totalGenerated << "%)\n";
		}
	
		if(useKinematics){ // Generate a uniformly distributed random point on the unit sphere in the center-of-mass frame.
			UnitSphereRandom(comAngle, hitPhi); // In the CM frame.
			hitTheta = conv->convertEject2lab(comAngle);
			Sphere2Cart(1.0, hitTheta, hitPhi, temp_ray); // Theta is now in the lab frame.
		}
		else{ // Generate a uniformly distributed random point on the unit sphere in the lab frame.
			UnitSphereRandom(temp_ray);
			comAngle = temp_ray.axis[1];
		}

		totalGenerated++;

		// Check CM angle.
		if(comAngle >= pi/2) continue;

		// Check angular bin.
		int bin = -1;
		for(size_t i = 0; i < angleBins.size()-1; i++){
			if(hitTheta >= angleBins[i] && hitTheta < angleBins[i+1]){
				bin = (int)i;
				break;
			}
		}

		totalInAngleBin++;

		if(WriteRXN_){
			pack->labTheta = hitTheta*rad2deg;
			pack->labPhi = hitPhi*rad2deg;
			pack->comAngle = comAngle*rad2deg;
			pack->labBin = bin;
		}

		t = radius_/std::sqrt(temp_ray.axis[0]*temp_ray.axis[0]+temp_ray.axis[2]*temp_ray.axis[2]);
		x = t*temp_ray.axis[0];
		y = t*temp_ray.axis[1];
		z = t*temp_ray.axis[2];
		
		// Check for events which do not intersect VANDLE.
		if(y >= -0.3 && y <= 0.3 && x >= 0){
			// Check for detector hit.
			int loc = -1;
			int mask = -1;
			for(size_t i = 0; i < barAngles.size(); i++){
				if(hitTheta >= barAngles[i]-angles[2] && hitTheta <= barAngles[i]+angles[2]){
					loc = i;
					if(hitTheta < barAngles[i]){
						if(hitTheta < barAngles[i]-angles[1]) mask = 0;
						else if(hitTheta < barAngles[i]-angles[0]) mask = 1;
						else mask = 2;
					}
					else{
						if(hitTheta >= barAngles[i]+angles[1]) mask = 4;
						else if(hitTheta >= barAngles[i]+angles[0]) mask = 3;
						else mask = 2;
					}
					break;
				}
			}

			if(loc >= 0 && mask >= 0){
				pack->MCARLOdata.Append(x, y, z, 0, 0, 0, hitTheta*rad2deg, hitPhi*rad2deg, 0, 0, loc, mask);
				count++;
			}
		}

		pack->tree->Fill();
		pack->MCARLOdata.Zero();
	}

	return totalGenerated;
}

void help(char * prog_name_){
	std::cout << "  SYNTAX: " << prog_name_ << " <detfile> <binfile> [relfile]\n";
}

int main(int argc, char *argv[]){
	if(argc < 3){
		std::cout << " Error: Invalid number of arguments to " << argv[0] << ". Expected 2, received " << argc-1 << ".\n";
		help(argv[0]);
		return 1;
	}

	unsigned int Nwanted = 0;
	unsigned int totalGenerated = 0;

	std::vector<Primitive*> detectors;

	std::cout << " Reading in NewVIKAR detector setup file...\n";
	int Ndet = ReadDetFile(argv[1], detectors);
	if(Ndet < 0){
		std::cout << " Error: failed to load detector setup file!\n";
		return 1; 
	}
	else if(Ndet == 0){ std::cout << " Error: Found no detectors in the detector setup file!\n"; }

	std::cout << "  Loaded " << Ndet << " detectors from file.\n";

	std::cout << " Reading angular bin file...\n";
	std::ifstream binFile(argv[2]);
	if(!binFile.good()){
		std::cout << " Error: Failed to load angular bin file!\n";
		return 1;
	}
	
	double labTheta, comTheta;
	std::vector<double> angularBins;
	while(true){
		binFile >> labTheta >> comTheta;
		if(binFile.eof()) break;
		angularBins.push_back(labTheta);
	}
	binFile.close();

	std::cout << "  Loaded " << angularBins.size()-1 << " angular bins.\n"; 

	std::vector<double> detectorAngles;
	for(std::vector<Primitive*>::iterator iter = detectors.begin(); iter != detectors.end(); iter++){
		detectorAngles.push_back((*iter)->GetTheta());
	}

	bool WriteReaction = false;
	std::cout << " Write reaction data? "; std::cin >> WriteReaction; 

	double radius;
	std::cout << " Enter detector radius (m): "; std::cin >> radius;
	
	DataPack pack;
	
	comConverter *conv = NULL;
	if(argc >= 4){
		std::string rxnFilename(argv[3]);
		conv = new comConverter();
		if(!conv->load(rxnFilename.c_str())){
			std::cout << " Error: failed to load input kinematics file \"" << rxnFilename << "\"!\n";
			return 1;
		}
	}

	pack.Open("mcarlo.root", WriteReaction);

	std::cout << std::endl;

	Nwanted = (unsigned int)proper_value("Enter number of ejectile MC events: ", 0.0, true);

	if(Nwanted > 0){
		std::cout << "  Performing Monte Carlo test on ejectile detectors...\n";
		totalGenerated = TestDetSetup(&pack, detectorAngles, angularBins, radius, Nwanted, WriteReaction, conv);
	
		std::cout << "  Found " << Nwanted << " ejectile events in " << totalGenerated << " trials (" << 100.0*Nwanted/totalGenerated << "%)\n\n";

		std::stringstream stream; stream << Nwanted;
		TNamed n1("EjectDet", stream.str().c_str());
		stream.str(""); stream << totalGenerated;
		TNamed n2("EjectTot", stream.str().c_str());
		stream.str(""); stream << 100.0*Nwanted/totalGenerated << " %";
		TNamed n3("EjectEff", stream.str().c_str());

		pack.file->cd();
		n1.Write();
		n2.Write();
		n3.Write();
	}

	std::cout << " Finished geometric efficiency test on detector setup...\n";
	
	return 0;
}
