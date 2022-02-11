#include "all.h"
#include "volume.h"
#include "scene.h"


//----------------------------

#define NUM_REPORT_CONTACS 4  //default number of contacts we report

#ifdef _DEBUG

//#define DEBUG_NO_REDUNDANT_FILTERING   //do not filter-out redundant contacts
#define DEBUG_BSP_VOL_AS_TRIANGLES  //test volumes in BSP by triangles, rather than as primitives

#endif

//----------------------------

static S_vector MulVectoryBy3x3(const S_vector &v, const S_vector *mat){

   return S_vector(
      v.x*mat[0].x + v.y*mat[1].x + v.z*mat[2].x,
      v.x*mat[0].y + v.y*mat[1].y + v.z*mat[2].y,
      v.x*mat[0].z + v.y*mat[1].z + v.z*mat[2].z);
}

/*
//----------------------------

static float DotVectorByColumn(const S_vector &v, const S_vector *mat, dword index){
   return
      v.x * mat[0][index] +
      v.y * mat[1][index] +
      v.z * mat[2][index];
}

//----------------------------

static float DotColumnByColumn(const S_vector *a, dword ia, const S_vector *b, dword ib){
   return
      a[0][ia] * b[0][ib] +
      a[1][ia] * b[1][ib] +
      a[2][ia] * b[2][ib];
}

//----------------------------

void Transpose(S_vector *m){
   swap(m[0][1], m[1][0]);
   swap(m[0][2], m[2][0]);
   swap(m[1][2], m[2][1]);
}
*/

//----------------------------

static bool ContactSphereSphere(const I3D_bsphere &bs1, const I3D_bsphere &bs2,
   CPI3D_volume vol1, const I3D_contact_data &cd){

                              //check if spheres potentionally collide
   float radius_sum = bs1.radius + bs2.radius;
   float r_2 = radius_sum * radius_sum;
   S_vector vol_dir = bs2.pos - bs1.pos;
   float d_2 = vol_dir.Square();

   if(r_2 <= d_2)
      return false;
                              //query collision
   if(cd.cb_query){
      if(!cd.cb_query(vol1, cd.context))
         return false;
   }
   float d = I3DSqrt(d_2);
   S_vector normal;

   if(IsMrgZeroLess(d)){
                              //special case - centers equal,
                              // normal undeterminable, just make some
      normal.x = 0;
      normal.y = 1;
      normal.z = 0;
   }else{
      normal = vol_dir / d;
   }
   float depth = radius_sum - d;
                              //put pos into middle of union, but not beyond this sphere's center
   S_vector pos = bs2.pos - normal * Max(0.0f, bs2.radius - depth * .5f);
   cd.cb_report(vol1, pos, normal, radius_sum - d, cd.context);
   return true;
}

//----------------------------
                              //SPHERE : SPHERE
void I3D_volume::ContactSphereSphere(const I3D_contact_data &cd) const{

   assert(volume_type==I3DVOLUME_SPHERE && cd.vol_src->volume_type==I3DVOLUME_SPHERE);
   vol_flags |= VOLF_DEBUG_TRIGGER;
   ::ContactSphereSphere(w_sphere, cd.vol_src->w_sphere, this, cd);
}

//----------------------------
                              //CYLINDER : SPHERE
void I3D_volume::ContactCylinderSphere(const I3D_contact_data &cd, CPI3D_volume vol, bool reverse) const{

   assert(volume_type==I3DVOLUME_CAPCYL && vol->volume_type==I3DVOLUME_SPHERE);
   vol_flags |= VOLF_DEBUG_TRIGGER;
   const I3D_bsphere src_s = vol->w_sphere;

                              //find the point on the cylinder axis that is closest to the sphere
   float pol = src_s.pos.PositionOnLine(w_sphere.pos, normal[2]);
   if(pol < -world_half_length)
      pol = -world_half_length;
   else
   if(pol > world_half_length)
      pol = world_half_length;
   S_vector cpos = w_sphere.pos + normal[2] * pol;
   //cd.cb_report(!reverse ? this : vol, cpos, S_vector(0, 0, 0), 0, cd.context); return; //debug
                              //get direction between spheres
   float radius_sum = world_radius + src_s.radius;
   float r_2 = radius_sum * radius_sum;
   S_vector vol_dir = src_s.pos - cpos;
   float d_2 = vol_dir.Square();
   if(r_2 <= d_2)
      return;
                              //query collision
   if(cd.cb_query){
      if(!cd.cb_query(!reverse ? this : vol, cd.context))
         return;
   }
   float d = I3DSqrt(d_2);
   S_vector normal;

   if(IsMrgZeroLess(d)){
                              //special case - centers equal,
                              // normal undeterminable, just make some
      normal.x = 0;
      normal.y = 1;
      normal.z = 0;
   }else{
      normal = vol_dir / d;
   }
   float depth = radius_sum - d;
                              //put pos into middle of union, but not beyond this sphere's center
   S_vector pos = src_s.pos - normal * Max(0.0f, src_s.radius - depth * .5f);
   cd.cb_report(!reverse ? this : vol, pos, !reverse ? normal : -normal, radius_sum - d, cd.context);
}

//----------------------------

void I3D_volume::ContactCylinderCylinder(const I3D_contact_data &cd) const{

   CPI3D_volume vol = cd.vol_src;
   assert(volume_type==I3DVOLUME_CAPCYL && vol->volume_type==I3DVOLUME_CAPCYL);
   vol_flags |= VOLF_DEBUG_TRIGGER;

   //int i;
   //const dReal tolerance = REAL(1e-5);
   const S_vector &pos1 = w_sphere.pos;
   const S_vector &pos2 = vol->w_sphere.pos;
   const S_vector &axis1 = normal[2];
   S_vector axis2 = vol->normal[2];
   I3D_bsphere bs1, bs2;
   bs1.radius = world_radius;
   bs2.radius = vol->world_radius;

   //dReal alpha1,alpha2,sphere1[3],sphere2[3];
   float alpha1, alpha2;
   int fix1 = 0;              //0 if alpha1 is free, +/-1 to fix at +/- world_half_length
   int fix2 = 0;              // same for 2nd volume

   for(int count=0; count<9; count++){
                              //find a trial solution by fixing or not fixing the alphas
      if(fix1){
         if(fix2){
                              //alpha1 and alpha2 are fixed, so the solution is easy
	         if(fix1 > 0)
               alpha1 = world_half_length;
            else
               alpha1 = -world_half_length;
	         if(fix2 > 0)
               alpha2 = vol->world_half_length;
            else
               alpha2 = -vol->world_half_length;

	         //for(i=0; i<3; i++) bs1.pos[i] = pos1[i] + alpha1*axis1[i];
            bs1.pos = pos1 + axis1 * alpha1;
	         //for (i=0; i<3; i++) bs2.pos[i] = pos2[i] + alpha2*axis2[i];
         }else{
	                             //fix alpha1 but let alpha2 be free
	         if(fix1 > 0)
               alpha1 = world_half_length;
            else
               alpha1 = -world_half_length;

	         //for (i=0; i<3; i++) bs1.pos[i] = pos1[i] + alpha1*axis1[i];
            bs1.pos = pos1 + axis1 * alpha1;
	         //alpha2 = (axis2[0]*(bs1.pos[0]-pos2[0]) + axis2[1]*(bs1.pos[1]-pos2[1]) + axis2[2]*(bs1.pos[2]-pos2[2]));
            alpha2 = (axis2 * (bs1.pos - pos2)).Sum();
         }
         bs2.pos = pos2 + axis2 * alpha2;
      }else{
         if(fix2){
                              //fix alpha2 but let alpha1 be free
	         if(fix2 > 0)
               alpha2 = vol->world_half_length;
            else
               alpha2 = -vol->world_half_length;
	         //for (i=0; i<3; i++) bs2.pos[i] = pos2[i] + alpha2*axis2[i];
            bs2.pos = pos2 + axis2 * alpha2;

         	//alpha1 = (axis1[0]*(bs2.pos[0]-pos1[0]) + axis1[1]*(bs2.pos[1]-pos1[1]) + axis1[2]*(bs2.pos[2]-pos1[2]));
            alpha1 = (axis1 * (bs2.pos - pos1)).Sum();
	         //for (i=0; i<3; i++) bs1.pos[i] = pos1[i] + alpha1*axis1[i];
            bs1.pos = pos1 + axis1 * alpha1;
         }else{
                              //let alpha1 and alpha2 be free
                              // compute determinant of d(d^2)\d(alpha) jacobian
	         float a1a2 = axis1.Dot(axis2);
	         float det = 1.0f - a1a2*a1a2;
	         if(det < 1e-5f){
                              //the cylinder axes (almost) parallel, so we will generate up to two contacts
                              //the solution matrix is rank deficient so alpha1 and alpha2 are related by:
	                           //    alpha2 =   alpha1 + (pos1-pos2)'*axis1   (if axis1==axis2)
	                           //    or alpha2 = -(alpha1 + (pos1-pos2)'*axis1)  (if axis1==-axis2)
	                           //first compute where the two cylinders overlap in alpha1 space:
	            if(a1a2 < 0){
	               //axis2[0] = -axis2[0]; axis2[1] = -axis2[1]; axis2[2] = -axis2[2];
                  axis2.Invert();
	            }
	            //for (i=0; i<3; i++) q[i] = pos1[i]-pos2[i];
	            S_vector q = pos1 - pos2;
	            float k = axis1.Dot(q);
	            float a1lo = -world_half_length;
	            float a1hi = world_half_length;
	            float a2lo = -vol->world_half_length - k;
	            float a2hi = vol->world_half_length - k;
	            float lo = (a1lo > a2lo) ? a1lo : a2lo;
	            float hi = (a1hi < a2hi) ? a1hi : a2hi;
	            if(lo <= hi){
	               int num_contacts = 3;//flags & NUMC_MASK;
	               if(num_contacts >= 2 && lo < hi){
                           	  //generate up to two contacts
                                 // if one of those contacts is not made, fall back on the one-contact strategy
	                  //for (i=0; i<3; i++) bs1.pos[i] = pos1[i] + lo*axis1[i];
                     bs1.pos = pos1 + axis1*lo;
	                  //for (i=0; i<3; i++) bs2.pos[i] = pos2[i] + (lo+k)*axis2[i];
                     bs2.pos = pos2 + axis2 * (lo+k);
	                  if(::ContactSphereSphere(bs1, bs2, this, cd)){
		                  //for (i=0; i<3; i++) bs1.pos[i] = pos1[i] + hi*axis1[i];
                        bs1.pos = pos1 + axis1 * hi;
		                  //for (i=0; i<3; i++) bs2.pos[i] = pos2[i] + (hi+k)*axis2[i];
                        bs2.pos = pos2 + axis2 * (hi+k);
		                  if(::ContactSphereSphere(bs1, bs2, this, cd))
		                     return;
	                  }
	               }
	                             //just one contact to generate, so put it in the middle of the range
	               alpha1 = (lo + hi) * .5f;
	               alpha2 = alpha1 + k;
	               //for (i=0; i<3; i++) bs1.pos[i] = pos1[i] + alpha1*axis1[i];
	               //for (i=0; i<3; i++) bs2.pos[i] = pos2[i] + alpha2*axis2[i];
               }else{
                              //collide by their capped sphere-halfs, which are closer to neighbour's geom position
                  alpha1 = world_half_length;
                  if(q.Dot(axis1)>0.0f)
                     alpha1 = -alpha1;
                  alpha2 = vol->world_half_length;
                  if(q.Dot(axis2)<0.0f)
                     alpha2 = -alpha2;
               }
               bs1.pos = pos1 + axis1*alpha1;
               bs2.pos = pos2 + axis2*alpha2;
               ::ContactSphereSphere(bs1, bs2, this, cd);
               return;
	         }
	         det = 1.0f / det;
	         //for (i=0; i<3; i++) delta[i] = pos1[i] - pos2[i];
      	   S_vector delta = pos1 - pos2;
	         float q1 = delta.Dot(axis1);
	         float q2 = delta.Dot(axis2);
	         alpha1 = det*(a1a2*q2-q1);
	         alpha2 = det*(q2-a1a2*q1);
	         //for (i=0; i<3; i++) bs1.pos[i] = pos1[i] + alpha1*axis1[i];
            bs1.pos = pos1 + axis1*alpha1;
	         //for (i=0; i<3; i++) bs2.pos[i] = pos2[i] + alpha2*axis2[i];
            bs2.pos = pos2 + axis2*alpha2;
         }
      }
                              //if the alphas are outside their allowed ranges then fix them and try again
      if(fix1==0){
         if(alpha1 < -world_half_length){
	         fix1 = -1;
	         continue;
         }
         if(alpha1 > world_half_length){
         	fix1 = 1;
         	continue;
         }
      }
      if(fix2==0){
         if(alpha2 < -vol->world_half_length){
	         fix2 = -1;
	         continue;
         }
         if(alpha2 > vol->world_half_length){
	         fix2 = 1;
	         continue;
         }
      }

                              //unfix the alpha variables if the local distance gradient indicates
                              // that we are not yet at the minimum
    //for (i=0; i<3; i++) tmp[i] = bs1.pos[i] - bs2.pos[i];
      S_vector tmp = bs1.pos - bs2.pos;
      if(fix1){
         float gradient = tmp.Dot(axis1);
         if((fix1 > 0 && gradient > 0.0f) || (fix1 < 0 && gradient < 0.0f)){
	         fix1 = 0;
	         continue;
         }
      }
      if(fix2){
         float gradient = -tmp.Dot(axis2);
         if((fix2 > 0 && gradient > 0.0f) || (fix2 < 0 && gradient < 0.0f)){
	         fix2 = 0;
	         continue;
         }
      }
      ::ContactSphereSphere(bs1, bs2, this, cd);
      return;
   }
                              //if we go through the loop too much, then give up. we should NEVER get to this point
}

