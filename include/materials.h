// materials.h
// Cory Thornsberry

#ifndef MATERIALS_H
#define MATERIALS_H

#include "detectors.h"

/////////////////////////////////////////////////////////////////////
// Globals
/////////////////////////////////////////////////////////////////////

// Constants for use in stopping power calcuations
extern const double electron_RME, proton_RME, neutron_RME;
extern const double bohr_e_radius, e, h, coeff;
extern const double alpha, avagadro;
extern const double amu2mev;

// Constant coefficients for the shell correction term
extern const double a1, a2, b1, b2, c1, c2;

// Ionization potentials
extern const double ionpot[13];

/////////////////////////////////////////////////////////////////////
// Class declarations
/////////////////////////////////////////////////////////////////////

class Primitive;
class Efficiency;
class Material;
class Particle;
class Target;
class RangeTable;

/////////////////////////////////////////////////////////////////////
// Efficiency
/////////////////////////////////////////////////////////////////////

class Efficiency{
  private:
	double *small_energy, *small_efficiency;
	double *med_energy, *med_efficiency;
	double *large_energy, *large_efficiency;
	unsigned int NsmallEff, NmedEff, NlargeEff;
	bool init_small, init_med, init_large;
	
	bool _read_eff_file(const char* fname, std::vector<double> &energy, std::vector<double> &efficiency);
	
  public:
	Efficiency();
	~Efficiency();
	
	unsigned int GetNsmall(){ return NsmallEff; }
	unsigned int GetNmedium(){ return NmedEff; }
	unsigned int GetNlarge(){ return NlargeEff; }
	
	bool IsSmallInit(){ return init_small; }
	bool IsMediumInit(){ return init_med; }
	bool IsLargeInit(){ return init_large; }
	
	unsigned int ReadSmall(const char*);
	unsigned int ReadMedium(const char*);
	unsigned int ReadLarge(const char*);
	double GetSmallEfficiency(double);
	double GetMediumEfficiency(double);
	double GetLargeEfficiency(double);
};

/////////////////////////////////////////////////////////////////////
// Material
/////////////////////////////////////////////////////////////////////

class Material{
  protected:
	std::string vikar_name; // The name vikar uses to search for this material
  	unsigned int num_elements; // Number of unique elements per molecule of the material
  	unsigned int total_elements; // Total number of elements per molecule in the material
  	unsigned int *num_per_molecule; // Number of each element per molecule
  	double *element_Z, *element_A; // Atomic numbers and atomic masses (u) for each element
  	double *element_I; // Ionization potentials (MeV) 
  	double avgZ, avgA; // Average Z and A of the elements within the material molecule
	double density; // Density of the material (g/cm^3)
	double Mmass; // Molar mass of material (g/mol)
	double edens; // Electron density (1/m^3)
	double rad_length; // The radiation length of the material (mg/cm^2)
	double lnIbar; // The natural log of the average ionization potential
	bool init;
	bool use_eloss;
	
	void _initialize();

	void _calculate(); // Calculate the average atomic charge, mass, and ionization potential for the material

	// Return the effective atomic charge (unitless) for a given value of beta_ and Z_
	double _zeff(double beta_, double Z_){ return Z_*(1-std::exp(-125.0*beta_/pow(Z_, 2.0/3.0))); }

	// Return the value of beta^2 (unitless) for a particle with a given energy_ and mass_
	// energy_ in MeV in (kinetic energy) and mass_ in MeV/c^2 (rest mass energy)
	double _b2(double energy_, double mass_){ return 1-pow(mass_/(energy_+mass_), 2.0); }
	
	// Return the energy (MeV) for a particle with a given beta2_ and mass_
	// mass_ in MeV/c^2 (rest mass energy)
	double _energy(double beta2_, double mass_){ return mass_*((1.0/std::sqrt(1.0-beta2_))-1.0); }

	// Return the ionization potential (MeV) for a particle with a given atomic charge Z_
	// Z_ is the atomic number of the ion of interest
	double _ionpot(unsigned int Z_);

	// Return the shell correction term for a particle with a given energy_ in this material
	// energy_ in MeV
	double _shell(double energy_);

	// Return the density effect correction term (unitless) for a particle with a given energy_ in this material
	// energy_ in MeV
	double _density(double energy_);

	// Return the stopping power for a proton with a given KE (energy_) in this material
	// energy_ in MeV
	double _pstop(double energy_);

	// Return the range (m) for a proton with given KE (energy_) in this material
	// energy_ in MeV
	double _prange(double energy_, unsigned int num_iterations_);

  public:
  	Material();
  	Material(unsigned int);
	~Material();
	
	bool Init(unsigned int);
	
	void SetUseFlag(bool state_=true){ use_eloss = state_; }
	void SetDensity(double density_){ density = density_; }
	void SetMolarMass(double Mmass_){ Mmass = Mmass_; }
	void SetElements(unsigned int*, double*, double*);
	void SetName(std::string name_){ vikar_name = name_; }

	bool IsInit(){ return init; }
	
