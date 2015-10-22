// geometry.cpp
// Cory Thornsberry

#include "geometry.h"
#include "detectors.h"

/////////////////////////////////////////////////////////////////////
// RegularPolygon
/////////////////////////////////////////////////////////////////////

/// Default constructor.
RegularPolygon::RegularPolygon(){
	nSides = 0;
	sector = 0.0;
	radius = 0.0;
	chord_length = 0.0;
	lines = NULL;
	init = false;
}

/** Polygon constructor. radius_ is the radius of a circle
  * which is completely bound within the polygon (in m) and
  * nSides_ is the number of sides of the polygon.
  */
bool RegularPolygon::Initialize(const double &radius_, const unsigned int &nSides_){
	if(init){ return false; }
	
	nSides = nSides_;
	sector = 2.0*pi/nSides;
	radius = radius_/std::cos(sector/2.0);
	chord_length = 2.0*radius*std::sin(sector/2.0);
	
	lines = new Line[nSides];
	
	double theta = -sector/2.0;
	for(unsigned int side = 0; side < nSides; side++){
		lines[side].p1 = Vector3(radius*std::cos(theta), radius*std::sin(theta));
		theta += sector;
		lines[side].p2 = Vector3(radius*std::cos(theta), radius*std::sin(theta));
		lines[side].dir = (lines[side].p2 - lines[side].p1);
	}
	
	return (init = true);
}

/** Return true if the point pt_ is contained within the polygon or the
  * point lies on one of its line segments and return false otherwise.
  */
bool RegularPolygon::IsInside(const Vector3 &pt_){
	return (IsInside(pt_.axis[0], pt_.axis[1]));
}

/** Return true if the point pt_ is contained within the polygon or the
  * point lies on one of its line segments and return false otherwise.
  */
bool RegularPolygon::IsInside(const double &x_, const double &y_){
	Ray trace_ray(x_, y_, x_+1, y_); // Infinite ray along the x-axis.
	Vector3 dummy_vector;
	
	int intersects = 0;
	for(unsigned int side = 0; side < nSides; side++){
		if(lines[side].Intersect(trace_ray, dummy_vector)){ 
			intersects++; 
		}
	}
	
	return (intersects%2!=0?true:false);
}

/** Dump information about the line segments of the polygon.
  *  line# p1x p1y p2x p2y
  */
void RegularPolygon::Dump(){
	for(unsigned int side = 0; side < nSides; side++){
		std::cout << side << "\t" << lines[side].p1.axis[0] << "\t" << lines[side].p1.axis[1] << "\t";
		std::cout << lines[side].p2.axis[0] << "\t" << lines[side].p2.axis[1] << "\n";
	}
}

/////////////////////////////////////////////////////////////////////
// Primitive Class
/////////////////////////////////////////////////////////////////////

// +X is defined as beam-right
// +Y is defined as the vertical axis
// +Z is defined as the beam axis

/// Default constructor.
Primitive::Primitive(){
	material_id = 0;
	front_face = 0;
	back_face = 2;
	location = 0;
	detX = Vector3(1.0, 0.0, 0.0);
	detY = Vector3(0.0, 1.0, 0.0);
	detZ = Vector3(0.0, 0.0, 1.0);
	length = 1.0;
	width = 1.0;
	depth = 1.0;
	small = false;
	med = false;
	large = false;
	need_set = true;
	use_recoil = false;
	use_eject = false;
	use_material = false;
	type = "unknown";
	subtype = "unknown";
	material_name = "";
}

/// Constructor using a NewVIKARDet object.
Primitive::Primitive(NewVIKARdet *det_){
	material_id = 0;
	front_face = 0;
	back_face = 2;
	location = det_->location;
	detX = Vector3(1.0, 0.0, 0.0);
	detY = Vector3(0.0, 1.0, 0.0);
	detZ = Vector3(0.0, 0.0, 1.0);
	SetPosition(det_->data[0], det_->data[1], det_->data[2]);
	SetRotation(det_->data[3], det_->data[4], det_->data[5]);
	small = false;
	med = false;
	large = false;
	need_set = true;
	if(det_->type=="vandle"){
		if(det_->subtype=="small"){ SetSmall(); }
		else if(det_->subtype=="medium"){ SetMedium(); }
		else if(det_->subtype=="large"){ SetLarge(); }
		else{
			std::cout << " Primitive: Unrecognized VANDLE subtype (" << det_->subtype << ")!\n";
			SetSize(det_->data[6], det_->data[7], det_->data[8]);
		}
	}
	else{ SetSize(det_->data[6], det_->data[7], det_->data[8]); }
	use_recoil = (det_->type=="dual" || det_->type=="recoil");
	use_eject = (det_->type=="vandle" || det_->type=="dual" || det_->type=="eject");
	use_material = false;
	type = det_->type;
	subtype = det_->subtype;
	material_name = det_->material;	
}

