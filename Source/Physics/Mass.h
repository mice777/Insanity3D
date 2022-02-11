

class C_mass{
   float volume;              //total volume of the body
   float weight;              //total weight (mass) of rigid body
   S_matrix i_tensor;         //inertia tensor + center of gravity at fourth row

   bool Check();
public:
   void Zero();
   void Identity();

//----------------------------
   inline const float GetWeight() const{ return weight; }
   inline const S_vector &GetCenter() const{ return i_tensor(3); }
   inline const S_matrix &GetInertiaMatrix() const{ return i_tensor; }
   inline float GetVolume() const{ return volume; }

//----------------------------
// Set the mass parameters to represent a sphere of the given radius and density,
// with the center of mass at (0, 0, 0) relative to the body.
   void SetSphere(float density, float radius);

//----------------------------
// Set the mass parameters to represent a capped cylinder of the given parameters and density,
// with the center of mass at (0, 0, 0) relative to the body.
//    radius ... radius of the cylinder (and the spherical cap)
//    length ... length of the cylinder (not counting the spherical cap)
//    dir ... the cylinder's long axis is oriented along the body's x, y or z axis
//       according to this value (1=x, 2=y, 3=z)
   void SetCappedCylinder(float density, float radius, float length);

//----------------------------
// Set the mass parameters to represent a flat-ended cylinder of the given parameters and density,
// with the center of mass at (0, 0, 0) relative to the body.
// Parameters same as for SetCappedCylinder.
   void SetCylinder(float density, int dir, float radius, float length);

//----------------------------
// Set the mass parameters to represent a box of the given dimensions and density,
// with the center of mass at (0, 0, 0) relative to the body.
// The side lengths of the box along the x, y and z axes are in 'sides' vector.
   void SetBox(float density, const S_vector &sides);

//----------------------------
// Given mass parameters for some object, adjust them so the total mass is now newmass.
// This is useful when using the above functions to set the mass parameters for certain objects -
// they take the object density, not the total mass.
   void Adjust(float newmass);

//----------------------------
// Given mass parameters for some object, adjust them to represent the object displaced by (x, y, z)
// relative to the body frame.
   void Translate(const S_vector &v);

//----------------------------
// Given mass parameters for some object, adjust them to represent the object rotated by R relative to the body frame.
   void Rotate(const S_quat q);

//----------------------------
// Add the mass 'm' to this mass.
   void Add(const C_mass &m);
};

typedef C_mass *PC_mass;

//----------------------------