//----------------------------

bool I3D_volume::IsSphereStaticCollision(const S_vector &pos, float radius) const{

   //vol_flags |= VOLF_DEBUG_TRIGGER;

   switch(volume_type){
   case I3DVOLUME_CAPCYL:
      {
                              //get point cliped onto cyl's main line
         float pol = pos.PositionOnLine(w_sphere.pos, normal[2]);
         if(pol < -world_half_length)
            pol = world_half_length;
         else
         if(pol > world_half_length)
            pol = world_half_length;
         S_vector p = w_sphere.pos + normal[2] * pol;
         //drv->DebugPoint(pos, .1f, 1); drv->DebugPoint(p, .1f, 1); drv->DebugLine(p, pos, 1);
                              //check distance
         float r_sum = radius + world_radius;
         return ((p - pos).Square() < r_sum*r_sum);
      }
      break;

   case I3DVOLUME_BOX:
   case I3DVOLUME_RECTANGLE:
      {
                              //get point clipped onto box's boundary
         S_vector p = pos;
         for(dword i=3; i--; ){
            const S_vector &n = normal[i];
            float d = -n.Dot(p);
            if(d < d_max[i]){
               p += n * (d - d_max[i]);
            }else
            if(d > -d_min[i]){
               p += n * (d + d_min[i]);
            }
         }
         //drv->DebugPoint(pos, .1f, 1); drv->DebugPoint(p, .1f, 1); drv->DebugLine(p, pos, 1);
                                    //check distance to the clipped point
         return ((p - pos).Square() < radius*radius);
      }
      break;
   default: assert(0);
   }
   return false;
   /*
#define TEST_SIDE(n) { \
      float q = normal[n].Dot(p); \
      float side = world_side_size[n] * .5f; \
      if(q < -side-radius || q > side+radius) return false; \
   }

   TEST_SIDE(0);
   TEST_SIDE(1);
   TEST_SIDE(2);
#undef TEST_SIDE
   return true;
   */
}

//----------------------------
                              //BOX : SPHERE (RECT : SPHERE)
void I3D_volume::ContactBoxSphere(const I3D_contact_data &cd, CPI3D_volume vol, bool reverse) const{

   assert((volume_type==I3DVOLUME_BOX || volume_type==I3DVOLUME_RECTANGLE) && vol->volume_type==I3DVOLUME_SPHERE);
   vol_flags |= VOLF_DEBUG_TRIGGER;
   const I3D_bsphere sphere = vol->w_sphere;
                              //get sphere's pos relative to box's pos
   S_vector p = sphere.pos - matrix(3);

   bool inside = true;
                              //get point clipped onto box's boundary
   S_vector q;
   float side[3];
#define MAKE_Q(n) \
   q[n] = normal[n].Dot(p); \
   side[n] = world_side_size[n] * .5f; \
   if(q[n] < -side[n]-sphere.radius || q[n] >  side[n]+sphere.radius) return; \
   if(q[n] < -side[n]){ q[n] = -side[n]; inside = false; } \
   if(q[n] >  side[n]){ q[n] =  side[n]; inside = false; }

   MAKE_Q(0);
   MAKE_Q(1);
   MAKE_Q(2);
#undef MAKE_Q

                              //query collision
   if(cd.cb_query){
      if(!cd.cb_query(!reverse ? this : vol, cd.context))
         return;
   }

   if(inside){
	                             //sphere center is inside box - find largest q's value
	   float max = I3DFabs(q[0]);
	   int maxi = 0;
	   for(int i=1; i<3; i++){
		   float tt = I3DFabs(q[i]);
		   if(tt > max) {
	         max = tt;
	         maxi = i;
		   }
      }
	                             //contact position = sphere center
	                             //contact normal aligned with box edge along largest 'q' value
      S_vector n = normal[maxi];
      bool inv = q[maxi] < 0.0f;
      if(reverse)
         inv = !inv;
      if(inv)
         n.Invert();
	                             //contact depth = distance to wall along normal plus radius
	   float depth = side[maxi] - max + sphere.radius;
      S_vector pos = sphere.pos + n * -Max(0.0f, Min(sphere.radius, max-sphere.radius) * .5f);
      //const S_vector &pos = cc.pos; //contact point at sphere position
      cd.cb_report(!reverse ? this : vol, pos, n, depth, cd.context);
      return;
   }
                              //put q back to world dir
                              // rotate by unit-length 3x3 matrix, because it's already scaled
                              // (the unit-len 3x3 matrix is made of 3 side normals)
                              // add frame's world position from matrix(3)
   S_vector qw = MulVectoryBy3x3(q, normal) + matrix(3);

   //cd.cb_report(this, qw, S_vector(0, 0, 0), 1, cd.context); return;  //debug - report world q
                              //make normal - pointing from world q to the sphere position
   S_vector n = sphere.pos - qw;
   float nd = n.Magnitude();
   if(IsMrgZeroLess(nd))
      return;
   float depth = sphere.radius - nd;
   if(depth <= 0.0f)
      return;
   n /= nd;
   S_vector pos = qw + n * (-depth * .5f);
   if(reverse)
      n.Invert();
   //const S_vector &pos = qw;
   cd.cb_report(!reverse ? this : vol, pos, n, depth, cd.context);
}