/** Set the global face coordinates (wrt global origin)
  * Each vertex is the center coordinate of one of the faces.
  */
void Primitive::_set_face_coords(){
	// Calculate the center points of each face
	GlobalFace[0] = position-detZ*(depth/2.0); // Front face
	GlobalFace[1] = position+detX*(width/2.0); // Right face
	GlobalFace[2] = position+detZ*(depth/2.0); // Back face
	GlobalFace[3] = position-detX*(width/2.0); // Left face
	GlobalFace[4] = position+detY*(length/2.0); // Top face
	GlobalFace[5] = position-detY*(length/2.0); // Bottom face

	need_set = false;
}

/// Return the unit vector of one of the faces. Returns the zero vector if the face is undefined
void Primitive::GetUnitVector(unsigned int face_, Vector3 &unit){
	if(face_ == 0){ unit = detZ; } // +z local axis face (front)
	else if(face_ == 1){ unit = detX; } // +x local axis (right)
	else if(face_ == 2){ unit = detZ; unit *= -1; } // -z local axis (back)
	else if(face_ == 3){ unit = detX; unit *= -1; } // -x local axis (left)
	else if(face_ == 4){ unit = detY; } // +y local axis (top)
	else if(face_ == 5){ unit = detY; unit *= -1; } // -y local axis (bottom)
	else{ unit = Vector3(0.0, 0.0, 0.0); }
}	

/// Return the local detector frame coordinates of a global coordinate
void Primitive::GetLocalCoords(const Vector3 &world_coords, double &x, double &y, double &z){
	Vector3 temp = (world_coords - position);
	x = temp.Dot(detX);
	y = temp.Dot(detY);
	z = temp.Dot(detZ);
}

/// Get a vector pointing to a 3d point inside of this geometry
void Primitive::GetRandomPointInside(Vector3& output){
	// Get a random point inside the 3d detector
	Vector3 temp = Vector3(frand(-length/2, length/2), frand(-width/2, width/2), frand(-depth/2, depth/2));
	
	// Rotate the random point based on the detector rotation
	rotationMatrix.Transform(temp);
	
	// Get the vector pointing from the origin to the random point
	output = position + temp;
}

/// Set the front and rear face ID for this detector.
void Primitive::SetFrontFace(unsigned int front_face_){ 
	if(front_face_ == 0){ back_face = 2; }
	else if(front_face_ == 1){ back_face = 3; }
	else if(front_face_ == 2){ back_face = 0; }
	else if(front_face_ == 3){ back_face = 1; }
	else if(front_face_ == 4){ back_face = 5; }
	else if(front_face_ == 5){ back_face = 4; }
	else{ return; }
	
	front_face = front_face_;
}

/** Set the physical size of the detector.
  * For setting VANDLE bars, it is better to use SetSmall/SetMedium/SetLarge methods.
  * Unknown detector types will not include efficiency data.
  */
void Primitive::SetSize(double length_, double width_, double depth_){
	if(length_ == 0.6 && width_ == 0.03 && depth_ == 0.03){ SetSmall(); }
	else if(length_ == 1.2 && width_ == 0.05 && depth_ == 0.03){ SetMedium(); }
	else if(length_ == 2.0 && width_ == 0.05 && depth_ == 0.05){ SetLarge(); }
	else{
		width = width_; length = length_; depth = depth_; 
		small = false; med = false; large = false; need_set = true;
	}
}

/// Set the position of the center of the detector using a 3d vector (in meters).
void Primitive::SetPosition(const Vector3 &pos){
	position = pos;
	need_set = true;
}

/// Set the cartesian position of the center of the detector in 3d space (in meters).
void Primitive::SetPosition(double x, double y, double z){
	position.axis[0] = x; position.axis[1] = y; position.axis[2] = z;
	need_set = true;
}

/// Set the polar position of the center of the detector in 3d space (meters and radians).
void Primitive::SetPolarPosition(double r, double theta, double phi){
	Sphere2Cart(r, theta, phi, position);
	need_set = true;
}

/** Get the local unit vectors using 3d matrix rotation
  * X and Y are face axes, Z is the axis into or out of the detector.
  */