	double GetAverageZ(){ return avgZ; } // Return the average atomic number of the elements in the molecule
	double GetAverageA(){ return avgA; } // Return the average atomic mass of the elements in the molecule
	double GetRadLength(){ return rad_length; } // Return the radiation length of the material (mg/cm^2)
	double GetDensity(){ return density; } // Return the density of the material (g/cm^3)
	double GetEdensity(){ return edens; } // Return the electron density (1/m^3) for the material
	double GetLNibar(){ return lnIbar; } // Return the natural log of the average ionization potential	
	double GetMolarMass(){ return Mmass; } // Return the molar mass of the material (g/mol)
	std::string GetName(){ return vikar_name; }

	unsigned int GetTotalElements(){ return total_elements; } // Return the total number of elements in the material molecule
	unsigned int GetNumElements(){ return num_elements; } // Return the number of unique elements per material molecule
	
	// Read a material file
	bool ReadMatFile(const char* filename_);
	
	// Return the stopping power (MeV/m) for a particle with given energy_, Z_, and mass_ in this material
	// energy_ in MeV and mass_ in MeV/c^2
	double StopPower(double energy_, double Z_, double mass_);

	// Return the range (m) for a particle with given energy_, Z_, and mass_, in this material
	// energy_ in MeV and mass_ in MeV/c^2
	double Range(double energy_, double Z_, double mass_);
	
	// Use Birks' equation to calculate the light output for a particle at a given energy_ in this material
	// energy_ in MeV, mass_ in MeV/c^2, L0_ in 1/MeV, kB_ in m/MeV, and C_ in (m/MeV)^2
	double Birks(double energy_, double Z_, double mass_, double L0_, double kB_, double C_);
	
	void Print();
	
	void Print(std::ofstream *file_);
};

/////////////////////////////////////////////////////////////////////
// RangeTable
/////////////////////////////////////////////////////////////////////

class RangeTable{
  private:
	double *energy, *range;
	unsigned int num_entries;
	bool use_table;
	
	bool _initialize(unsigned int);
	
  public:
	RangeTable(){ use_table = false; }
	RangeTable(unsigned int);
	~RangeTable();
	
	bool Init(unsigned int); // Initialize arrays but do not fill them
	bool Init(unsigned int num_entries_, double startE_, double stopE_, double Z_, double mass_, Material *mat_); // Initialize arrays and fill them using Material
	bool UseTable(){ return use_table; }
	bool Set(unsigned int, double, double); // Manually set a data point with an energy and a range
	unsigned int GetEntries(){ return num_entries; } // Return the number of entries in the array
	double GetRange(double energy_); // Get the particle range at a given energy using linear interpolation
	double GetEnergy(double range_); // Get the particle energy at a given range usign linear interpolation
	double GetNewE(double energy_, double dist_, double &dist_traveled); // Get the energy loss of a particle traversing a distance through a material
	bool GetEntry(unsigned int entry_, double &E, double &R){
		if(!use_table || entry_ >= num_entries){ return false; }
		E = energy[entry_]; R = range[entry_];
		return true;
	}
	void Print(); // Print values to the screen
};

/////////////////////////////////////////////////////////////////////
// Particle
/////////////////////////////////////////////////////////////////////

class Particle{
  protected:
	double A, Z; /// Mass and charge number of the particle
	double maxE; /// Maximum energy in the range table
	double mass; /// Rest mass energy of the particle (MeV/c^2)
	std::string name; /// The name of the particle.
	bool init; /// True if the material and range table have been initialized.
	
	RangeTable table; /// The range table to use for energy loss calculations.
	Material *mat; /// The material to use for energy loss calculations.
	
  public:
	Particle(){ 
		A = 0.0; Z = 0.0; maxE = 0.0; mass = 0.0;
		name = "unknown"; init = false; 
	}
	Particle(const std::string &name_, const double &Z_, const double &A_, const double &BE_A_=0.0){ 
		SetParticle(name_, Z_, A_, BE_A_); 
		init = false;
	}
	
	/** Set the mass number of the particle. This does NOT set 
	  * the actual mass of the particle. To do that, call SetMass().
	  */
	void SetA(const double &A_){ A = A_; }
	
	/// Set the charge number of the particle.
	void SetZ(const double &Z_){ Z = Z_; }
	
	/// Auto-set the mass of the particle in units of MeV/c^2. Assumes that Z and A have been set.
	void SetMass(const double &BE_A_=0.0){ mass = Z*proton_RME + (A-Z)*neutron_RME - BE_A_*A; }
	
	/// Manually set the mass of the particle in units of MeV/c^2.
	void SetMassMeV(const double &mass_){ mass = mass_; }
	
	/// Manually set the mass of the particle in units of amu.
	void SetMassAMU(const double &mass_){ mass = mass_*amu2mev; } 
	
	/// Setup the particle name, charge number, and mass.
	void SetParticle(const std::string &name_, const double &Z_, const double &A_, const double &BE_A_=0.0){
		Z = Z_; A = A_; name = name_; 
		SetMass(BE_A_);
	} 
	