//----------------------------
// Find all the intersection points between the 2D rectangle with vertices
// at (+/-h[0],+/-h[1]) and the 2D quadrilateral with vertices (p[0],p[1]),
// (p[2],p[3]),(p[4],p[5]),(p[6],p[7]).
//
// The intersection points are returned as x,y pairs in the 'ret' array.
// The number of intersection points is returned by the function (this will
// be in the range 0 to 8).
static int IntersectRectQuad(float h[2], float p[8], float ret[16]){
                              //q (and r) contain nq (and nr) coordinate points for the current (and chopped) polygons
   int nq = 4, nr = 0;
   float buffer[16];
   float *q = p;
   float *r = ret;
   for(int dir=0; dir <= 1; dir++){
                              //direction notation: xy[0] = x axis, xy[1] = y axis
      for(int sign=-1; sign <= 1; sign += 2){
                              //chop q along the line xy[dir] = sign*h[dir]
         float *pq = q;
         float *pr = r;
         nr = 0;
         for(int i=nq; i > 0; i--){
                              //go through all points in q and all lines between adjacent points
	         if(sign*pq[dir] < h[dir]){
	                             //this point is inside the chopping line
	            pr[0] = pq[0];
	            pr[1] = pq[1];
	            pr += 2;
	            ++nr;
	            if(nr & 8){
	               q = r;
	               goto done;
	            }
	         }
	         float *nextq = (i > 1) ? pq+2 : q;
	         if((sign*pq[dir] < h[dir]) ^ (sign*nextq[dir] < h[dir])){
                              //this line crosses the chopping line
	            pr[1-dir] = pq[1-dir] + (nextq[1-dir]-pq[1-dir]) /
	               (nextq[dir]-pq[dir]) * (sign*h[dir]-pq[dir]);
	            pr[dir] = sign*h[dir];
	            pr += 2;
	            nr++;
	            if(nr & 8){
	               q = r;
	               goto done;
	            }
	         }
	         pq += 2;
         }
         q = r;
         r = (q==ret) ? buffer : ret;
         nq = nr;
      }
   }
done:
   if(q != ret)
      memcpy(ret, q, nr*2*sizeof(float));
   return nr;
}

//----------------------------
// Given n points in the plane (array p, of size 2*n), generate m points that best represent the whole set.
// The definition of 'best' here is not predetermined - the idea is to select points that give good box-box
// collision detection behavior. The chosen point indexes are returned in the array iret (of size m).
// 'i0' is always the first entry in the array. n must be in the range [1..8]. m must be in the range [1..n].
//  i0 must be in the range [0..n-1].
static void CullPoints(dword nun_points, float p[], dword max_points, int index_0, dword iret[]){

                              //compute the centroid of the polygon in cx,cy
   float a, cx, cy;

   switch(nun_points){
   case 1:
      cx = p[0];
      cy = p[1];
      break;
   case 2:
      cx = .5f * (p[0] + p[2]);
      cy = .5f * (p[1] + p[3]);
      break;
   default:
      a = 0.0f;
      cx = 0.0f;
      cy = 0.0f;
      for(dword i=0; i<(nun_points-1); i++){
         float q = p[i*2]*p[i*2+3] - p[i*2+2]*p[i*2+1];
         a += q;
         cx += q*(p[i*2]+p[i*2+2]);
         cy += q*(p[i*2+1]+p[i*2+3]);
      }
      float q = p[nun_points*2-2]*p[1] - p[0]*p[nun_points*2-1];
      a = 1.0f / (3.0f * (a+q));
      cx = a*(cx + q*(p[nun_points*2-2]+p[0]));
      cy = a*(cy + q*(p[nun_points*2-1]+p[1]));
   }

                              //compute the angle of each point w.r.t. the centroid
   float angle[8];
   for(dword i=0; i<nun_points; i++)
      angle[i] = (float)atan2(p[i*2+1]-cy, p[i*2]-cx);

                              //search for points that have angles closest to angle[i0] + i*(2*pi/m).
   int avail[8];
   for(i=0; i<nun_points; i++)
      avail[i] = 1;

   avail[index_0] = 0;
   iret[0] = index_0;
   ++iret;
   for(dword j=1; j<max_points; j++){
      a = float(j) * (2.0f*PI/max_points) + angle[index_0];
      if(a > PI)
         a -= 2.0f*PI;
      float maxdiff = 1e9f;
      for(i=0; i<nun_points; i++){
         if(avail[i]){
	         float diff = I3DFabs(angle[i]-a);
	         if(diff > PI)
               diff = 2.0f*PI - diff;
	         if(maxdiff > diff){
	            maxdiff = diff;
	            *iret = i;
	         }
         }
      }
      avail[*iret] = 0;
      ++iret;
   }
}

//----------------------------

void I3D_volume::ContactBoxBox(const I3D_contact_data &cd) const{

   assert((volume_type==I3DVOLUME_BOX || volume_type==I3DVOLUME_RECTANGLE) && cd.vol_src->volume_type==I3DVOLUME_BOX);
   vol_flags |= VOLF_DEBUG_TRIGGER;

   dword maxc = NUM_REPORT_CONTACS;
                              //query collision
   if(cd.cb_query){
      maxc = cd.cb_query(this, cd.context);
      if(!maxc)
         return;
   }

   const S_vector &p1 = cd.vol_src->matrix(3);
   const S_vector &p2 = matrix(3);
   const S_vector *_R1 = cd.vol_src->normal;
   const S_vector *_R2 = normal;
   const S_vector &side1 = *(S_vector*)cd.vol_src->world_side_size;
   const S_vector &side2 = *(S_vector*)world_side_size;

   const float FUDGE_FACTOR = 1.05f;

                              //get vector from centers of box 1 to box 2, relative to box 1
   S_vector p = p2 - p1;
                              //get pp = p relative to body
   S_vector pp(p.Dot(_R1[0]), p.Dot(_R1[1]), p.Dot(_R1[2]));
   
                              //get side lengths / 2
   S_vector A = side1 * .5f, B = side2 * .5f;

                              //Rij is R1'*R2, i.e. the relative rotation between R1 and R2
   float R11 = _R1[0].Dot(_R2[0]), R12 = _R1[0].Dot(_R2[1]), R13 = _R1[0].Dot(_R2[2]);
   float R21 = _R1[1].Dot(_R2[0]), R22 = _R1[1].Dot(_R2[1]), R23 = _R1[1].Dot(_R2[2]);
   float R31 = _R1[2].Dot(_R2[0]), R32 = _R1[2].Dot(_R2[1]), R33 = _R1[2].Dot(_R2[2]);

   float Q11 = I3DFabs(R11), Q12 = I3DFabs(R12), Q13 = I3DFabs(R13);
   float Q21 = I3DFabs(R21), Q22 = I3DFabs(R22), Q23 = I3DFabs(R23);
   float Q31 = I3DFabs(R31), Q32 = I3DFabs(R32), Q33 = I3DFabs(R33);

                              //for all 15 possible separating axes:
                              // * see if the axis separates the boxes. if so, return 0.
                              // * find the depth of the penetration along the separating axis (s2)
                              // * if this is the largest depth so far, record it.
                              //the normal vector will be set to the separating axis with the smallest
                              //depth. note: normalR is set to point to a column of R1 or R2 if that is
                              //the smallest depth normal so far. otherwise normalR is 0 and normalC is
                              //set to a vector relative to body 1. invert_normal is true if the sign of
                              //the normal should be flipped.
#define TST(expr1, expr2, norm, cc) { \
   float s2 = I3DFabs(expr1) - (expr2); \
   if(s2 > 0.0f) return; \
   if(depth < s2){ \
      depth = s2; \
      normalR = norm; \
      invert_normal = ((expr1) >= 0); \
      code = (cc); \
   } }

   float depth = -1e+16f;
   bool invert_normal = false;

                              //code: 0 = no collision
                              // 1-3 = box1's separating axis index (1=X, 2=Y, 3=Z)
                              // 4-6 = box2's separating axis index (4=X, 5=Y, 6=Z)
                              // 7-15 = edge:edge collision (single contact computed)
   int code = 0;
   const S_vector *normalR = NULL;
                              //separating axis = u1, u2, u3
   TST(pp[0], (A[0] + B[0]*Q11 + B[1]*Q12 + B[2]*Q13), &_R1[0], 1);
   TST(pp[1], (A[1] + B[0]*Q21 + B[1]*Q22 + B[2]*Q23), &_R1[1], 2);
   TST(pp[2], (A[2] + B[0]*Q31 + B[1]*Q32 + B[2]*Q33), &_R1[2], 3);
                              //separating axis = v1, v2, v3
   TST(p.Dot(_R2[0]), (A[0]*Q11 + A[1]*Q21 + A[2]*Q31 + B[0]), &_R2[0], 4);
   TST(p.Dot(_R2[1]), (A[0]*Q12 + A[1]*Q22 + A[2]*Q32 + B[1]), &_R2[1], 5);
   TST(p.Dot(_R2[2]), (A[0]*Q13 + A[1]*Q23 + A[2]*Q33 + B[2]), &_R2[2], 6);

                              //note: cross product axes need to be scaled when s is computed.
                              // normal (n1,n2,n3) is relative to box 1
#undef TST
   S_vector normalC;