void Primitive::SetRotation(double theta_, double phi_, double psi_){
	theta = theta_; phi = phi_; psi = psi_;
	
	double sin_theta = std::sin(theta); double cos_theta = std::cos(theta);
	double sin_phi = std::sin(phi); double cos_phi = std::cos(phi);
	double sin_psi = std::sin(psi); double cos_psi = std::cos(psi);	
	
	// Pitch-Roll-Yaw convention
	// Rotate by angle theta about the y-axis
	//  angle phi about the z-axis
	//  angle psi about the x-axis
	detX = Vector3(cos_theta*cos_phi, cos_theta*sin_phi, -sin_theta); // Width axis
	detY = Vector3(sin_psi*sin_theta*cos_phi-cos_psi*sin_phi, sin_psi*sin_theta*sin_phi+cos_psi*cos_phi, cos_theta*sin_psi); // Length axis
	detZ = Vector3(cos_psi*sin_theta*cos_phi+sin_psi*sin_phi, cos_psi*sin_theta*sin_phi-sin_psi*cos_phi, cos_theta*cos_psi); // Depth axis
	
	// Normalize the unit vectors
	detX.Normalize(); detY.Normalize(); detZ.Normalize();
	need_set = true;
	
	// Set the rotation matrix
	rotationMatrix.SetUnitX(detX);
	rotationMatrix.SetUnitY(detY);
	rotationMatrix.SetUnitZ(detZ);
}

/** Manually set the local detector unit vectors
  * I would advise against this, use SetRotation instead
  * Note: This method does not update the detector rotation values
  *  and should therefore only be used for testing and debugging.
  */
void Primitive::SetUnitVectors(const Vector3 &unitX, const Vector3 &unitY, const Vector3 &unitZ){
	detX = unitX; detY = unitY; detZ = unitZ;
	
	detX.Normalize(); detY.Normalize(); detZ.Normalize();
	need_set = true;
	
	// Set the rotation matrix
	rotationMatrix.SetUnitX(detX);
	rotationMatrix.SetUnitY(detY);
	rotationMatrix.SetUnitZ(detZ);
}

/** Check if a point (in local coordinates) is within the bounds of the primitive
  * Return true if the coordinates are within the primitive and false otherwise.
  */
bool Primitive::CheckBounds(const unsigned int &face_, const double &x_, const double &y_, const double &z_){
	if(face_ == 0 || face_ == 2){ // Front face (upstream) or back face (downstream)
		if((x_ >= -width/2.0 && x_ <= width/2.0) && (y_ >= -length/2.0 && y_ <= length/2.0)){ return true; }
	}
	else if(face_ == 1 || face_ == 3){ // Right face (beam-right) or left face (beam-left)
		if((z_ >= -depth/2.0 && z_ <= depth/2.0) && (y_ >= -length/2.0 && y_ <= length/2.0)){ return true; }
	}
	else if(face_ == 4 || face_ == 5){ // Top face (+y) or bottom face (-y)
		if((x_ >= -width/2.0 && x_ <= width/2.0) && (z_ >= -depth/2.0 && z_ <= depth/2.0)){ return true; }
	}
	
	return false;	
}

/** Find if a ray (from the origin) intersects the infinite
  * plane defined by one of the faces of the VANDLE detector
  * face 0 is along the +z local axis
  * face 1 is along the +x local axis
  * face 2 is along the -z local axis
  * face 3 is along the -x local axis
  * face 4 is along the +y local axis
  * face 5 is along the -y local axis.
  */
bool Primitive::PlaneIntersect(const Vector3 &offset_, const Vector3 &direction_, unsigned int face_, Vector3 &P){
	if(need_set){ _set_face_coords(); }
	
	Vector3 unit;
	GetUnitVector(face_, unit);
	
	// The ray vector has the parametric form ray = offset_ + t*direction_
	// First find the intersection points between the ray and a plane containing the face polygon
	//double denom = direction_.Dot(unit);
	//if(denom < 1E-8){ return false; }
	
	double t = (GlobalFace[face_].Dot(unit)-offset_.Dot(unit))/(direction_.Dot(unit));
	P = offset_ + direction_*t; // The intersection point on the plane
	
	if(t >= 0){ return true; } // The ray intersects the plane
	return false; // The plane is behind the ray, the ray will never intersect it
}

