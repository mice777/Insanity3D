#include <IPhysics.h>
#include "mass.h"
#include "matrix.h"

//----------------------------

                              //fix non-uniform mass problems (mainly for small volumes)
#define FIX_MASS_DISTRIBUTION

//----------------------------
//----------------------------
/* check whether an n*n matrix A is positive definite, return 1/0 (yes/no).
 * positive definite means that x'*A*x > 0 for any x. this performs a
 * cholesky decomposition of A. if the decomposition fails then the matrix
 * is not positive definite. A is stored by rows. A is not altered.
 */
static bool dIsPositiveDefinite(const dReal *A, int n){

   dReal *Acopy;
   assert(n > 0 && A);
   int nskip = dPAD (n);
   Acopy = (dReal*) alloca (nskip*n * sizeof(dReal));
   memcpy (Acopy,A,nskip*n * sizeof(dReal));
   return dFactorCholesky(Acopy,n);
}

//----------------------------
//----------------------------

void C_mass::Zero(){
   memset(this, 0, sizeof(*this));
   i_tensor.m[3][3] = 1.0f;
}

//----------------------------

void C_mass::Identity(){
   weight = 1.0f;
   volume = 1.0f;
   i_tensor.Identity();
}

//----------------------------

bool C_mass::Check(){

   if(weight <= 0.0f){
      assert(("weight must be > 0", 0));
      return false;
   }
   if(!dIsPositiveDefinite(&i_tensor(0).x, 3)){
      assert(("inertia must be positive definite", 0));
      return false;
   }

                              //verify that the center of mass position is consistent with the mass
                              // and inertia matrix
                              //this is done by checking that the inertia around
                              // the center of mass is also positive definite
                              //from the comment in dMassTranslate(), if the body is translated so that its center 
                              // of mass is at the point of reference, then the new inertia is:
                              // I + mass*crossmat(c)^2
                              //note that requiring this to be positive definite is exactly equivalent
                              //to requiring that the spatial inertia matrix
                              // [ mass*eye(3,3)   M*crossmat(c)^T ]
                              // [ M*crossmat(c)   I               ]
                              // is positive definite, given that I is PD and mass>0. see the theorem
                              // about partitioned PD matrices for proof.
   S_matrix chat;
   chat.Zero();
   SetCrossMatrix(chat, i_tensor(3));
   S_matrix I2 = chat % chat;
   for(int i=0; i<3; i++)
      I2(i) = i_tensor(i) + I2(i)*weight;
   if(!dIsPositiveDefinite(&I2(0).x, 3)){
      assert(("center of mass inconsistent with mass parameters", 0));
      return false;
   }
   return true;
}

//----------------------------

void C_mass::SetSphere(float density, float radius){
 
   Identity();
                              //V = 4/3 * PI * r^3
   volume = 4.0f / 3.0f * PI * radius*radius*radius;
   weight = volume * density;
                              //Ix = Iy = Iz = 2/5 * m * r^2
   float ii = 0.4f * weight * radius*radius;
   i_tensor(0)[0] = ii;
   i_tensor(1)[1] = ii;
   i_tensor(2)[2] = ii;
#ifdef _DEBUG
   Check();
#endif
}

//----------------------------

void C_mass::SetCappedCylinder(float density, float radius, float length){

   Identity();

   float r2 = radius * radius;
                              //cylinder: V = PI * r^2 * l
   float m1 = PI * r2 * length;
                              //caps: V = 4/3 * PI * r^3
   float m2 = 4.0f / 3.0f * PI * r2 * radius;

   volume = m1 + m2;
   weight = volume * density;

   m1 *= density;
   m2 *= density;
                              //Ia = 1/4 * m1 * r^2 + 1/12 * m1 * l^2
                              // + 2/5 * m2 * r^2 + 1/2 * m2 * l^2
   float ia = m1 * (.25f * r2 + (1.0f / 12.0f) * length * length) +
      m2 * (.4f * r2 + .5f * length * length);
                              //Ib = 1/2 * m1 * r^2 + 2/5 * m2 * r^2
   float ib = (m1 * .5f + m2 * .4f) * r2;

#ifdef FIX_MASS_DISTRIBUTION
   const float FIX_MIN = 1e-4f;
   const float FIX_MAX = 1e-2f;
   if(volume < FIX_MAX){
                              //correct symetry of mass
      float fix_ratio = 1.0f;
      if(volume > FIX_MIN){
         fix_ratio = 1.0f - (volume - FIX_MIN) / (FIX_MAX - FIX_MIN);
      }
      fix_ratio *= .5f;
      float iaa = ia * (1.0f-fix_ratio) + ib * fix_ratio;
      float iba = ib * (1.0f-fix_ratio) + ia * fix_ratio;
      ia = iaa;
      ib = iba;
   }
#endif

   i_tensor(0)[0] = ia;
   i_tensor(1)[1] = ia;
   i_tensor(2)[2] = ib;

#ifdef _DEBUG
   Check();
#endif
}

//----------------------------