#define TST(expr1, expr2, n1, n2, n3, cc) { \
   float s2 = I3DFabs(expr1) - (expr2); \
   if(s2 > 0) return; \
   float l = I3DSqrt((n1)*(n1) + (n2)*(n2) + (n3)*(n3)); \
   if(l > 0){ \
      s2 /= l; \
      if(depth < s2*FUDGE_FACTOR){ \
         depth = s2; \
         normalR = NULL; \
         normalC.x = (n1)/l; normalC.y = (n2)/l; normalC.z = (n3)/l; \
         invert_normal = ((expr1) >= 0); \
         code = (cc); \
      } \
   } }

                              //separating axis = u1 x (v1,v2,v3)
   TST(pp[2]*R21 - pp[1]*R31, (A[1]*Q31+A[2]*Q21+B[1]*Q13+B[2]*Q12), 0, -R31, R21, 7);
   TST(pp[2]*R22 - pp[1]*R32, (A[1]*Q32+A[2]*Q22+B[0]*Q13+B[2]*Q11), 0, -R32, R22, 8);
   TST(pp[2]*R23 - pp[1]*R33, (A[1]*Q33+A[2]*Q23+B[0]*Q12+B[1]*Q11), 0, -R33, R23, 9);

                              //separating axis = u2 x (v1,v2,v3)
   TST(pp[0]*R31 - pp[2]*R11, (A[0]*Q31+A[2]*Q11+B[1]*Q23+B[2]*Q22), R31, 0, -R11, 10);
   TST(pp[0]*R32 - pp[2]*R12, (A[0]*Q32+A[2]*Q12+B[0]*Q23+B[2]*Q21), R32, 0, -R12, 11);
   TST(pp[0]*R33 - pp[2]*R13, (A[0]*Q33+A[2]*Q13+B[0]*Q22+B[1]*Q21), R33, 0, -R13, 12);

                              //separating axis = u3 x (v1,v2,v3)
   TST(pp[1]*R11 - pp[0]*R21, (A[0]*Q21+A[1]*Q11+B[1]*Q33+B[2]*Q32), -R21, R11, 0, 13);
   TST(pp[1]*R12 - pp[0]*R22, (A[0]*Q22+A[1]*Q12+B[0]*Q33+B[2]*Q31), -R22, R12, 0, 14);
   TST(pp[1]*R13 - pp[0]*R23, (A[0]*Q23+A[1]*Q13+B[0]*Q32+B[1]*Q31), -R23, R13, 0, 15);

#undef TST
   if(!code)
      return;

                              //if we get to this point, the boxes interpenetrate
                              // compute the normal in global coordinates.
   S_vector normal;
   if(normalR){
      normal = *normalR;
   }else {
      //dMULTIPLY0_331(normal,R1,normalC);
      normal = MulVectoryBy3x3(normalC, _R1);
   }
   if(invert_normal)
      normal.Invert();
   depth = -depth;

   //drv->DEBUG(code);
                              //compute contact point(s)
   if(code > 6){
                              //an edge from box 1 touches an edge from box 2
      int j;

                              //find a point pa on the intersecting edge of box 1
      S_vector pa = p1;
      for(j=0; j<3; j++){
         //sign = (dDOT14(normal,R1+j) > 0) ? REAL(1.0) : REAL(-1.0);
         float sign = (normal.Dot(_R1[j]) <= 0.0f) ? 1.0f : -1.0f;
         pa += _R1[j] * (A[j] * sign);
      }

                              //find a point pb on the intersecting edge of box 2
      S_vector pb = p2;
      for(j=0; j<3; j++){
         //sign = (dDOT14(normal,R2+j) > 0) ? REAL(-1.0) : REAL(1.0);
         float sign = (normal.Dot(_R2[j]) <= 0.0f) ? -1.0f : 1.0f;
         pb += _R2[j] * (B[j] * sign);
      }

      code -= 7;
      /*
      S_vector ua, ub;
      for(int i=0; i<3; i++){
         ua[i] = R1[i][code/3];
         ub[i] = R2[i][code%3];
      }
      */
      S_vector ua = _R1[code/3];
      S_vector ub = _R2[code%3];
      float alpha, beta;

      //drv->DebugLine(pa, pa+ua, 0); drv->DebugLine(pb, pb+ub, 0);

      if(!LineIntersection(pa, ua, pb, ub, alpha, beta)){
         //assert(0);
         return;
      }
                              //compute closest collision points
      pa += ua * alpha;
      pb += ub * beta;
      //cd.cb_report(this, pa, normal, depth, cd.context);
      //cd.cb_report(this, pb, normal, depth, cd.context);
                              //make the contact point a 'compromise' between those two
      S_vector pos = (pa + pb) * .5f;
      cd.cb_report(this, pos, normal, depth, cd.context);
      return;
   }

                              //okay, we have a face-something intersection 
                              // (because the separating axis is perpendicular to a face)
                              //define face 'a' to be the reference face
                              // (i.e. the normal vector is perpendicular to this)
                              // and face 'b' to be the incident face (the closest face of the other box)
   const S_vector *_Ra, *_Rb, *pa, *pb, *Sa, *Sb;
   S_vector normal2;

   if(code <= 3){
      _Ra = _R1;
      _Rb = _R2;
      pa = &p1;
      pb = &p2;
      Sa = &A;
      Sb = &B;

      normal2 = -normal;
   }else{
      _Ra = _R2;
      _Rb = _R1;
      pa = &p2;
      pb = &p1;
      Sa = &B;
      Sb = &A;

      normal2 = normal;
   }
                              //nr = normal vector of reference face dotted with axes of incident box
                              //anr = absolute values of nr
   //dMULTIPLY1_331 (nr,Rb,normal2);
   /*
   nr.x = Rb[0].Dot(normal2);
   nr.y = Rb[1].Dot(normal2);
   nr.z = Rb[2].Dot(normal2);
   */
   S_vector nr(normal2.Dot(_Rb[0]), normal2.Dot(_Rb[1]), normal2.Dot(_Rb[2]));
   S_vector anr(I3DFabs(nr.x), I3DFabs(nr.y), I3DFabs(nr.z));

                              //find the largest compontent of anr: this corresponds to the normal
                              // for the indident face
                              //the other axis numbers of the indicent face are stored in a1, a2
   int lanr, a1, a2;
   if(anr[1] > anr[0]){
      if(anr[1] > anr[2]){
         a1 = 0;
         lanr = 1;
         a2 = 2;
      }else{
         a1 = 0;
         a2 = 1;
         lanr = 2;
      }
   }else{
      if(anr[0] > anr[2]){
         lanr = 0;
         a1 = 1;
         a2 = 2;
      }else {
         a1 = 0;
         a2 = 1;
         lanr = 2;
      }
   }

                              //compute center point of incident face, in reference-face coordinates
   S_vector center = *pb - *pa;
   if(nr[lanr] < 0){
      //for(int i=0; i<3; i++) center[i] += (*Sb)[lanr] * Rb[i][lanr];
      center += _Rb[lanr] * (*Sb)[lanr];
   }else{
      //for(int i=0; i<3; i++) center[i] -= (*Sb)[lanr] * Rb[i][lanr];
      center -= _Rb[lanr] * (*Sb)[lanr];
   }

                              //find the normal and non-normal axis numbers of the reference box
   int codeN, code1, code2;
   if(code <= 3)
      codeN = code-1;
   else
      codeN = code-4;

   switch(codeN){
   case 0:
      code1 = 1;
      code2 = 2;
      break;
   case 1:
      code1 = 0;
      code2 = 2;
      break;
   default:
      code1 = 0;
      code2 = 1;
   }

                              //find the four corners of the incident face, in reference-face coordinates
   //c1 = dDOT14(center, Ra+code1);
   //c2 = dDOT14 (center,Ra+code2);
   float c1 = center.Dot(_Ra[code1]);
   float c2 = center.Dot(_Ra[code2]);
                              //optimize this? - we have already computed this data above, but it is not
                              // stored in an easy-to-index format
                              //for now it's quicker just to recompute the four dot products
   //m11 = dDOT44 (Ra+code1,Rb+a1);
   //m12 = dDOT44 (Ra+code1,Rb+a2);
   //m21 = dDOT44 (Ra+code2,Rb+a1);
   //m22 = dDOT44 (Ra+code2,Rb+a2);
   float m11 = _Ra[code1].Dot(_Rb[a1]);
   float m12 = _Ra[code1].Dot(_Rb[a2]);
   float m21 = _Ra[code2].Dot(_Rb[a1]);
   float m22 = _Ra[code2].Dot(_Rb[a2]);

   float quad[8];	            //2D coordinate of incident face (x,y pairs)
   {
      float k1 = m11 * (*Sb)[a1];
      float k2 = m21 * (*Sb)[a1];
      float k3 = m12 * (*Sb)[a2];
      float k4 = m22 * (*Sb)[a2];
      quad[0] = c1 - k1 - k3;
      quad[1] = c2 - k2 - k4;
      quad[2] = c1 - k1 + k3;
      quad[3] = c2 - k2 + k4;
      quad[4] = c1 + k1 + k3;
      quad[5] = c2 + k2 + k4;
      quad[6] = c1 + k1 - k3;
      quad[7] = c2 + k2 - k4;
   }

                              //find the size of the reference face
   float rect[2];
   rect[0] = (*Sa)[code1];
   rect[1] = (*Sa)[code2];

                              //intersect the incident and reference faces
   float ret[16];
   int n = IntersectRectQuad(rect, quad, ret);
   if(n < 1)
      return;                 //this should never happen

                              //convert the intersection points into reference-face coordinates,
                              // and compute the contact position and depth for each point. only keep
                              //those points that have a positive (penetrating) depth. delete points in
                              // the 'ret' array as necessary so that 'point' and 'ret' correspond.
   //float point[3*8];          //penetrating contact points
   S_vector point[8];          //penetrating contact points
   float dep[8];              //depths for those points
   float det1 = 1.0f / (m11*m22 - m12*m21);
   m11 *= det1;
   m12 *= det1;
   m21 *= det1;
   m22 *= det1;

   dword cnum = 0;            //number of penetrating contact points found
   for(int j=0; j < n; j++){
      float k1 =  m22*(ret[j*2]-c1) - m12*(ret[j*2+1]-c2);
      float k2 = -m21*(ret[j*2]-c1) + m11*(ret[j*2+1]-c2);
      /*
      for(int i=0; i<3; i++)
         point[cnum][i] = center[i] + k1*Rb[i][a1] + k2*Rb[i][a2];
         */
      point[cnum] = center + _Rb[a1]*k1 + _Rb[a2]*k2;
      dep[cnum] = (*Sa)[codeN] - normal2.Dot(point[cnum]);
      if(dep[cnum] >= 0){
         ret[cnum*2] = ret[j*2];
         ret[cnum*2+1] = ret[j*2+1];
         ++cnum;
      }
   }
   if(cnum < 1)
      return;                 //this should never happen

   if(maxc > cnum)
      maxc = cnum;

   if(cnum <= maxc){
                              //we have less contacts than we need, so we use them all
      for(dword i=0; i < cnum; i++){
         S_vector pos = point[i] + *pa;
         float depth = dep[i];
         cd.cb_report(this, pos, normal, depth, cd.context);
      }
   }else{
                              //we have more contacts than are wanted, some of them must be culled

                              //find the deepest point, it is always the first contact
      int i1 = 0;
      float maxdepth = dep[0];
      for(dword i=1; i<cnum; i++){
         if(dep[i] > maxdepth){
	         maxdepth = dep[i];
	         i1 = i;
         }
      }
      dword iret[8];
      CullPoints(cnum, ret, maxc, i1, iret);

      for(i=0; i < maxc; i++){
         dword ii = iret[i];
         S_vector pos = point[ii] + *pa;
         float depth = dep[ii];
         cd.cb_report(this, pos, normal, depth, cd.context);
      }
   }
}