/** Calculate the intersection of a ray of the form (offset_ + t * direction_) with this 
  * primitive shape offset_ is the point where the ray originates wrt the global origin.
  * direction_ is the direction of the ray wrt the global origin.
  * P1 is the first intersection point in global coordinates.
  * P2 is the second intersection point in global coordinates.
  * norm is the normal vector to the surface at point P1.
  * Return true if the primitive is intersected, and false otherwise.
  */
bool Primitive::IntersectPrimitive(const Vector3 &offset_, const Vector3 &direction_, Vector3 &P1, Vector3 &P2, Vector3 &norm,
								int &face1, int &face2, double &px, double &py, double &pz){
	if(need_set){ _set_face_coords(); }
	
	double px2, py2, pz2;
	int face_count = 0;
	Vector3 ray, unit;
	for(unsigned int i = 0; i < 6; i++){
		GetUnitVector(i, unit);
		if(PlaneIntersect(offset_, direction_, i, ray)){ // The ray intersects the plane containing this face
			// Transform the intersection point into local coordinates and check if they're within the bounds
			GetLocalCoords(ray, px2, py2, pz2);
			if(CheckBounds(i, px2, py2, pz2)){ // The face was struck
				if(face_count == 0){ 
					px = px2; py = py2; pz = pz2;
					P1 = ray; 
					face1 = i; 
				}
				else if(face_count == 1){ 
					P2 = ray; 
					face2 = i; 
					
					if(P2.Length() < P1.Length()){ px = px2; py = py2; pz = pz2; } // Ensure we get the hit on the plane facing the target
					//break;
				}
				face_count++;
			}
		} // if(PlaneIntersect(offset_, direction_, i, ray))
	} // for(unsigned int i = 0; i < 6; i++)
	
	return (face_count > 0); 
}

/// Alternate form of IntersectPrimitive which does not return the surface normal.
bool Primitive::IntersectPrimitive(const Vector3& offset_, const Vector3& direction_, Vector3 &P1, Vector3 &P2, int &face1, int &face2, double &px, double &py, double &pz){
	Vector3 dummy;
	return IntersectPrimitive(offset_, direction_, P1, P2, dummy, face1, face2, px, py, pz);
}
	
/** Trace a ray through the detector and calculate the thickness it sees between two faces (f1 and f2)
  * Return -1 if the ray does not travel through both faces.
  */
double Primitive::GetApparentThickness(const Vector3 &offset_, const Vector3 &direction_, unsigned int f1_, unsigned int f2_, Vector3 &intersect1, Vector3 &intersect2){
	if(f1_ > 5 || f2_ > 5){ return -1; } // Invalid face number
	if(need_set){ _set_face_coords(); }
	double px, py, pz;
	
	// Check face 1
	if(PlaneIntersect(offset_, direction_, f1_, intersect1)){ 
		GetLocalCoords(intersect1, px, py, pz);
		if(!CheckBounds(f1_, px, py, pz)){ return -1; }
	}
	else{ return -1; }
	
	// Check face 2
	if(PlaneIntersect(offset_, direction_, f2_, intersect2)){
		GetLocalCoords(intersect2, px, py, pz);
		if(!CheckBounds(f2_, px, py, pz)){ return -1; }
	}
	else{ return -1; }
	return Dist3d(intersect1, intersect2);
}

/** Dump raw cartesian face vertex data.
  * This returns a string containing the vertex coordinates of the
  * centers of all six faces of the VANDLE detector and its center coordinate.
  */
std::string Primitive::DumpVertex(){
	if(need_set){ _set_face_coords(); }
	std::stringstream stream;
	for(unsigned int i = 0; i < 6; i++){
		stream << GlobalFace[i].axis[0] << "\t" << GlobalFace[i].axis[1] << "\t" << GlobalFace[i].axis[2] << "\n";
	}
	stream << position.axis[0] << "\t" << position.axis[1] << "\t" << position.axis[2];
	return stream.str();
}

/** Dump VIKAR detector format string
  * X(m) Y(m) Z(m) Theta(rad) Phi(rad) Psi(rad) Type Subtype Length(m) Width(m) Depth(m) Material.
  */
std::string Primitive::DumpDet(){
	if(need_set){ _set_face_coords(); }
	std::stringstream stream;
	stream << position.axis[0] << "\t" << position.axis[1] << "\t" << position.axis[2];
	stream << "\t" << theta << "\t" << phi << "\t" << psi;
	stream << "\t" << type << "\t" << subtype;
	stream << "\t" << length << "\t" << width << "\t" << depth;
	stream << "\t" << material_name;
	return stream.str();
}