void C_mass::SetCylinder(float density, int dir, float radius, float length){

   Identity();

   float r2 = radius * radius;
                              //V = PI * r^2 * l
   volume = PI * r2 * length;
   weight = volume * density;
                              //Ia = 1/4 * m * r^2 + 1/12 * m * l^2
   float I = weight * (.25f * r2 + (1.0f / 12.0f) * length * length);
   i_tensor(0)[0] = I;
   i_tensor(1)[1] = I;
   i_tensor(2)[2] = I;
                              //Ib = 1/2 * m * r^2
   i_tensor(dir-1)[dir-1] = weight * .5f * r2;

#ifdef _DEBUG
   Check();
#endif
}

//----------------------------

void C_mass::SetBox(float density, const S_vector &in_sides){

   Identity();

   S_vector sides = in_sides;
                              //V = x * y * z
   volume = sides.x * sides.y * sides.z;
   weight = volume * density;

#ifdef FIX_MASS_DISTRIBUTION
   const float FIX_MIN = 1e-3f;
   const float FIX_MAX = 1e-1f;
   if(volume < FIX_MAX){
                              //correct symetry of mass
      float fix_ratio = 1.0f;
      if(volume > FIX_MIN){
         fix_ratio = 1.0f - (volume - FIX_MIN) / (FIX_MAX - FIX_MIN);
      }
      fix_ratio *= .5f;
      float avg_side = sides.Sum() / 3.0f;
      S_vector ss(avg_side, avg_side, avg_side);
      sides = sides*(1.0f-fix_ratio) + ss*fix_ratio;
   }
#endif
                              //Ix = 1/12 * m * (y^2 + z^2)
                              //Iy = 1/12 * m * (x^2 + z^2)
                              //Iz = 1/12 * m * (x^2 + y^2)
   i_tensor(0)[0] = weight / 12.0f * (sides.y*sides.y + sides.z*sides.z);
   i_tensor(1)[1] = weight / 12.0f * (sides.x*sides.x + sides.z*sides.z);
   i_tensor(2)[2] = weight / 12.0f * (sides.x*sides.x + sides.y*sides.y);
#ifdef _DEBUG
   Check();
#endif
}

//----------------------------

void C_mass::Adjust(float new_weight){

   float scale = new_weight / weight;
   weight = new_weight;
                              //rescale the inertia tensor matrix
   for(int j=0; j<3; j++)
      i_tensor(j) *= scale;
#ifdef _DEBUG
   Check();
#endif
}

//----------------------------

void C_mass::Translate(const S_vector &v){

                              //if the body is translated by `a' relative to its point of reference,
                              // the new inertia about the point of reference is:
                              //    I + mass*(crossmat(c)^2 - crossmat(c+a)^2)
                              // where c is the existing center of mass and I is the old inertia
   S_matrix ahat, chat;

   S_vector c = i_tensor(3);
                              //adjust inertia matrix
   chat.Zero();
   SetCrossMatrix(chat, c);
   S_vector a = v + i_tensor(3);
   ahat.Zero();
   SetCrossMatrix(ahat, a);
   S_matrix t1 = ahat % ahat;
   S_matrix t2 = chat % chat;
   for(int ii=0; ii<3; ii++)
      i_tensor(ii) += (t2(ii) - t1(ii)) * weight;

                              //ensure perfect symmetry
   i_tensor(1)[0] = i_tensor(0)[1];
   i_tensor(2)[0] = i_tensor(0)[2];
   i_tensor(2)[1] = i_tensor(1)[2];

                              //adjust center of mass
   i_tensor(3) += v;

#ifdef _DEBUG
   Check();
#endif
}

//----------------------------

void C_mass::Rotate(const S_quat q){
   
   S_matrix m_rot = q;
   S_matrix m_rot_inv = m_rot; m_rot_inv.Transpose();

                              //if the body is rotated by `R' relative to its point of reference,
                              //the new inertia about the point of reference is:
                              //    R * I * R'
                              // where I is the old inertia
   {
      /*
                              //rotate inertia matrix
      S_matrix Im;
      float *I = &Im(0).x;
      I[3] = 0;
      I[7] = 0;
      I[11] = 0;
      {
         dMatrix3 t1;
         dMULTIPLY2_333(t1, &i_tensor(0).x, R);
         dMULTIPLY0_333(I, R, t1);

      }
      i_tensor(0) = Im(0);
      i_tensor(1) = Im(1);
      i_tensor(2) = Im(2);
      */
      i_tensor = m_rot_inv % i_tensor % m_rot;
   }

                              //ensure perfect symmetry
   i_tensor(1)[0] = i_tensor(0)[1];
   i_tensor(2)[0] = i_tensor(0)[2];
   i_tensor(2)[1] = i_tensor(1)[2];

   assert(i_tensor(3).IsNull());
   /*
                              //rotate center of mass
   S_vector c = i_tensor(3);
   dMULTIPLY0_331(&i_tensor(3).x, R, &c.x);
   */

#ifdef _DEBUG
   Check();
#endif
}

//----------------------------

void C_mass::Add(const C_mass &m){

   volume += m.volume;
   float denom = 1.0f / (weight + m.weight);
   i_tensor(3) = (i_tensor(3)*weight + m.i_tensor(3)*m.weight) * denom;

   weight += m.weight;
   for(int j=0; j<3; j++)
      i_tensor(j) += m.i_tensor(j);
}

//----------------------------