//----------------------------

void I3D_volume::ContactBoxCylinder(const I3D_contact_data &cd, CPI3D_volume vol, bool reverse) const{

   assert((volume_type==I3DVOLUME_BOX || volume_type==I3DVOLUME_RECTANGLE) && vol->volume_type==I3DVOLUME_CAPCYL);
   vol_flags |= VOLF_DEBUG_TRIGGER;

                              //get p1,p2 = cylinder axis endpoints, get radius
   S_vector p1 = vol->w_sphere.pos + vol->normal[2] * vol->world_half_length;
   S_vector p2 = vol->w_sphere.pos - vol->normal[2] * vol->world_half_length;

                              //get the closest point between the cylinder axis and the box

// Given a line segment p1-p2 and a box (center 'c', rotation 'R', side length
// vector 'side'), compute the points of closest approach between the box
// and the line. Return these points in 'lret' (the point on the line) and
// 'bret' (the point on the box). If the line actually penetrates the box
// then the solution is not unique, but only one solution will be returned.
// In this case the solution points will coincide.

                              //compute the start and delta of the line p1-p2 relative to the box
                              //we will do all subsequent computations in this box-relative coordinate system
                              //we have to do a translation and rotation for each point
   S_vector tmp = p1 - w_sphere.pos;
   S_vector s(tmp.Dot(normal[0]), tmp.Dot(normal[1]), tmp.Dot(normal[2]));
   S_vector cyl_line = p2 - p1;
   S_vector v(cyl_line.Dot(normal[0]), cyl_line.Dot(normal[1]), cyl_line.Dot(normal[2]));

                              //mirror the line so that v has all components >= 0
   S_vector sign;
   for(int i=0; i<3; i++){
      if(v[i] < 0.0f){
         s[i] = -s[i];
         v[i] = -v[i];
         sign[i] = -1.0f;
      }else
         sign[i] = 1.0f;
   }

                              //compute v^2
   S_vector v2 = v*v;

                              //compute the half-sides of the box
   //h[0] = REAL(0.5) * side[0]; h[1] = REAL(0.5) * side[1]; h[2] = REAL(0.5) * side[2];
   S_vector h = (*(S_vector*)world_side_size) * .5f;

                              //region is -1,0,+1 depending on which side of the box planes each
                              // coordinate is on
                              //tanchor is the next t value at which there is a transition,
                              // or the last one if there are no more
   int region[3];
   float tanchor[3];

                              //find the region and tanchor values for p1
   for(i=0; i<3; i++){
      if(!IsMrgZeroLess(v[i])){
         if(s[i] < -h[i]){
      	   region[i] = -1;
	         tanchor[i] = (-h[i]-s[i]) / v[i];
         }else{
      	   region[i] = (s[i] > h[i]);
	         tanchor[i] = (h[i]-s[i]) / v[i];
         }
      }else{
         region[i] = 0;
         tanchor[i] = 2;		    //this will never be a valid tanchor
      }
   }

                              //compute d|d|^2/dt for t=0
                              // if it's >= 0 then p1 is the closest point
   float t = 0.0f;
   float dd2dt = 0.0f;
   for(i=0; i<3; i++)
      dd2dt -= (region[i] ? v2[i] : 0.0f) * tanchor[i];

   if(dd2dt < 0.0f){

      do{
                                 //find the point on the line that is at the next clip plane boundary
         float next_t = 1.0f;
         for(i=0; i<3; i++){
            if(tanchor[i] > t && tanchor[i] < 1 && tanchor[i] < next_t)
               next_t = tanchor[i];
         }

                                 //compute d|d|^2/dt for the next t
         float next_dd2dt = 0.0f;
         for(i=0; i<3; i++)
            next_dd2dt += (region[i] ? v2[i] : 0) * (next_t - tanchor[i]);

                                 //if the sign of d|d|^2/dt has changed, solution = the crossover point
         if(next_dd2dt >= 0){
            float m = (next_dd2dt-dd2dt) / (next_t - t);
            t -= dd2dt / m;
            goto got_answer;
         }

                                 //advance to the next anchor point / region
          for(i=0; i<3; i++){
            if(tanchor[i] == next_t){
   	         tanchor[i] = (h[i]-s[i]) / v[i];
      	      region[i]++;
            }
         }
         t = next_t;
         dd2dt = next_dd2dt;
      }while(t < 1.0f);
      t = 1.0f;
got_answer:{}
   }

   I3D_bsphere bs_c, bs_b;
                              //compute closest point on the line
   bs_c.pos = p1 + cyl_line*t;
   //cd.cb_report(this, bs_c.pos, S_vector(0, 0, 0), 0, cd.context); return;

                              //compute closest point on the box
   for(i=0; i<3; i++){
      tmp[i] = sign[i] * (s[i] + t*v[i]);
      if(tmp[i] < -h[i])
         tmp[i] = -h[i];
      else
      if(tmp[i] > h[i])
         tmp[i] = h[i];
   }
   bs_b.pos = MulVectoryBy3x3(tmp, normal);
   bs_b.pos += w_sphere.pos;

   bs_c.radius = vol->world_radius;
   bs_b.radius = 0.0f;
   if(!reverse)
      ::ContactSphereSphere(bs_b, bs_c, this, cd);
   else
      ::ContactSphereSphere(bs_c, bs_b, vol, cd);
}

//----------------------------

struct S_bsp_contact{
   S_vector pos;
   S_vector normal;
   float depth;

   const S_bsp_triface *tf;
   int edge_index;
   int corner_index;
   float blend_count;

   S_bsp_contact():
      edge_index(-1),
      corner_index(-1),
      blend_count(1.0f)
   {}
   S_bsp_contact(const S_vector &p, const S_bsp_triface *tf1):
      edge_index(-1),
      corner_index(-1),
      blend_count(1.0f),
      pos(p), tf(tf1), normal(0, 0, 0), depth(0.0f)
   {}

   bool IsSimilar(const S_bsp_contact &c, float acos_thresh = .2f, float pos_thresh = .01f) const{
      float n_cos = normal.Dot(c.normal);
      if((1.0f - n_cos) < acos_thresh){
         if((pos-c.pos).Square() < pos_thresh*pos_thresh){
            if(I3DFabs(depth - c.depth) < pos_thresh){
               return true;
            }
         }
      }
      return false;
   }

   void Blend(const S_bsp_contact &c){
      float r_blend_count = 1.0f / (blend_count+1.0f);
      normal = normal*blend_count + c.normal;
      normal.Normalize();
      depth = (depth*blend_count + c.depth) * r_blend_count;
      pos = (pos*blend_count + c.pos) * r_blend_count;
      blend_count += 1.0f;
   }

   float GetScaledDepth() const{ return blend_count / (.2f + depth); }
};

//----------------------------
#define MAX_REPORT_CONTACTS 40

struct S_bsp_contact_context{
   const S_bsp_triface *marked_faces;
   S_bsp_contact contacts[MAX_REPORT_CONTACTS];
   dword num_contacts;

                              //linked list of all volumes which were marked in any test
                              // the list is cleared at destructor
                              //this is to avoid duplicated tests of volumes
   C_link_list<I3D_volume::S_tag_node> marked_volumes;

   PI3D_driver drv;

   S_bsp_contact_context():
      num_contacts(0),
      marked_faces(NULL)
   {}
   ~S_bsp_contact_context(){
      marked_volumes.ClearFull();
      for(const S_bsp_triface *f=marked_faces, *ff; f; ){
         ff = f;
         f = f->tag;
         ff->tag = NULL;
      }
   }
};
#define CONTACT_CONTEXT S_bsp_contact_context &cc = *static_cast<S_bsp_contact_context*>(context);

//----------------------------