	/// Set the name of the particle.
	void SetName(std::string name_){ name = name_; }
	
	/** Set the material for the particle to use for energy loss calculations.
	  * This also sets up the range table.
	  */
	bool SetMaterial(Material *mat_, const double &Ebeam_, const double &Espread_);
	
	double GetA(){ return A; } /// Return the atomic mass of the particle
	double GetZ(){ return Z; } /// Return the atomic charge of the particle
	double GetN(){ return A-Z; } /// Return the number of neutrons
	double GetMass(){ return mass; } /// Return the rest mass of the particle (MeV)
	double GetMassAMU(){ return mass/amu2mev; } /// Return the rest mass of the particle (amu).
	double GetMaxE(){ return maxE; } /// Return the maximum particle energy (MeV)
	std::string GetName(){ return name; } /// Return the name of the particle.
	Material *GetMaterial(){ return mat; } /// Return a pointer to the energy loss material.
	RangeTable *GetTable(){ return &table; } /// Return a pointer to the range table.
	
	/// Get the relativistic factor of the particle (unitless).
	double GetGamma(const double &velocity_){ return 1.0/std::sqrt(1.0-velocity_*velocity_/(c*c)); }
	
	/// Get the kinetic energy from the total energy (MeV).
	double GetKEfromTE(const double &energy_){ return (energy_ - mass); }	
	
	/// Get the total energy from the kinetic energy (MeV).
	double GetTEfromKE(const double &energy_){ return (energy_ + mass); }
	
	/// Get the kinetic energy from the velocity of the particle (MeV).
	double GetKEfromV(const double &velocity_);
	
	/// Get the total energy from the velocity of the particle (MeV).
	double GetTEfromV(const double &velocity_);
	
	/// Get the relativistic momentum from the total energy of the particle (MeV/c).
	double GetPfromTE(const double &energy_){ return std::sqrt(energy_*energy_ - mass*mass); }
	
	/// Get the relativistic momentum from the kinetic energy of the particle (MeV/c).
	double GetPfromKE(const double &energy_){ return GetPfromTE(energy_+mass); }

	/// Get the relativistic momentum from the velocity of the particle (MeV/c).
	double GetPfromV(const double &velocity_){ return GetGamma(velocity_)*mass*velocity_/c; }

	/// Get the relativistic velocity from the total energy of the particle (m/s).
	double GetVfromTE(const double &energy_){ return GetVfromKE(energy_-mass); }
	
	/// Get the relativistic velocity from the kinetic energy of the particle (m/s).
	double GetVfromKE(const double &energy_){ return c*std::sqrt(1.0 - std::pow(1.0/(1+energy_/mass), 2.0)); }
	
	/// Get the energy of a particle stopped in distance range_.
	double GetTableEnergy(const double &range_);
	
	/// Get the range of the particle given an initial energy_.
	double GetTableRange(const double &energy_);
	
	/** Get the final energy of a particle with energy_ moving a
	  * distance dist_ through the material.
	  */
	double GetTableNewE(const double &energy_, const double &dist_, double &dist_traveled);
};

/////////////////////////////////////////////////////////////////////
// Target
/////////////////////////////////////////////////////////////////////

class Target : public Particle {
  private:
	double thickness; // Thickness of the target (mg/cm^2)
	double Zthickness; // Thickness of the target in the z-direction (mg/cm^2)
	double density; // Density of the target (g/cm^3)
	double rad_length; // The radiation length of the material (mg/cm^2)
	double angle; // Angle of target wrt the beam axis (rad)
	
	Primitive *physical; // The physical target geometry
	
  public:
	Target();
	Target(unsigned int);

	void SetThickness(double thickness_);
	void SetAngle(double angle_);
	void SetDensity(double density_);
	void SetRadLength(double rad_length_){ rad_length = rad_length_; }
	
	double GetThickness(){ return thickness; } // Return the thickness of the target (mg/cm^2)
	double GetZthickness(){ return Zthickness; } // Return the thickness the beam sees (mg/cm^2)
	double GetRealThickness(){ return thickness/(density*1E5); } // Return the physical thickness of the target (m)
	double GetRealZthickness(){ return Zthickness/(density*1E5); } // Return the physical thickness the beam sees (m)
	double GetAngle(){ return angle; } // Return the angle of the target wrt the beam axis
	double GetDensity(){ return density; }
	double GetRadLength(){ return rad_length; }
	Primitive *GetPrimitive(){ return physical; } // Return a pointer to the 3d geometry object
	
	// Get the depth into the target at which the reaction occurs
	// offset_ is the global position where the beam particle originates
	// direction_ is the direction of the beam particle entering the target
	// intersect is the global position where the beam particle intersects the front face of the target
	// interact is the global position where the beam particle reacts inside the target
	double GetInteractionDepth(const Vector3 &offset_, const Vector3 &direction_, Vector3 &intersect, Vector3 &interact);

	// Determine the new direction of a particle inside the target due to angular straggling
	bool AngleStraggling(const Vector3 &direction_, double A_, double Z, double E_, Vector3 &new_direction);
};

#endif