void C_bsp_tree::GenContactsOnNode(const S_bsp_node &node, const I3D_contact_data &cd, float dist, void *context) const{

   CONTACT_CONTEXT;
#ifdef _DEBUG
#pragma warning(disable: 4189)
   PI3D_driver drv = cc.drv;
#endif

   CPI3D_volume vol = cd.vol_src;
   const I3D_bsphere &bs = vol->GetBoundSphere();

   const dword *face_list = node.GetFaces();
   const S_bsp_triface *pfaces = &faces.front();
                              //check all faces
   for(dword i = node.NumFaces(); i--; ){
      dword face_index = face_list[i];
      const S_bsp_triface &tf = pfaces[face_index];

                              //check if we tested it before (by using linked list)
      if(tf.tag)
         continue;
      tf.tag = cc.marked_faces;
      cc.marked_faces = &tf;

      CPI3D_frame col_frm = frames.GetFrameInfo(tf.origin.GetFrameIndex()).frm;
      if(!col_frm->IsOn1())
         continue;
                              //if the frame is not on, skip further testing

      I3D_VOLUMETYPE vt = vol->GetVolumeType1();

#ifndef DEBUG_BSP_VOL_AS_TRIANGLES
      if(col_frm->GetType1()==FRAME_VOLUME){
                              //special handling - test triangles of volume by volume:volume test
         CPI3D_volume col_vol = I3DCAST_CVOLUME(col_frm);
                              //check if the volume wasn't already processed
         if(!col_vol->tag_node.IsInList()){
            cc.marked_volumes.Add(&col_vol->tag_node);
            col_vol->Prepare();

                              //fast bounding-sphere test first
            if(SphereCollide(col_vol->w_sphere, vol->w_sphere)){

               switch(col_vol->GetVolumeType1()){
               case I3DVOLUME_BOX:
               case I3DVOLUME_RECTANGLE:
                  switch(vt){
                  case I3DVOLUME_SPHERE:
                     col_vol->ContactBoxSphere(cd, vol, false);
                     break;
                  case I3DVOLUME_CAPCYL:
                     col_vol->ContactBoxCylinder(cd, vol, false);
                     break;
                  case I3DVOLUME_BOX:
                  case I3DVOLUME_RECTANGLE:
                     col_vol->ContactBoxBox(cd);
                     break;
                  default: assert(0);
                  }
                  break;
               default: assert(0);
               }
            }
         }
         continue;
      }
#endif
                              //recompute distance if real plane is different than node's one
      const S_plane &face_plane = planes[tf.plane_id];
      if(tf.plane_id != node.plane_id)
         dist = bs.pos.DistanceToPlane(face_plane);

      switch(vt){
      case I3DVOLUME_SPHERE:
      case I3DVOLUME_BOX:
         {
                                    //ignore triangle, if our center is not facing it
            if(dist < 0.0f)
               break;
                              //make closest position to plane
            S_vector pos = bs.pos + face_plane.normal * -dist;
            float box_edge_dist[3];
            bool is_in = true;
            assert(cc.num_contacts < MAX_REPORT_CONTACTS);
            if(cc.num_contacts == MAX_REPORT_CONTACTS) continue;
            S_bsp_contact &c = cc.contacts[cc.num_contacts];
                              //check all 3 edges, get point closer
            int i;
            for(i=3; i--; ){
               float d = bs.pos.DistanceToPlane(tf.edge_planes[i]);
               if(d >= bs.radius)
                  break;
               box_edge_dist[i] = d;
               if(d > 0.0f){
                  if(c.edge_index==-1){
                              //compute point pos to be on the edge
                     const S_vector &p0 = vertices[tf.vertices_id[i]];
                     const S_vector &p1 = vertices[tf.vertices_id[next_tri_indx[i]]];
                     S_vector dir = p1 - p0;
                     float pol = pos.PositionOnLine(p0, dir);
                     if(pol<=0.0f){
                        c.corner_index = i;
                        pos = p0;
                     }else
                     if(pol>=1.0f){
                        c.corner_index = next_tri_indx[i];
                        pos = p1;
                     }else{
                        c.edge_index = i;
                        pos = p0 + dir * pol;
                     }
                  }else{
                              //out of 2 edges, the closest point must be at vertex position
                     dword vi = (next_tri_indx[i]==(dword)c.edge_index) ? c.edge_index : i;
                     pos = vertices[tf.vertices_id[vi]];
                     c.edge_index = -1;
                     //assert(c.corner_index!=-1 || c.corner_index==i);
                     c.corner_index = i;
                  }
                  is_in = false;
               }
            }
            if(i!=-1)
               break;

                              //query collision
            if(cd.cb_query){
               CPI3D_frame col_frm = frames.GetFrameInfo(tf.origin.GetFrameIndex()).frm;
               if(!cd.cb_query(col_frm, cd.context))
                  break;
            }

            switch(vt){
            case I3DVOLUME_SPHERE:
                              //only report if center is behind the plane
               {
                  float p_dist = dist;
                  if(!is_in){
                                    //revise distance to the contact point
                     c.normal = bs.pos - pos;
                     p_dist = c.normal.Square();
                                    //quit if it's more than radius
                     if(p_dist >= bs.radius*bs.radius)
                        continue;
                     p_dist = I3DSqrt(p_dist);
                     if(!IsMrgZeroLess(p_dist)){
                        c.normal /= p_dist;
                                    //make normal not to go inside of the face
                        if(c.normal.Dot(face_plane.normal) < 0.0f){
                                    //make normal perpendicular to plane normal, going in the current direction
                           S_vector cross = c.normal.Cross(face_plane.normal);
                           c.normal = face_plane.normal.Cross(cross);
                           c.normal.Normalize();
                        }
                     }else
                        c.normal = face_plane.normal;
                     c.pos = pos;
                  }else{
                     c.normal = face_plane.normal;
                     c.pos = bs.pos + c.normal * -bs.radius;
                  }
                  c.depth = bs.radius - p_dist;
                  c.tf = &tf;
                  ++cc.num_contacts;
               }
               break;

            case I3DVOLUME_BOX:
               {
                  //if(face_index!=64) break;   //debug!
                              //box - more detailed test
                  c.edge_index = -1;
                  c.corner_index = -1;

                  const S_vector &bb_min = vol->bbox.min;

                                    //info about 8 corner points of box
                  struct S_point{
                     float depth;
                     bool sink;
                     S_vector pos;
                     float tri_edge_depth[3];
                     dword tri_edge_clip; //set lowest 3 bits if vertex is out of particular tri edge
                  } points[8];

                  bool any_sink = false;
                  dword clip_union = 0x7; //for detection if all verts are out of single tri's edge
                  dword clip_intersection = 0;  //''                     inside of all tri's edges

                  for(i=8; i--; ){
                     S_point &p = points[i];
                     p.pos = bb_min;
                     if(i&1) p.pos += vol->normal[0] * vol->world_side_size[0];
                     if(i&2) p.pos += vol->normal[1] * vol->world_side_size[1];
                     if(i&4) p.pos += vol->normal[2] * vol->world_side_size[2];
                     float d = p.pos.DistanceToPlane(face_plane);
                     //cc.contacts.push_back(S_bsp_contact(p.pos, &tf)); DEBUG(d);
                     p.depth = -d;
                     dword sign_code = I3DFloatAsInt(d)&0x80000000;
                     p.sink = (sign_code);
                     if(p.sink)
                        any_sink = true;

                              //generate tri-edge depths
                     p.tri_edge_clip = 0;
                     for(dword ei=3; ei--; ){
                        float d = p.pos.DistanceToPlane(tf.edge_planes[ei]);
                        p.tri_edge_depth[ei] = d;
                        dword sign_bit = dword(I3DFloatAsInt(d)) >> 31;
                        p.tri_edge_clip |= (sign_bit^1)<<ei;
                     }
                              //compute point's clip flags
                     clip_intersection |= p.tri_edge_clip;
                     clip_union &= p.tri_edge_clip;
                  }
                              //check if any vertex is sink below the tri's plane
                  if(!any_sink)
                     break;

                              //if any 'outside' bit is not cleared, it means all verts are off one tri's edge's side
                  if(clip_union)
                     continue;

                              //if no 'outside' bits are set, it means all verts are inside of triangle
                  if(!clip_intersection){
                              //simplest case - all verts are inside tri's edges
                              // (quite common case if box is small and BSP triangle is large)
                              //simply report all sink contacts
                     for(i=8; i--; ){
                        S_point &p = points[i];
                        if(p.sink){
                           assert(cc.num_contacts < MAX_REPORT_CONTACTS);
                           if(cc.num_contacts == MAX_REPORT_CONTACTS) continue;
                           S_bsp_contact &c = cc.contacts[cc.num_contacts++];

                           c.pos = p.pos;
                           c.depth = p.depth;
                           c.normal = face_plane.normal;
                           c.tf = &tf;
                        }
                     }
                     break;
                  }

                  /*
                              //check if box's center is above the triangle
                  bool bb_above_tri = true;
                  for(i=3; i--; ){
                     float d = bs.pos.DistanceToPlane(tf.edge_planes[i]);
                     if(d > 0.0f){
                        bb_above_tri = false;
                        break;
                     }
                  }
                  */

                              //we may have maximally 6 contacts where tri's edges collide with box
                  S_bsp_contact edge_col[6];
                  S_vector edge_col_sum(0, 0, 0);
                  dword num_edge_cols = 0;
                  S_vector tri_center(0, 0, 0);

                              //generate points of collision of tri's edges with box
                  for(int ei=3; ei--; ){
                     const S_vector &p0 = vertices[tf.vertices_id[ei]];
                     tri_center += p0;
                              //if no box's vertex out of this edge, it can't hit box
                     if(!(clip_intersection&(1<<ei)))
                        continue;

                     const S_vector &p1 = vertices[tf.vertices_id[next_tri_indx[ei]]];
                     S_vector dir_e = p1 - p0;

                              //get intersection with box
                     float last_enter = -1e+16f;
                     float first_leave = 1e+16f;

                     for(i=3; i--; ){
                        const S_normal &n = vol->normal[i];
                                 //get intersection with the box's side normal
                        float f = n.Dot(dir_e);
                        float f_abs = I3DFabs(f);
                        if(IsMrgZeroLess(f_abs)){
                              //edge parallel to box's side, it must be inside in order to be valid
                           float d = -p0.Dot(vol->normal[i]);
                           if(d <= vol->d_max[i] || -d <= vol->d_min[i])
                              break;
                           continue;
                        }
                        float r_f = 1.0f / f;
                        float d_max = -( n.Dot(p0)+vol->d_max[i]) * r_f;
                        float d_min = -(n.Dot(p0)-vol->d_min[i]) * r_f;
                        if(f < 0.0f)
                           swap(d_max, d_min);
                        if(last_enter < d_min){
                           last_enter = d_min;
                        }
                        if(first_leave > d_max){
                           first_leave = d_max;
                        }
                     }
                                 //if edge is parallel to some side, and not inside, skip
                     if(i!=-1)
                        continue;
                                 //check if edge hit the box
                     if(last_enter > first_leave)
                        continue;

                     //drv->DebugLine(p0, p1, 1, 0xff00ff00);

                              //process 2 possible intersections
                     for(int j=2; j--; ){
                        float p;
                        bool is_corner = false;
                        if(!j){
                           p = last_enter;
                           if(p<0.0f){
                                    //clip to corner
                              p = 0.0f;
                              is_corner = true;
                           }
                           if(p>1.0f)
                              continue;
                        }else{
                           p = first_leave;
                           if(p>=1.0f){
                                    //skip, because neighbour edge processes this corner
                              continue;
                           }
                           if(p<0.0f)
                              continue;
                        }
                        assert(p>=0.0f && p<=1.0f);
                                    //store the point
                        S_bsp_contact &ec = edge_col[num_edge_cols++];
                        ec.edge_index = is_corner ? -1 : ei;
                        ec.corner_index = is_corner ? ei : -1;
                        ec.pos = p0 + dir_e * p;
                        edge_col_sum += ec.pos;
                        //drv->DebugPoint(ec.pos, .02f, 1, 0xff00ffff);
                     }
                  }
                  assert(num_edge_cols <= 6);

                  S_vector normal(0, 0, 0);
                  int normal_index = -1;  //index of best box's normal used (if -1, we're using tri's plane normal)
                  if(num_edge_cols){
                              //get median point (use projected box center, if it's above triangle
                     if(!is_in)
                        edge_col_sum /= (float)num_edge_cols;
                     else
                        edge_col_sum = bs.pos + face_plane.normal*-dist;
                     //drv->DebugPoint(edge_col_sum, .04f, 1, 0xc0ff0000);

                              //for that point, determine best normal to use
                     float min_depth = 1e+16f;
                     for(int j=3; j--; ){
                        const S_normal &n = vol->normal[j];
                              //check which side of normal to use
                        float f = edge_col_sum.Dot(n) + (vol->d_max[j] - vol->d_min[j])*.5f;
                        f = f<0 ? 1.0f : -1.0f;
                              //get depth in this side
                        float d;
                        if(f > 0.0f){
                           d = n.Dot(edge_col_sum) - vol->d_min[j] ;
                        }else{
                           d = -(n.Dot(edge_col_sum) + vol->d_max[j]);
                        }
                        //assert(d >= -1e-2f);
                                 //use if it's less than what we already have
                        if(min_depth > d){
                           min_depth = d;
                           normal = n * f;
                           normal_index = j;
                        }
                     }
                              //don't make point normal dowards triangle center
                     tri_center /= 3.0f;
                     S_vector c_dir = tri_center - edge_col_sum;
                     //drv->DebugLine(tri_center, edge_col_sum, 1, 0x4000ffff);
                     if(c_dir.Dot(normal) > 0.0f){
                        normal = face_plane.normal;
                     }else
                     if(normal.Dot(face_plane.normal) < 0.0f){
                              //don't make the normal point opposite the tri's normal
                        S_vector c = normal.Cross(face_plane.normal);
                        normal = face_plane.normal.Cross(c);
                     }
                     //drv->DebugLine(edge_col_sum, edge_col_sum+normal, 1, 0xc0ff0000);
                  }else{
                              //no edge collision, use plane normal
                     normal = face_plane.normal;
                  }

                              //now, having normal, do actual reporting
                  const S_normal &n = is_in ? face_plane.normal : normal;
                  //*
                              //tri's cols first
                  for(i=num_edge_cols; i--; ){
                     S_bsp_contact &c = edge_col[i];
                     c.tf = &tf;
                              //compute depth in 'normal_index' side of box
                     const S_normal &vol_n = vol->normal[normal_index];
                              //check which side of normal to use
                     c.depth = I3DFabs(c.pos.Dot(vol_n) + (vol->d_max[normal_index] - vol->d_min[normal_index])*.5f);
                     c.depth = vol->world_side_size[normal_index]*.5f - c.depth;
                     //drv->DEBUG(c.depth);
                              //report only non-zero contacts
                     if(c.depth > 1e-5f){
                        assert(cc.num_contacts < MAX_REPORT_CONTACTS);
                        if(cc.num_contacts<MAX_REPORT_CONTACTS){
                           c.normal = n;
                           c.tf = &tf;
                           cc.contacts[cc.num_contacts++] = c;
                        }
                     }
                  }
                  /**/

                              //report all sink box's points, which project onto the triangle
                  bool sink_test_pass = false;
                  for(i=8; i--; ){
                     const S_point &p = points[i];
                     if(!p.sink)
                        continue;
                     assert(cc.num_contacts < MAX_REPORT_CONTACTS);
                     if(cc.num_contacts == MAX_REPORT_CONTACTS) continue;
                     S_bsp_contact &c = cc.contacts[cc.num_contacts];

                     if(num_edge_cols){
                              //compute depth (skip if normal parallel to tri)
                        if(!face_plane.IntersectionPosition(p.pos, n, c.depth) || c.depth<1e-5f)
                           continue;
                        S_vector pproj = p.pos + normal*c.depth;
                              //check if projected point is inside the triangle
                        for(int j=3; j--; ){
                           const S_plane &tp = tf.edge_planes[j];
                           if(pproj.DistanceToPlane(tp) >= 0.0f)
                              break;
                        }
                        if(j!=-1)
                           continue;
                        //drv->DebugPoint(pproj, .02f, 1);
                     }else{
                              //no tri's edge collides with box, but point is sink
                              // - check if it's inside of triangle
                        if(p.tri_edge_clip || IsMrgZeroLess(p.depth))
                           continue;
                        if(!sink_test_pass){
                              //there're 2 possibilities how the box may collide - from top of tri's edge,
                              // or from bottom - detect this by ratio

                              //report only when point's depth is less than bbox's dist from tri
                           float r_dd = dist / (dist+p.depth);
                           for(int j=3; j--; ){
                              float r_de = box_edge_dist[j];
                              if(r_de <= 0.0f)
                                 continue;
                              r_de /= (r_de - p.tri_edge_depth[j]);
                              if(r_dd < r_de)
                                 break;
                              j = 0;
                           }
                           if(j!=-1){
                                       //failed - break out, since no point have chance to pass
                              break;
                           }
                           sink_test_pass = true;
                        }
                        c.depth = p.depth;
                     }
                     //drv->DebugLine(p.pos, p.pos+n, 1);
                     c.pos = p.pos;
                     c.normal = n;
                     c.tf = &tf;
                     ++cc.num_contacts;
                  }
               }
               break;
            default: assert(0);
            }
         }
         break;

      case I3DVOLUME_CAPCYL:
         {
                              //fast bounding-sphere culling
            int i;
            for(i=3; i--; ){
               float d = bs.pos.DistanceToPlane(tf.edge_planes[i]);
               if(d >= bs.radius)
                  break;
            }
            if(i!=-1)
               continue;

            S_bsp_contact cp[2];
                              //get 2 cylinder's end points
            S_vector cap[2];
            cap[0] = bs.pos;
            cap[1] = bs.pos;
            cap[0] -= vol->normal[2] * vol->world_half_length;
            cap[1] += vol->normal[2] * vol->world_half_length;
            //c.pos = cap[0]; cc.contacts.push_back(c);
            float cap_depth[2];
            bool is_in[2];
            dword out_side = 0x7;   //clip flags, cleared whenever point is not out of triangle's side
            bool sink = true;

            for(dword ci=2; ci--; ){
               //cc.contacts.push_back(S_bsp_contact(cap[ci], &tf));
                              //get cap's distance to plane
               float &depth = cap_depth[ci];
               depth = cap[ci].DistanceToPlane(face_plane);
               if(depth >= 0.0f)
                  sink = false;

                              //make closest positions to plane
               S_vector p_on_pl = cap[ci] + face_plane.normal * -depth;
               S_vector &cpp = cp[ci].pos;
               cpp = p_on_pl;

               is_in[ci] = true;
               dword out_flags = 0;
               cp[ci].normal = face_plane.normal;
                              //check all 3 edges, get point closer
               for(int i=3; i--; ){
                  float dte = p_on_pl.DistanceToPlane(tf.edge_planes[i]);
                  //if(dte >= bs.radius) break;
                  if(dte >= 0.0f){
                     if(dte >= vol->world_radius)
                        out_flags |= 1<<i;

                     if(depth < vol->world_radius){
                                 //compute point pos to be on the edge
                        const S_vector &p0 = vertices[tf.vertices_id[i]];
                        const S_vector &p1 = vertices[tf.vertices_id[next_tri_indx[i]]];
                        S_vector dir = p1 - p0;

                        float u, pol;
                        if(!LineIntersection(bs.pos, vol->normal[2], p0, dir, u, pol)){
                           depth = vol->world_radius+.01f;
                           continue;
                        }
                        if(I3DFabs(u) > vol->world_half_length){
                              //lines collide beyong cylinder's segment, compute position of cap on the tri's edge
                           pol = cap[ci].PositionOnLine(p0, dir);
                        }
                        if(pol<=0.0f){
                           cp[ci].corner_index = i;
                           cpp = p0;
                        }else
                        if(pol>=1.0f){
                           cp[ci].corner_index = next_tri_indx[i];
                           cpp = p1;
                        }else{
                           cp[ci].edge_index = i;
                           cpp = p0 + dir * pol;
                        }
                        //cc.contacts.push_back(S_bsp_contact(cpp, &tf));
                        is_in[ci] = false;
                     }
                  }
               }
               if(!is_in[ci]){
                              //re-compute depth
                  float pol = cpp.PositionOnLine(bs.pos, vol->normal[2]);
                  bool pol_clipped = false;
                  if(pol < -vol->world_half_length){
                     pol = -vol->world_half_length;
                     pol_clipped = true;
                  }else
                  if(pol > vol->world_half_length){
                     pol = vol->world_half_length;
                     pol_clipped = true;
                  }
                  S_vector px = bs.pos + vol->normal[2]*pol;
                  //cc.contacts.push_back(S_bsp_contact(cap[ci], &tf));
                  //cc.contacts.push_back(S_bsp_contact(px, &tf));
                  S_vector &n = cp[ci].normal;
                  n = px - cpp;
                              //if collided by line, do not make normal point inside
                  //if(!pol_clipped && px.DistanceToPlane(face_plane) <= 0.0f) n.Invert();
                  int ei = cp[ci].edge_index;
                  if(ei==-1)
                     ei = cp[ci].corner_index;
                  assert(ei!=-1);
                  if(!pol_clipped){
                     if(px.DistanceToPlane(face_plane) <= 0.0f && px.DistanceToPlane(tf.edge_planes[ei]) < 0.0f)
                        n.Invert();
                  }
                  depth = n.Magnitude();

                  if(IsMrgZeroLess(depth)){
                     n = face_plane.normal;
                  }else{
                                 //make normal not to go inside of the face
                     if(n.Dot(face_plane.normal) < 0.0f){
                                 //make normal perpendicular to plane normal, going in the current direction
                        S_vector cross = n.Cross(face_plane.normal);
                        if(!cross.IsNull()){
                           n = face_plane.normal.Cross(cross);
                           n.Normalize();
                        }else{
                           //n /= d;
                           n = face_plane.normal;
                        }
                     }else
                     /**/
                        n /= depth;
                  }
               }
               out_side &= out_flags;
            }
                              //if entirely out of single side, quit
            if(out_side || sink)
               break;

                              //query collision
            if(cd.cb_query){
               CPI3D_frame col_frm = frames.GetFrameInfo(tf.origin.GetFrameIndex()).frm;
               if(!cd.cb_query(col_frm, cd.context))
                  break;
            }

            for(ci=2; ci--; ){
               float depth = cap_depth[ci];
               if(depth < vol->world_radius && (depth>0.0f || cap_depth[1-ci]>0.0f)){
                  cp[ci].depth = vol->world_radius - depth;
                  cp[ci].tf = &tf;
                  assert(cc.num_contacts < MAX_REPORT_CONTACTS);
                  if(cc.num_contacts == MAX_REPORT_CONTACTS) break;
                  S_bsp_contact &c = cc.contacts[cc.num_contacts++];
                  c = cp[ci];
               }
            }
         }
         break;

      default: assert(0);
      }
   }
}

//----------------------------

void C_bsp_tree::GenContactsRecursive(const S_bsp_node &node, const I3D_contact_data &cd, void *context) const{

   const I3D_bsphere &bs = cd.vol_src->GetBoundSphere();

                              //signed distance of sphere origin to plane defined by Node
   E_PLANE_SIDE near_side, far_side;
   float dist = GetPointPlaneDistance(node.plane_id, bs.pos, near_side, far_side);
   assert(near_side != far_side);
                              //child of node for half-space containing the origin of sphere
   const S_bsp_node *tree_facing = node.GetChildBySide(near_side);

   if((dist < bs.radius) && (dist > -bs.radius)){
   //if((dist < bs.radius) && (dist > -cc.max_depth_test)){
                              //sphere intersect plane

                              //gen contacts of sphere with node's faces (on partion plane)
      GenContactsOnNode(node, cd, dist, context);

                              //the "other" child node -- i.e. not equal to facing
      const S_bsp_node *tree_opposite = node.GetChildBySide(far_side);
                              //the sphere touches the plane, process also opposite leaf
      if(tree_opposite)
         GenContactsRecursive(*tree_opposite, cd, context);
   }
                              //recursively process the facing leaf
   if(tree_facing)
      GenContactsRecursive(*tree_facing, cd, context);
}

//----------------------------

void C_bsp_tree::GenerateContacts(const I3D_contact_data &cd, PI3D_driver drv) const{

   if(!nodes.GetRoot())
      return;

                              //collect all contacts first
   S_bsp_contact_context cc;
   cc.drv = drv;
   //cc.face_cache.Reserve(faces.size());
   GenContactsRecursive(*nodes.GetRoot(), cd, &cc);

   int i;

#ifndef DEBUG_NO_REDUNDANT_FILTERING
   float scale = cd.vol_src->GetMatrix()(0).Magnitude();
   {
      const S_vector &nu_scl = cd.vol_src->GetNUScale();
      switch(cd.vol_src->GetVolumeType1()){
      case I3DVOLUME_BOX:
      case I3DVOLUME_RECTANGLE:
         scale *= nu_scl.Magnitude();
         break;
      case I3DVOLUME_CAPCYL:
         scale *= (nu_scl.x+nu_scl.z);
         break;
      case I3DVOLUME_CYLINDER:
         scale *= I3DSqrt(nu_scl.x*nu_scl.x+nu_scl.z*nu_scl.z);
         break;
      }
   }

                              //filter contacts
   //if(0)
   {
      for(i=cc.num_contacts; i--; ){
         S_bsp_contact &c = cc.contacts[i];
                                 //check edge & corner contact, try to optimize out
         if(c.edge_index!=-1){
                                 //on edge, try to find neighbour
            const S_bsp_triface &tf = *c.tf;
            dword vi0 = tf.vertices_id[c.edge_index];
            dword vi1 = tf.vertices_id[next_tri_indx[c.edge_index]];

            for(int i1=cc.num_contacts; i1--; ){
               if(i1==i) continue;
               S_bsp_contact &c1 = cc.contacts[i1];
               if(c1.tf == c.tf) continue;
               if(c1.edge_index!=-1 || c1.corner_index!=-1)
                  continue;
               const S_bsp_triface &tf1 = *c1.tf;
                                 //check if it's our neighbour on the edge
               if(vi0==tf1.vertices_id[0] || vi0==tf1.vertices_id[1] || vi0==tf1.vertices_id[2]){
                  if(vi1==tf1.vertices_id[0] || vi1==tf1.vertices_id[1] || vi1==tf1.vertices_id[2]){
                              //the edge is shared with this contact, forget ours
                     //c = cc.contacts.back(); cc.contacts.pop_back();
                     c = cc.contacts[--cc.num_contacts];
                     break;
                  }
               }
            }
         }else
         if(c.corner_index!=-1){
                                 //on corner, try to find neighbour
            const S_bsp_triface &tf = *c.tf;
            dword vi = tf.vertices_id[c.corner_index];

            for(int i1=cc.num_contacts; i1--; ){
               if(i1==i) continue;
               S_bsp_contact &c1 = cc.contacts[i1];
               if(c1.tf == c.tf) continue;
               if(c1.corner_index!=-1 || c1.edge_index!=-1) continue;
               const S_bsp_triface &tf1 = *c1.tf;
                                 //check if it's our neighbour on the edge
               if(vi==tf1.vertices_id[0] || vi==tf1.vertices_id[1] || vi==tf1.vertices_id[2]){
                              //the corner is shared with this contact, forget ours
                  //c = cc.contacts.back(); cc.contacts.pop_back();
                  c = cc.contacts[--cc.num_contacts];
                  break;
               }
            }
         }
      }
   }
                              //2nd pass - try to collapse similar contacts
   //if(0)
   {
      for(i=cc.num_contacts; i--; ){
         S_bsp_contact &c = cc.contacts[i];
         CPI3D_frame frm = frames.GetFrameInfo(c.tf->origin.GetFrameIndex()).frm;
         for(int j=cc.num_contacts; --j > i; ){
            S_bsp_contact &c1 = cc.contacts[j];
            CPI3D_frame frm1 = frames.GetFrameInfo(c1.tf->origin.GetFrameIndex()).frm;
            float acos_thresh = (frm==frm1) ? 1.0f : .4f;
            float pos_thresh = (frm==frm1) ? scale*.15f : scale*.05f;
            if(c.IsSimilar(c1, acos_thresh, pos_thresh)){
               c1.Blend(c);
               //c = cc.contacts.back(); cc.contacts.pop_back();
               c = cc.contacts[--cc.num_contacts];
            }
         }
      }
   }

#endif

                              //report them now
   for(i=cc.num_contacts; i--; ){
      const S_bsp_contact &c = cc.contacts[i];
      CPI3D_frame col_frm = frames.GetFrameInfo(c.tf->origin.GetFrameIndex()).frm;
      cd.cb_report(col_frm, c.pos, c.normal, c.depth, cd.context);
      c.tf->highlight = true;
   }
}

//----------------------------
