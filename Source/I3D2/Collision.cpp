
/*--------------------------------------------------------
   Copyright (c) 2001 LonelyCatGames.
   All rights reserved.
   Author: J.Vrubel

   File: Collision.cpp
   Content: Collision testing functions, combine bsp tree for static geometry and hiearchy 
            volumes for dynamic actors.
--------------------------------------------------------*/

#include "all.h"
#include "scene.h"
#include "volume.h"           
#include "bsp.h"
#include "visual.h"
                              //for edit frames
#include "occluder.h"
#include "dummy.h"
#include "joint.h"

                              //minimal count of volumes neaded for create hiearchy autobox
//#define DYNAMIC_VOLUME_BOUND_LIMIT 2

//#define SPHERE_VOLUME_DETECT_INSIDE    //detect line-sphere collision from inside of sphere
//#define BOX_VOLUME_DETECT_INSIDE    //detect line-box collision from inside of sphere

#ifdef _DEBUG
                              //global I3D_driver variable for debug output in response functions
//#define S_B_DEBUG_DYN(n) drv->DEBUG(n)
#define S_B_DEBUG_DYN(n) 

//#define S_R_DEBUG_DYN(n) drv->DEBUG(n)
#define S_R_DEBUG_DYN(n)

//#define S_S_DEBUG_DYN(n) drv->DEBUG(n)
#define S_S_DEBUG_DYN(n)

//#define S_CC_DEBUG(n) drv->DEBUG(n)
#define S_CC_DEBUG(n)

//#define L_CC_DEBUG(n) col_frm->GetDriver1()->DEBUG(n)
#define L_CC_DEBUG(n)

const dword R = 0xffff0000;
const dword G = 0xff00ff00;
const dword B = 0xff0000ff;

#else
#define S_B_DEBUG_DYN(n)
#define S_R_DEBUG_DYN(n)
#define S_S_DEBUG_DYN(n)
#define S_CC_DEBUG(n)
#define L_CC_DEBUG(n)

#endif //_DEBUG


//----------------------------
// I3D_scene collision functions
//----------------------------

void TestColLineCylinder(const S_vector &in_pos, const S_vector &z_axis,
   float radius, float half_length, PI3D_frame col_frm, I3D_collision_data &cd, bool capped){

                              //note: this code inspired by ODE library
#define REPORT_COLLISION \
   if(cd.callback){ \
      I3D_cresp_data rd(col_frm, n, alpha, cd.cresp_context); \
      rd.CopyInputValues(cd); \
      if(!cd.callback(rd)) \
         return; \
   } \
   cd.SetReturnValues(alpha, col_frm, n);

#ifdef _DEBUG
   PI3D_driver drv = col_frm->GetDriver1(); drv;
#endif

   const S_vector &norm_dir = cd.GetNormalizedDir();

   S_vector dir_to_sphere = in_pos - cd.from;
                              //position of ray start along ccyl axis
   float pos_on_line = -dir_to_sphere.Dot(z_axis);
   S_vector dir_to_pol = z_axis*pos_on_line + dir_to_sphere;
   //drv->DebugPoint(cd.from+dir_to_pol, .1f, 1); drv->DebugLine(cd.from, cd.from+dir_to_pol, 1);
   float cc = dir_to_pol.Square() - radius*radius;
   bool in_tube = CHECK_ZERO_LESS(cc);

                              //see if ray start position is inside the capped cylinder
   if(in_tube){
      if(capped){
         if(pos_on_line < -half_length)
            pos_on_line = -half_length;
         else
         if(pos_on_line > half_length)
            pos_on_line = half_length;
         S_vector cap = in_pos + z_axis * pos_on_line;
         if((cd.from-cap).Square() < radius*radius){
            //inside_ccyl = true;
                              //we're inside, quit
            return;
         }
      }else{
         if(!(pos_on_line<-half_length || pos_on_line>half_length))
            return;
      }
   //}

                              //compute ray collision with infinite cylinder, except for the case where
                              // the ray is outside the capped cylinder but within the infinite cylinder
                              // (it that case the ray can only hit endcaps)
   //if(in_tube){
                              //set 'pos_on_line' to cap position to check
      if(CHECK_ZERO_LESS(pos_on_line))
         pos_on_line = -half_length;
      else
         pos_on_line = half_length;
   }else{
      float dir_cos = z_axis.Dot(norm_dir);
      S_vector r = z_axis*dir_cos - norm_dir;
      //drv->DebugLine(cd.from, cd.from+r, 1, R);

      float a = r.Square();
      float b = 2.0f * dir_to_pol.Dot(r);
      float ff = b*b - 4.0f * a * cc;

      if(CHECK_ZERO_LESS(ff) || IsMrgZeroLess(a)){
                              //the ray does not intersect the infinite cylinder, but if the ray is
                              // inside and parallel to the cylinder axis it may intersect the end
                              // caps. set 'ff' to cap position to check
         /*
         if(!inside_ccyl)
            return;
         if(uv < 0.0f)
            k = -half_length;
         else
            k = half_length;
            */
         L_CC_DEBUG("ray doesn't hit cylinder - quit");
         return;
      }else{
         ff = I3DSqrt(ff);
         L_CC_DEBUG(C_xstr(
            "dir_cos = %\n"
            "a = %\n"
            "ff = %\n"
            )
            %dir_cos
            %a
            %ff
            );
         a = 1.0f / (2.0f*a);
         float alpha = (-b-ff) * a;
         if(CHECK_ZERO_LESS(alpha)){
	         alpha = (-b+ff) * a;
	         if(CHECK_ZERO_LESS(alpha))
               return;
         }
         if(alpha > cd.GetHitDistance())
            return;

                              //the ray intersects the infinite cylinder. check to see if the
                              // intersection point is between the caps

         S_vector pos = cd.from + norm_dir*alpha;
         S_vector q = pos - in_pos;
         ff = q.Dot(z_axis);
         if(ff >= -half_length && ff <= half_length){
            S_vector n = pos - (in_pos + z_axis*ff);
            REPORT_COLLISION;
	         return;
         }
                              //the infinite cylinder intersection point is not between the caps
                              // set 'pos_on_line' to cap position to check.
         if(CHECK_ZERO_LESS(ff))
            pos_on_line = -half_length;
         else
            pos_on_line = half_length;
      }
   }
                              //check for ray intersection with the caps
                              //k must indicate the cap position to check
   S_vector sphere_pos = in_pos + z_axis * pos_on_line;
   S_vector dir_to_sphere_pos = cd.from - sphere_pos;
   if(capped){

      float b = dir_to_sphere_pos.Dot(norm_dir);
      float c = dir_to_sphere_pos.Square() - radius*radius;
                              //note: if C <= 0 then the start of the ray is inside the sphere
      //assert(CHECK_ZERO_GREATER(c));
      float k = b*b - c;
      if(IsMrgZeroLess(k))
         return;
      k = I3DSqrt(k);
      float alpha = -b - k;
      if(CHECK_ZERO_LESS(alpha)){
         alpha = -b + k;
         if(CHECK_ZERO_LESS(alpha))
            return;
      }
      if(alpha > cd.GetHitDistance())
         return;

      S_vector pos = cd.from + norm_dir*alpha;
      S_vector n = pos - sphere_pos;
                              //confirm collision
      REPORT_COLLISION;
   }else{
                              //perform a ray plan interesection
      float k2 = norm_dir.Dot(z_axis);
                              //ray parallel to the plane
      if(IsAbsMrgZero(k2))
         return;
      float alpha = -dir_to_sphere_pos.Dot(z_axis) / k2;
      if(CHECK_ZERO_LESS(alpha) || alpha>=cd.GetHitDistance())
         return;
      S_vector pos = cd.from + norm_dir*alpha;
                              //check if position is withing circle
      if((sphere_pos-pos).Square() > radius*radius)
         return;
      S_vector n = (CHECK_ZERO_LESS(pos_on_line)) ? -z_axis : z_axis;
      REPORT_COLLISION;
   }
}

//----------------------------
//----------------------------

void I3D_volume::CheckCol_L_S_dyn(I3D_collision_data &cd){

                              //check if direct dist to sphere closest than what we have
   S_vector dir_to_sphere = w_sphere.pos - cd.from;
   {
      float dist_2 = dir_to_sphere.Square();
      float u = cd.GetHitDistance() + w_sphere.radius;
      u *= u;
      if(FLOAT_BITMASK(u) < FLOAT_BITMASK(dist_2))
                              //we can't reach the sphere anyway
         return;
   }
                              //the collision of line and sphere is solved by equation:
                              // d = -b +- sqrt(b^2 - 4 * a * c) / (2 * a)

                              //compute a (squared line's direction)
                              //optimization: since a is always 1.0f (we've got normalized
                              // direction), all we need is to scale top part by 0.5f 

                              //compute b (2 * line's direction dot direction to sphere center)
   const S_vector &norm_dir = cd.GetNormalizedDir();

   float b = norm_dir.Dot(dir_to_sphere);// * 2.0f;
                              //compute c (sphere center squared + line's P1 squared
                              // - 2 * line's p1 dot sphere center - squared radius
   float c = dir_to_sphere.Square() - (w_sphere.radius*w_sphere.radius);
#if defined _DEBUG && 0
   { float from_dot = cd.from.Square();
      float c1 = sphere_pos_dot + from_dot - 2.0f * (cd.from.Dot(w_sphere.pos)) - (w_sphere.radius*w_sphere.radius);
      assert(I3DFabs(c-c1) < 1e-3f); }
#endif


                              //solve the equation (a is ommited, since it is 1.0)
   float d = b * b - //4.0f *
      c;
                              //now we have semi-result, by which sign we may detect:
                              //when d is less than zero, the ray doesn't intersect sphere,
                              // when d is zero, the ray touches sphere (which we may dismiss too)
   if(CHECK_ZERO_LESS(d))
      return;

   d = I3DSqrt(d);
                              //solve both cases of the equation
                              // postpone division (i.e. multiply by 0.5) for later
   float u  = (b + d);
   float u1 = (b - d);
     
                              //get smaller positive value (intersection distance)
#ifdef SPHERE_VOLUME_DETECT_INSIDE
   if(CHECK_ZERO_LESS(u)){
      if(CHECK_ZERO_LESS(u1)){
                              //both intersection points behind us
         return false;
      }
      u = u1;
   }else
   if(CHECK_ZERO_GREATER(u1)){
      u = Min(u, u1);
   }
#else
                              //if any of them is less than zero, we can't hit the sphere
   if(CHECK_ZERO_LESS(u) || CHECK_ZERO_LESS(u1))
      return;
   (int&)u = Min((int&)u, (int&)u1);
#endif
   //u *= .5f;
                              //now u is distance of closest collision point

                              //check if closest hit is better than what we have
   float hit_d = cd.GetHitDistance();
   if(FLOAT_BITMASK(hit_d) < FLOAT_BITMASK(u))
      return;

   S_vector hit_normal = (cd.from + norm_dir * u) - w_sphere.pos;

                              //confirm collision
   if(cd.callback){
                              //set values for collision response
      I3D_cresp_data rd(this, hit_normal, u, cd.cresp_context);
      rd.CopyInputValues(cd);
      bool b = cd.callback(rd);
      if(!b)
         return;
   }
                              //collision detected
                              //save closest distance
   cd.SetReturnValues(u, this, hit_normal);

   vol_flags |= VOLF_DEBUG_TRIGGER;
}

//----------------------------

void I3D_volume::CheckCol_L_CC_dyn(I3D_collision_data &cd){

   TestColLineCylinder(w_sphere.pos, normal[2], world_radius, world_half_length, this, cd, (volume_type==I3DVOLUME_CAPCYL));
}

//----------------------------

bool I3D_volume::CheckCol_L_B_dyn(I3D_collision_data &cd, bool passive){

                              //consider we're in the box
   bool in = true;
                              //cached distances, keeping min/max extremes
   float hmin = 0.0f;
   float lmax = 1.0e+10f;

                              //keep indicies into best inside and outside normals
                              //so that we may return it as collision result
   int best_norm = -1;
#ifdef _DEBUG
   best_norm = -1;
#endif
                              //flip flags help us determine if normal need
                              //to be flipped
   bool flip_normal = true;

#ifdef BOX_VOLUME_DETECT_INSIDE
   int best_norm_in;
   bool flip_normal_in;
#endif

                              //check all 3 axes
   for(int j=0; j<3; j++){
                              //distances in which our ray collides with
                              //min/max sides of box
      float u_min, u_max;

      float dot_pos = normal[j].Dot(cd.from);

      u_max = -(dot_pos + d_max[j]);
      u_min = -(dot_pos - d_min[j]);

      const S_vector &norm_dir = cd.GetNormalizedDir();
      float f = normal[j].Dot(norm_dir);

//      L_B_DEBUG(C_fstr("u_min: %.2f, u_max: %.2f, f: %.2f", u_min, u_max, f));
                              //check if we aren't parallel with this side
      if(IsAbsMrgZero(f)){
                              //check if any of 'from' or 'to' is inside
                              //fast quit 1 - missing parallel side
         if(CHECK_ZERO_GREATER(u_min) || CHECK_ZERO_LESS(u_max)){
//            L_B_DEBUG("Fast quit 1 - missing parallel side");
            return false;
         }
         continue;
      }
      float r_dot_dir = 1.0f / f;
      u_max *= r_dot_dir;
      u_min *= r_dot_dir;

      bool swapped = false;
                              //determine which side (min/max) we've hit first
      if(u_max < u_min){
         swap(u_max, u_min);
         swapped = true;
      }

                              //both sides behind our back, exit
      if(CHECK_ZERO_LESS(u_max))
         return false;

      if(CHECK_ZERO_GREATER(u_min)){
                              //both sides in front of us, we can't be in
         if(hmin <= u_min){
                              //this is out farest (outside) hit position
                              //save this
            hmin = u_min;
            best_norm = j;
            flip_normal = swapped;
         }
         in = false;
      }
      if(lmax > u_max){
                              //this is out closest (inside) hit position
                              //save this
         lmax = u_max;
#ifdef BOX_VOLUME_DETECT_INSIDE
         best_norm_in = j;
         flip_normal_in = swapped;
#endif
      }
   }

   if(in){
                              //we're inside
                              // if passive test is made, it's succeeded,
                              // otherwise it's failed
#ifdef BOX_VOLUME_DETECT_INSIDE
                              //passive check ends here
      if(passive)
         return true;
                              //we're colliding with best_norm_in side
                              //in lmax distance
      hmin = lmax;
      best_norm = best_norm_in;
      flip_normal = flip_normal_in;
#else
      return (passive);
#endif
   }
#ifdef BOX_VOLUME_DETECT_INSIDE
   else
#endif
                              //test if our collision computations
                              //end up on the box
   if(hmin > lmax)
      return false;
                              //we've hit the box
                              //check if we have already better collision detected
   float hit_d = cd.GetHitDistance();
   if(FLOAT_BITMASK(hit_d) <= FLOAT_BITMASK(hmin))
      return false;

                              //collision detected
                              //passive check ends here
   if(passive)
      return true;

   assert(best_norm!=-1);

   S_vector hit_normal = normal[best_norm];
   if(!flip_normal)
      hit_normal.Invert();

   if(cd.callback){
                              //set values for collision response
      I3D_cresp_data rd(this, hit_normal, hmin, cd.cresp_context);
      rd.CopyInputValues(cd);
      bool b = cd.callback(rd);
      if(!b)
         return false;
  }

   cd.SetReturnValues(hmin, this, hit_normal);

   vol_flags |= VOLF_DEBUG_TRIGGER;
   return true;
}

//----------------------------

void I3D_volume::CheckCol_L_R_dyn(I3D_collision_data &cd){

                              //get ray/plane collision point
   const S_vector &norm_dir = cd.GetNormalizedDir();
   float u = normal[2].Dot(norm_dir);
   if(IsAbsMrgZero(u))
      return;

   float f = -normal[2].Dot(cd.from);
   float length = (f - d_max[2]) / u;
                              //don't look back
   if(CHECK_ZERO_LESS(length))
      return;
                              //reject more far collisions than
                              //what we already have
   float hit_d = cd.GetHitDistance();
   if(FLOAT_BITMASK(hit_d) <= FLOAT_BITMASK(length))
      return;
                              //get point on plane
   S_vector p = cd.from + norm_dir * length;

                              //check point's position against edges
                              //check X
   u = normal[0].Dot(p);
   if(u <= d_min[0] || u >= -d_max[0])
      return;
                              //check Y
   u = normal[1].Dot(p);
   if(u <= d_min[1] || u >= -d_max[1])
      return;

                              //collision detected
   S_vector hit_normal = normal[2];
   if(f > d_max[2])
      hit_normal.Invert();

   if(cd.callback){
                              //set values for collision response
      I3D_cresp_data rd(this, hit_normal, length, cd.cresp_context);
      rd.CopyInputValues(cd);
      bool b = cd.callback(rd);
      if(!b)
         return;
   }

   cd.SetReturnValues(length, this, hit_normal);

   vol_flags |= VOLF_DEBUG_TRIGGER;
}

/*
//----------------------------
// Sphere - BSP intersection (static, move)
//----------------------------
// Slide move along collision planes.
// Return false if move was stopped.
// Algo:
// Testing line (from, to). Trace for closest collision, accept first part (from, impact_point).
// Now response velocity (impact_point, to), ie second part of line. We clip this velocity
// vector by plane normal and recieve new_direction. New to we get by equtation
// new_to = impact_point + new_direction. 
// This relative solution don't require impact plane's d or to_pos (both can be computed from
// impact_point), which is main adventage when trying response group of objects, where 
// every volume collide on different line (parallel).
static bool SolveCollision(S_vector &solve_dir, const vector<S_vector> &clip_planes,
   const S_vector &orig_dir, const S_vector &primary_velocity){

   int num_planes = clip_planes.size();
   const S_vector *p_clip = &clip_planes.front();
                  //modify dir so it parallels all of the clip planes
   for(int i = num_planes; i--; ){
      const S_vector &clip = p_clip[i];
      ClipVelocity(solve_dir, clip);
      COL_DEBUG(C_fstr("use clip plane [%.2f, %.2f, %.2f], move dir change to: [%.2f, %.2f, %.2f]", 
         clip.x, clip.y, clip.z, solve_dir.x, solve_dir.y, solve_dir.z));
                              //check parallel/back
      for (int j = num_planes; j--; ){
		   if(j != i){
            float f = solve_dir.Dot(p_clip[j]);
            //if(CHECK_ZERO_LESS(f))
            if(f < -MRG_ZERO)
            {
               COL_DEBUG(C_fstr("velocity-clip dot: %.8f", f));
				   break;	// not ok
            }
		   }
      }
      if(j == -1) 
         break;               //all clip planes parallel, break
   }

   if(i != -1){
                        // ok, slide along plane, just keep modified dir
      COL_DEBUG(C_fstr("move_dir clipped from [%.2f, %.2f, %.2f] to [%.2f, %.2f, %.2f]",
         orig_dir.x, orig_dir.y, orig_dir.z, solve_dir.x, solve_dir.y, solve_dir.z));
   }
   else{
                        // planes forming line, slide it
	   if(num_planes != 2){
                              //blocked by more planes
         //solve_dir = orig_dir;
         solve_dir.Zero();
         COL_DEBUG(C_fstr("move bloked by %i planes", num_planes));
         return false;
	   }

      int last_clip = 0;
      int prev_clip = 1;
      S_vector line_dir = p_clip[prev_clip].Cross(p_clip[last_clip]);
      float d = line_dir.Dot(orig_dir);
      solve_dir = line_dir * d;
      COL_DEBUG(C_fstr("forming slide line: [%.2f, %.2f, %.2f]", solve_dir.x, solve_dir.y, solve_dir.z));
   }

                        // if new velocity is against the original velocity, stop to avoid occilations in corners
#if 1
   float f3 = primary_velocity.Dot(solve_dir);
	if(CHECK_ZERO_LESS(f3))
   {
      COL_DEBUG(C_fstr("velocity against primary dir[%.2f, %.2f, %.2f], dot: %.8f",
         primary_velocity.x, primary_velocity.y, primary_velocity.z, f3));
      solve_dir.Zero();
		return false;
	}
#endif
   return true;
}
*/

//----------------------------

bool C_bsp_tree::TraceSphere(const S_trace_help &th)const{

#if 1

   if(!nodes.GetRoot())
      return false;

   if(IsMrgZeroLess(th.dir_magn))
      return false;

   bool b = NodeIntersection_MS(*nodes.GetRoot(), .0f, th.dir_magn, th);
   return b;

#else
   bool ret = false;
   const S_bsp_triface *p_trifaces = faces.begin();
   for(int i = 0; i < faces.size(); i++)
   //int i = 3;
   {
      bool b = CheckCol_MS_F(p_trifaces[i], th.from, th.to, th.radius, th.collision_list);
      if(b){
         p_trifaces[i].highlight = true;
         ret = true;
      }
   }
   return ret;
#endif
}

//----------------------------

bool C_bsp_tree::SphereStaticIntersection(I3D_collision_data &cd) const{
 
   if(!nodes.GetRoot())
      return false;
   C_bitfield face_cache(faces.size());
   C_bitfield frame_cache(frames.Size());
   return NodeIntersection_S(*nodes.GetRoot(), cd, face_cache, frame_cache);
}

//----------------------------

bool C_bsp_tree::FaceListItersection_S(const S_bsp_node &node, I3D_collision_data &cd, float plane_dist,
   C_bitfield &face_cache, C_bitfield &frame_cache) const{

   bool ret(false);
   const BSP_index *pflist = node.GetFaces();

   const S_bsp_triface *pfaces = &faces.front();
   for(int i = node.NumFaces(); i--; ){
      dword face_index = pflist[i];

                              //check if we tested it before
      if(face_cache.IsOn(face_index))
         continue;
      face_cache.SetOn(face_index);

      const S_bsp_triface &tf = pfaces[face_index];

                              //check if frame was tested before
      if(frame_cache.IsOn(tf.origin.frm_id))
         continue;
      frame_cache.SetOn(tf.origin.frm_id);

                              //recompute distance of sphere to face plane
      float sf_dist(plane_dist);
      const S_plane &face_plane = planes[tf.plane_id];
      //if(!tf.plane_side) face_plane.Invert();

      if(tf.plane_id != node.plane_id){
         sf_dist = cd.from.DistanceToPlane(face_plane);
                                    //fast quit: sphere not touch the plane
         if(I3DFabs(sf_dist) > cd.radius){
            continue;
         }
      }
      bool b = CheckCol_S_F(tf, cd, sf_dist);
      if(b){
         bool accept = true;
         if(cd.callback){
            CPI3D_frame origin_frm = frames.GetFrame(tf.origin.frm_id);
            I3D_cresp_data rd(const_cast<PI3D_frame>(origin_frm), face_plane.normal, sf_dist, cd.cresp_context);
            rd.CopyInputValues(cd);
            rd.face_index = tf.origin.GetFaceIndex();
            accept =  cd.callback(rd);
         }
         tf.highlight = accept;
         ret |= accept;
      }
   }
   return ret;
}

//----------------------------

bool C_bsp_tree::NodeIntersection_S(const S_bsp_node &node, I3D_collision_data &cd,
   C_bitfield &face_cache, C_bitfield &frame_cache) const{


                              //signed distance of sphere origin to plane defined by Node
   E_PLANE_SIDE near_side, far_side;
   float dist = GetPointPlaneDistance(node.plane_id, cd.from, near_side, far_side);
   assert(near_side != far_side); //debug
                              //child of Node for half-space containing the origin of sphere
   const S_bsp_node *tree_near = node.GetChildBySide(near_side);
                              //the "other" child of Node -- i.e. not equal to near
   const S_bsp_node *tree_far = node.GetChildBySide(far_side);

                              
   if((dist > cd.radius) || (dist < -cd.radius)){
                              //whole sphere is on near side
      if(tree_near)
         return NodeIntersection_S(*tree_near, cd, face_cache, frame_cache);
   }else{
                              //sphere intersect plane
      bool any_hit = false;
      if(tree_far){
         bool hit = NodeIntersection_S(*tree_far, cd, face_cache, frame_cache);
         any_hit |= hit;
      }
                           //test if sphere collide with faces in node (on partion plane)
      bool hit = FaceListItersection_S(node, cd, dist, face_cache, frame_cache);
      any_hit |= hit;

      if(tree_near){
         bool hit = NodeIntersection_S(*tree_near, cd, face_cache, frame_cache);
         any_hit |= hit;
      }
      return any_hit;
   }
   return false;
}

//----------------------------

bool C_bsp_tree::CheckCol_S_F(const S_bsp_triface &tf, I3D_collision_data &cd, float plane_dist) const{

                              // count the number of triangle vertices inside the sphere
   for(int i=3; i--; ){
      //float point_dist = (vertices[tf.vertices_id[i]] - cd.from).Magnitude();
      float point_dist = (vertices[tf.vertices_id[i]] - cd.from).Square();
                              //fast quit: point inside sphere
      if(point_dist < (cd.radius*cd.radius))
         return true;
   }
                              //all point are outside of sphere, 
                              //get closest point on triangle plane
   const S_plane &face_plane = planes[tf.plane_id];
   //if(!tf.plane_side) face_plane.Invert();
   S_vector point_on_plane = cd.from - face_plane.normal*plane_dist;

                              //distance of point to edge planes
   bool inside_ep[3];
   for(int j = 3; j--; ){
      float ep_dist = -point_on_plane.DistanceToPlane(tf.edge_planes[j]);
      inside_ep[j] = CHECK_ZERO_GREATER(ep_dist);
      if(!inside_ep[j])
         break;
   }
                              //fast quit: point inside of triangle
   if(inside_ep[0]&&inside_ep[1]&&inside_ep[2])
      return true;

                              //check intersection of edges and sphere
   float edge_magnitude[3];
   S_vector edge_dir[3];

   int j0 = 0;
   for(j = 3; j--; ){
      int j1 = j0;
      j0 = j;

      const S_vector &vert_tf = vertices[tf.vertices_id[j]];
      const S_vector &vert_tf1 = vertices[tf.vertices_id[j1]];
      S_vector edge = vert_tf - vert_tf1;
      //edge = tf._edges[j];

      edge_magnitude[j] = edge.Magnitude();
      edge_dir[j] = -edge; edge_dir[j].Normalize();
      //float dist_to_edge = cd.from.DistanceToLine(tf.verticies[j], edge_dir[j]);
      //float pos_on_line = cd.from.PositionOnLine(tf.verticies[j], edge_dir[j]);
      float dist_to_edge = cd.from.DistanceToLine(vert_tf, edge_dir[j]);
      float pos_on_line = cd.from.PositionOnLine(vert_tf, edge_dir[j]);

      assert(CHECK_ZERO_GREATER(dist_to_edge));
      bool edge_intersect = (dist_to_edge < cd.radius) &&
         CHECK_ZERO_GREATER(pos_on_line) &&  (pos_on_line < edge_magnitude[j]);
      if(edge_intersect)
         return true;
   }

   return false;
}

//----------------------------
// sphere testing
//----------------------------

void I3D_volume::CheckCol_S_S_dyn(const S_trace_help &th) const{

                              //Algo:
                              // consider the volume's radius to be the sum of radiuses, and simply solve
                              // line vs. sphere collision

   const S_vector &norm_dir = th.GetNormalizedDir();

   S_vector dir_to_sphere = w_sphere.pos - th.from;
   const float radius_sum = w_sphere.radius + th.radius;
                              //the collision of line and sphere is solved by equation:
                              // d = -b +- sqrt(b^2 - 4 * a * c) / (2 * a)

                              //compute a (squared line's direction)
                              //optimization: since a is always 1.0f (we've got normalized
                              // direction), all we need is to scale top part by 0.5f 

                              //compute b (2 * line's direction dot direction to sphere center)
   float b = norm_dir.Dot(dir_to_sphere);// * 2.0f;
   S_S_DEBUG_DYN(C_xstr("b = %") %b);
   if(b <= 0.0f){
      S_S_DEBUG_DYN("dir not facing sphere - quit");
      return;
   }
                              //compute c (sphere center squared + line's P1 squared
                              // - 2 * line's p1 dot sphere center - squared radius
   float c = dir_to_sphere.Square() - radius_sum*radius_sum;
#if defined _DEBUG && 0
   { float c1 = sphere_pos_dot + th.from_squared - 2.0f * (th.from.Dot(w_sphere.pos)) - (radius_sum*radius_sum);
   assert(I3DFabs(c-c1) < 1e-2f); }
#endif
   S_S_DEBUG_DYN(C_xstr("c = %") %c);

                              //solve the equation (a is ommited, since it is 1.0)
   float d = b * b - c;
   S_S_DEBUG_DYN(C_xstr("d^2 = %") %d);
                              //now we have semi-result, by which sign we may detect:
                              //when d is less than zero, the ray doesn't intersect sphere
   if(CHECK_ZERO_LESS(d)){
      S_S_DEBUG_DYN("missing sphere - exit");
      return;
   }

   d = I3DSqrt(d);
   S_S_DEBUG_DYN(C_xstr("d = %") %d);
                              //solve closer case of the equation
   float u  = (b - d);// * .5f;
   assert(u <= (b + d));//*.5f);
   S_S_DEBUG_DYN(C_xstr("u = %") %u);
   if(u >= th.dir_magn){
      S_S_DEBUG_DYN("line too short - exit");
      return;
   }
                              //now u is distance of closest collision point

                              //get point on the sphere
   S_vector col_pt = th.from + norm_dir * u;
   //drv->DebugPoint(col_pt, .1f, 1, R);
                              //report collision
   th.collision_list.push_back(S_collision_info());
   S_collision_info &ci = th.collision_list.back();
   ci.hit_frm = this;
   ci.vol_source = th.vol_source;
   ci.plane_id = (dword)-1;

   ci.plane.normal = col_pt - w_sphere.pos;
   ci.plane.normal.Normalize();
         
   ci.plane.d = -(ci.plane.normal.Dot(w_sphere.pos)) + radius_sum;
   ci.hit_distance = Max(0.0f, u);
   S_S_DEBUG_DYN(C_xstr(
      "hit_distance %\n"
      )
      %ci.hit_distance
      );
   //drv->DebugPlane(ci.plane, w_sphere.pos);
   vol_flags |= VOLF_DEBUG_TRIGGER;
}

//----------------------------

void I3D_volume::CheckCol_S_CC_dyn(const S_trace_help &th) const{


                              //Algo:
                              // enlarge radius by tested sphere's radius, and solve as line vs. capcyl

   const S_vector &in_pos = w_sphere.pos;
   const S_vector &z_axis = normal[2];
   float radius = world_radius + th.radius;
   //bool capped = (volume_type==I3DVOLUME_CAPCYL);
   const float half_length = world_half_length;

#undef REPORT_COLLISION
#define REPORT_COLLISION \
   th.collision_list.push_back(S_collision_info()); S_collision_info &ci = th.collision_list.back(); \
   ci.hit_frm = this; ci.vol_source = th.vol_source; ci.plane_id = (dword)-1; \
   S_plane &pl = ci.plane; float &hit_distance = ci.hit_distance;

   const S_vector &norm_dir = th.GetNormalizedDir();

   S_vector dir_to_sphere = in_pos - th.from;
                              //position of ray start along ccyl axis
   float pos_on_line = -dir_to_sphere.Dot(z_axis);
   S_vector dir_to_pol = z_axis*pos_on_line + dir_to_sphere;
   //drv->DebugPoint(th.from+dir_to_pol, .1f, 1); drv->DebugLine(th.from, th.from+dir_to_pol, 1);
   float cc = dir_to_pol.Square() - radius*radius;
   bool in_tube = CHECK_ZERO_LESS(cc);
   float dir_cos = z_axis.Dot(norm_dir);
   S_CC_DEBUG(C_xstr("dir_cos = %") %dir_cos);

                              //see if ray start position is inside the capped cylinder
   if(in_tube){
      if(pos_on_line < -half_length)
         pos_on_line = -half_length;
      else
      if(pos_on_line > half_length)
         pos_on_line = half_length;
         /*
         S_vector cap = in_pos + z_axis * pos_on_line;
         if((th.from-cap).Square() < radius*radius){
            //inside_ccyl = true;
                              //we're inside, quit
            S_CC_DEBUG("not outside caps - quit");
            return;
         }
         */
   }else{
      S_vector r = z_axis*dir_cos - norm_dir;
      //drv->DebugLine(cd.from, cd.from+r, 1, R);

      float a = r.Square();
      float b = 2.0f * dir_to_pol.Dot(r);
      float ff = b*b - 4.0f * a * cc;

      if(ff<0.0f || IsMrgZeroLess(a)){
                              //the ray does not intersect the infinite cylinder, but if the ray is
                              // inside and parallel to the cylinder axis it may intersect the end
                              // caps. set 'ff' to cap position to check
         S_CC_DEBUG("ray doesn't hit cylinder - quit");
         return;
      }else{
         ff = I3DSqrt(ff);
         S_CC_DEBUG(C_xstr(
            "a = %\n"
            "ff = %\n"
            )
            %a
            %ff
            );
         a = 1.0f / (2.0f*a);
         float alpha = (-b-ff) * a;
         if(alpha < 0.0f){
	         alpha = (-b+ff) * a;
	         if(alpha < 0.0f)
               return;
         }
         if(alpha >= th.dir_magn){
            S_CC_DEBUG("dir short - quit");                              
            return;
         }

                              //the ray intersects the infinite cylinder. check to see if the
                              // intersection point is between the caps

         S_vector pos = th.from + norm_dir*alpha;
         S_vector q = pos - in_pos;
         ff = q.Dot(z_axis);
         if(ff >= -half_length && ff <= half_length){
            S_vector pt_on_z = in_pos + z_axis*ff;
            REPORT_COLLISION;

            pl.normal = pos - pt_on_z;
            pl.normal.Normalize();
            pl.d = -pt_on_z.Dot(pl.normal) + radius;
            hit_distance = alpha;
            S_CC_DEBUG(C_xstr("hit_distance = %") % hit_distance);
            //drv->DebugPoint(pos); drv->DebugPoint(pt_on_z); drv->DebugPlane(pl, pt_on_z);
            //th.collision_list.pop_back();
	         return;
         }
                              //the infinite cylinder intersection point is not between the caps
                              // set 'pos_on_line' to cap position to check.
         if(ff < 0.0f)
            pos_on_line = -half_length;
         else
            pos_on_line = half_length;
      }
   }

   S_vector sphere_pos = in_pos + z_axis * pos_on_line;
   /*
   if(!capped){
                              //ray parallel to the plane?
      if(!IsAbsMrgZero(dir_cos)){
         //S_CC_DEBUG("parallel to side - quit");
         S_vector end_pos = in_pos + z_axis * (pos_on_line + (pos_on_line<0.0f ? -th.radius : th.radius));
         drv->DebugPoint(end_pos, .1f, 1, G);

         float alpha = (end_pos-th.from).Dot(z_axis) / dir_cos;
         S_CC_DEBUG(C_xstr("alpha = %") % alpha);
         if(alpha>=th.dir_magn){
            S_CC_DEBUG("dir short - quit");
            return;
         }
         S_vector pos = th.from + norm_dir*alpha;
         drv->DebugPoint(pos);
                                 //check if position is withing circle
         S_vector end_pos_dir = pos - end_pos;
         float d_2 = end_pos_dir.Square();
         if(d_2 <= radius*radius){
            //S_CC_DEBUG("missing circle - exit");
            if(d_2 <= world_radius*world_radius){
                                 //hitting end circle plane, report
               S_CC_DEBUG("hitting end circle plane");
               REPORT_COLLISION;
               pl.normal = (pos_on_line<0.0f) ? -z_axis : z_axis;
               pl.d = -pos.Dot(pl.normal);
               hit_distance = Max(0.0f, alpha);
               //drv->DebugPlane(pl, in_pos, B);
               //th.collision_list.pop_back();
               return;
            }
         }
         sphere_pos += end_pos_dir * (world_radius / I3DSqrt(d_2));
      }else{
         sphere_pos -= th.dir_norm * world_radius;
      }
                              //solve using sphere placed on ring of the cylinder's end
      radius = th.radius;
      drv->DebugPoint(sphere_pos, .1f, 1, R);
      return;
   }
   */
                              //check for ray intersection with the caps
                              //'pos_on_line' must indicate the cap position to check
   //drv->DebugPoint(sphere_pos);
   S_vector dir_to_sphere_pos = sphere_pos - th.from;

   float b = norm_dir.Dot(dir_to_sphere_pos);
   S_CC_DEBUG(C_xstr("b = %") %b);
   if(b <= 0.0f){
      S_CC_DEBUG("dir not facing sphere - quit");
      return;
   }

   float c = dir_to_sphere_pos.Square() - radius*radius;
   //c = sphere_pos.Dot(sphere_pos) + th.from_squared - 2.0f * (th.from.Dot(sphere_pos)) - (radius*radius);

                           //note: if C <= 0 then the start of the ray is inside the sphere
   //assert(c >= 0.0f);
   float d = b*b - c;
   S_CC_DEBUG(C_xstr(
      "c = %"
      "d = %"
      )
      %c
      %d
      );
   if(d <= 0.0f){
      S_CC_DEBUG("missing cyl - quit");
      return;
   }
   d = I3DSqrt(d);
   float alpha = b - d;
   if(alpha >= th.dir_magn){
      S_CC_DEBUG("dir short - quit");                              
      return;
   }
   /*
   if(alpha < 0.0f){
      alpha = -b + k;
      if(alpha < 0.0f)
         return;
   }
   */
   //if(alpha > cd.GetHitDistance()) return;
   REPORT_COLLISION;

   S_vector pos = th.from + norm_dir*alpha;
   pl.normal = pos - sphere_pos;
   pl.normal.Normalize();
   pl.d = -pos.Dot(pl.normal);
   hit_distance = Max(0.0f, alpha);

   //drv->DebugPoint(pos); drv->DebugPlane(pl, pos);
   S_CC_DEBUG(C_xstr("hit_distance = %") % hit_distance);
}

//----------------------------

bool I3D_volume::CheckCol_S_B_dyn(const S_trace_help &th, bool passive) const{

   float radius = th.radius;

   int i;

   enum E_SIDE_MODE{          //bits
      SM_MIN_OUT = 1,         //absolutely outside
      SM_MIN_OUT_CROSS = 2,   //crossing, center outside
      SM_MIN_IN_CROSS = 4,    //crossing, center inside
      SM_IN = 8,              //absolutely inside
      SM_MAX_IN_CROSS = 0x10, //crossing, center inside
      SM_MAX_OUT_CROSS = 0x20,//crossing, center outside
      SM_MAX_OUT = 0x40,      //absolutely outside
      SM_MIN_CENTER_OUT = 0x80,  //center less than min axis
      SM_MAX_CENTER_OUT = 0x100, //center greater than max axis
   };

   struct S_sphere_info{
                              //info for all 3 volume-aligned axes
      float d[3];
      dword side_mode[3];
      int num_sides_center_out;
      int solid_side_index;
      S_sphere_info():
         num_sides_center_out(0)
      {}
   };
   S_sphere_info si_from, si_to;

   S_vector edge_col_to;      //must contain copy of (adjusted) th.to before entering edge_col

                              //float bitmask of radius (radius is always positive)
   int radius_i = FLOAT_BITMASK(radius);

                              //compute what's necessary
   S_B_DEBUG_DYN(C_fstr("S_B collision on %s:", (const char*)GetName()));
   for(i=0; i<3; i++){
      float d_f_min, d_f_max = 0.0f;
      float d_t_min, d_t_max;

                              //compute info for 'from'
      float &d_from = si_from.d[i];
      float &d_to = si_to.d[i];

      d_from = - (normal[i].Dot(th.from));
      d_to = - (normal[i].Dot(th.to));

                              //fast quits - absolute bottleneck - optimized
      d_f_min = d_min[i] + d_from;
      d_t_min = d_min[i] + d_to;
      if(FLOAT_BITMASK(d_f_min) >= radius_i){
         if(FLOAT_BITMASK(d_t_min) >= radius_i){
            S_B_DEBUG_DYN("both 'from' & 'to' out of side, quitting");
            /*
            if(FLOAT_BITMASK(d_f_max) < th.critical_dist_i){
               AddCriticalVolume((PI3D_volume)this);
               S_B_DEBUG_DYN("'from' closer than critical_dist, adding to critical list");
            }
            */
            return false;
         }
                              //no way, compute the rest
                              //'from'
         si_from.side_mode[i] = SM_MIN_OUT | SM_MIN_CENTER_OUT;
         ++si_from.num_sides_center_out;
         si_from.solid_side_index = i;
                              //'to'
         if(CHECK_ZERO_GREATER(d_t_min)){
            si_to.side_mode[i] = SM_MIN_OUT_CROSS | SM_MIN_CENTER_OUT;
            ++si_to.num_sides_center_out;
            si_to.solid_side_index = i;
         }else
         if(CHECK_ZERO_GREATER(d_t_max=d_max[i] - d_to)){
            si_to.side_mode[i] = ((FLOAT_BITMASK(d_t_max) >= radius_i) ? SM_MAX_OUT : SM_MAX_OUT_CROSS) | SM_MAX_CENTER_OUT;
            ++si_to.num_sides_center_out;
            si_to.solid_side_index = i;
         }else
            si_to.side_mode[i] = (CHECK_ZERO_GREATER(d_t_min) || FLOAT_BITMASK(d_t_min) < radius_i) ? SM_MIN_IN_CROSS :
               (CHECK_ZERO_GREATER(d_t_max) || FLOAT_BITMASK(d_t_max) < radius_i) ? SM_MAX_IN_CROSS : SM_IN;
      }else{
         d_f_max = d_max[i] - d_from;
         d_t_max = d_max[i] - d_to;
         if(FLOAT_BITMASK(d_f_max) >= radius_i){
            if(FLOAT_BITMASK(d_t_max) >= radius_i){
               S_B_DEBUG_DYN("both 'from' & 'to' out of side, quitting");
               /*
               if(FLOAT_BITMASK(d_f_max) < th.critical_dist_i){
                  AddCriticalVolume((PI3D_volume)this);
                  S_B_DEBUG_DYN("'from' closer than critical_dist, adding to critical list");
               }
               */
               return false;
            }
                              //no way, compute the rest
                              //'from'
            si_from.side_mode[i] = SM_MAX_OUT | SM_MAX_CENTER_OUT;
            ++si_from.num_sides_center_out;
            si_from.solid_side_index = i;
         }else{
                              //no way, compute the rest
                              //'from'
            if(CHECK_ZERO_GREATER(d_f_min)){
               si_from.side_mode[i] = SM_MIN_OUT_CROSS | SM_MIN_CENTER_OUT;
               ++si_from.num_sides_center_out;
               si_from.solid_side_index = i;
            }else
            if(CHECK_ZERO_GREATER(d_f_max)){
               si_from.side_mode[i] = SM_MAX_OUT_CROSS | SM_MAX_CENTER_OUT;
               ++si_from.num_sides_center_out;
               si_from.solid_side_index = i;
            }else
               si_from.side_mode[i] = (CHECK_ZERO_GREATER(d_f_min) || FLOAT_BITMASK(d_f_min) < radius_i) ? SM_MIN_IN_CROSS :
                  (CHECK_ZERO_GREATER(d_f_max) || FLOAT_BITMASK(d_f_max) < radius_i) ? SM_MAX_IN_CROSS : SM_IN;
         }
                              //'to'
         if(CHECK_ZERO_GREATER(d_t_min)){
            si_to.side_mode[i] = ((FLOAT_BITMASK(d_t_min) >= radius_i) ? SM_MIN_OUT : SM_MIN_OUT_CROSS) | SM_MIN_CENTER_OUT;
            ++si_to.num_sides_center_out;
            si_to.solid_side_index = i;
         }else
         if(CHECK_ZERO_GREATER(d_t_max)){
            si_to.side_mode[i] = ((FLOAT_BITMASK(d_t_max) >= radius_i) ? SM_MAX_OUT : SM_MAX_OUT_CROSS) | SM_MAX_CENTER_OUT;
            ++si_to.num_sides_center_out;
            si_to.solid_side_index = i;
         }else
            si_to.side_mode[i] = (CHECK_ZERO_GREATER(d_t_min) || FLOAT_BITMASK(d_t_min) < radius_i) ? SM_MIN_IN_CROSS :
               (CHECK_ZERO_GREATER(d_t_max) || FLOAT_BITMASK(d_t_max) < radius_i) ? SM_MAX_IN_CROSS : SM_IN;
      }
                              //dump info
      S_B_DEBUG_DYN(C_fstr("%i: d_from: %.2f d_to: %.2f dfmax: %.2f dfmin: %.2f dtmax: %.2f dtmin: %.2f, from.side_mode = 0x%x, to.side_mode = 0x%x",
         i, si_from.d[i], si_to.d[i], d_f_max, d_f_min, d_t_max, d_t_min,
         si_from.side_mode[i], si_to.side_mode[i]));
      S_B_DEBUG_DYN(C_fstr("   from.side_mode = 0x%x, to.side_mode = 0x%x",         
         si_from.side_mode[i], si_to.side_mode[i]));
   }
   S_B_DEBUG_DYN(C_fstr("from.num_sides_center_out = %i, to.num_sides_center_out = %i", si_from.num_sides_center_out, si_to.num_sides_center_out));

   S_vector coll_point;       //used much much later

                              //info for solid collision
   int solid_axis_index = -1; //put invalid value here
   bool solid_max_side = true;

   if(passive){
      if(si_from.num_sides_center_out<2)
      if(!((si_from.side_mode[0]|si_from.side_mode[1]|si_from.side_mode[2])&(SM_MIN_OUT|SM_MAX_OUT))){
         S_B_DEBUG_DYN("Fast quit 2 - 'from' touching box, passive test succeeded");
         vol_flags |= VOLF_DEBUG_TRIGGER;
         return true;
      }
      if(si_to.num_sides_center_out<2)
      if(!((si_to.side_mode[0]|si_to.side_mode[1]|si_to.side_mode[2])&(SM_MIN_OUT|SM_MAX_OUT))){
         S_B_DEBUG_DYN("Fast quit 2 - 'to' touching box, passive test succeeded");
         vol_flags |= VOLF_DEBUG_TRIGGER;
         return true;
      }
   }else{   //passive
                              //fast quit 3 - comming from inside
      if(!si_from.num_sides_center_out){
         S_B_DEBUG_DYN("Fast quit 3 - comming from inside");
         return false;
      }
      if(si_from.num_sides_center_out==1){
         if(!si_to.num_sides_center_out){
            solid_axis_index = si_from.solid_side_index;
            solid_max_side = (si_from.side_mode[solid_axis_index]&SM_MAX_CENTER_OUT);
            goto solid_col;
         }
         if(si_to.num_sides_center_out==1 && (si_from.solid_side_index == si_to.solid_side_index)){
                              //check if we're comming towards the side
            float d_delta = si_to.d[si_from.solid_side_index]-si_from.d[si_from.solid_side_index];
            if(si_from.side_mode[si_from.solid_side_index]&SM_MAX_CENTER_OUT){
               if(d_delta<=0.0f){
                  S_B_DEBUG_DYN("Fast quit - going out of side");
                  return false;
               }
            }else{
               if(d_delta>=0.0f){
                  S_B_DEBUG_DYN("Fast quit - going out of side");
                  return false;
               }
            }
            solid_axis_index = si_from.solid_side_index;
            solid_max_side = (si_from.side_mode[solid_axis_index]&SM_MAX_CENTER_OUT);
            goto solid_col;
         }
      }
   }
   int hit_index[3];
   float hit_dist[3];         //with min|max side
   bool hit_front[3];

   int hit_sorted_axes[3];

   for(i=0; i<3; i++){
      if(si_from.side_mode[i]&(SM_MIN_CENTER_OUT|SM_MAX_CENTER_OUT)){
         hit_index[i] = bool(si_from.side_mode[i]&SM_MAX_CENTER_OUT);
         hit_front[i] = true;
      }else{
         hit_index[i] = bool(si_from.d[i]>si_to.d[i]);
         hit_front[i] = false;
      }

      float u;
      u = normal[i].Dot(th.dir);
      if(IsAbsMrgZero(u)){
         hit_dist[i] = 1e+10f;
      }else
         hit_dist[i] = (si_from.d[i] - (!hit_index[i] ? -d_min[i] : d_max[i])) / u;
      S_B_DEBUG_DYN(C_fstr("hit_index = %i, side hit at distance %.2f, hit_front = %i", hit_index[i], hit_dist[i], hit_front[i]));
   }
                              //sort axes by hit distance
   if(hit_dist[0]<=hit_dist[1]){
      if(hit_dist[0]<=hit_dist[2]){
         hit_sorted_axes[0] = 0;

         if(hit_dist[1]<=hit_dist[2]) hit_sorted_axes[1]=1, hit_sorted_axes[2]=2;
         else hit_sorted_axes[1]=2, hit_sorted_axes[2]=1;
      }else{
         hit_sorted_axes[0] = 2;
         hit_sorted_axes[1]=0, hit_sorted_axes[2]=1;
      }
   }else
   if(hit_dist[0]<=hit_dist[2]){
      hit_sorted_axes[0] = 1;
      hit_sorted_axes[1]=0, hit_sorted_axes[2]=2;
   }else{
      if(hit_dist[1]<=hit_dist[2]){
         hit_sorted_axes[0] = 1;
         hit_sorted_axes[1] = 2;
      }else{
         hit_sorted_axes[0] = 2;
         hit_sorted_axes[1] = 1;
      }
      hit_sorted_axes[2]=0;
   }
   S_B_DEBUG_DYN(C_fstr("sorted axes: %i %i %i", hit_sorted_axes[0], hit_sorted_axes[1], hit_sorted_axes[2]));

   if(si_from.num_sides_center_out==1){
                              //we're comming from inside of single side

                              //check if closest side is the one 'out' - if yes, it's solid collision
      if(si_from.side_mode[hit_sorted_axes[0]]&(SM_MIN_CENTER_OUT|SM_MAX_CENTER_OUT)){
         if(passive){
            S_B_DEBUG_DYN("hitting cube - test successfull");
            vol_flags |= VOLF_DEBUG_TRIGGER;
            return true;
         }
         solid_axis_index = hit_sorted_axes[0];
         solid_max_side = (hit_index[solid_axis_index]);
         goto solid_col;
      }
                              //this may be edge or corner collision now,

                              //get edge of first closest sides
      S_vector edge_pt = bbox.min;
      if(hit_index[hit_sorted_axes[0]])
         edge_pt += world_side[hit_sorted_axes[0]];
      if(hit_index[hit_sorted_axes[1]])
         edge_pt += world_side[hit_sorted_axes[1]];
      const S_vector &edge_dir = world_side[hit_sorted_axes[2]];
      S_B_DEBUG_DYN(C_fstr("checking against edge, point = %.2f, %.2f, %.2f, dir = %.2f, %.2f, %.2f",
         edge_pt.x, edge_pt.y, edge_pt.z,
         edge_dir.x, edge_dir.y, edge_dir.z));

                              //check if fahrest side is the one 'out' - if yes, it may be corner collision
      if(si_from.side_mode[hit_sorted_axes[2]]&(SM_MIN_CENTER_OUT|SM_MAX_CENTER_OUT)){
         S_B_DEBUG_DYN("corner collision");
                              //get closest point of 'to' and the point
         //if(th.dir_squared < MRG_ZERO) return false;
         coll_point = edge_pt;
         float u = th.dir.Dot(coll_point-th.from) * th.r_dir_squared;
         if(u<=.005f){  //passed the corner?
            S_B_DEBUG_DYN("Fast quit 6 - past the corner");
            return false;
         }
         S_B_DEBUG_DYN(C_fstr("Closest point to corner at distance %.2f", u));
         edge_col_to = th.to;
         if(u < 1.0f) edge_col_to = th.from + th.dir*u;
      }else{
         S_B_DEBUG_DYN("edge collision");
         float u, u1;
                              //should always pass
         if(!LineIntersection(th.from, th.dir, edge_pt, edge_dir, u, u1)){
            //todo("test this!!!");
                              //too small th.dir...? solve as distance of point to line
            u = world_side_dot[hit_sorted_axes[2]];
            if(IsAbsMrgZero(u))  //is this possible???
               return false;
            u1 = edge_dir.Dot(th.to-edge_pt) / u;
            u = 1.0f;
         }
         S_B_DEBUG_DYN(C_fstr("Collision of edge and th.dir at distances %.2f, %.2f", u1, u));
         edge_col_to = th.to;
         if(u <= 1.0f){
            if(IsAbsMrgZero(u)) return false;
            coll_point = edge_pt + edge_dir * u1;
            u = 1.0f/u;
            coll_point = th.from + (coll_point-th.from)*u;
            radius *= u;
         }else{
                        //re-compute closest point on edge
            u1 = edge_dir.Dot(th.to-edge_pt) / world_side_dot[hit_sorted_axes[2]];
            S_B_DEBUG_DYN(C_fstr("re-computing point on edge at distance %.2f", u1));
            coll_point = edge_pt + edge_dir * u1;
         }
      }
   }else{
      int master_axis;        //facing and farer
      int slave_axis;         //facing and closer
      int third_axis;

      for(i=3; i--; ) if(hit_front[hit_sorted_axes[i]]) break;
      master_axis = hit_sorted_axes[i];
      if(hit_dist[master_axis]<=0.0f){
         S_B_DEBUG_DYN("Fast quit - going out of master axis");
         return false;
      }

      for(; i--; ) if(hit_front[hit_sorted_axes[i]]) break;
      assert(i!=-1);
      slave_axis = hit_sorted_axes[i];
      third_axis = 3-(master_axis+slave_axis);

      S_B_DEBUG_DYN(C_fstr("master_axis = %i, slave_axis = %i", master_axis, slave_axis));

                              //edge_side_1 is alway master_axis
      int edge_side_2;
      bool var_limit_to_side;
                              //for case of solid col
      solid_axis_index = master_axis;
      solid_max_side = (si_from.side_mode[master_axis]&SM_MAX_CENTER_OUT);

                              //detect if collision with master/slave edge
      if(si_from.side_mode[slave_axis]&si_to.side_mode[slave_axis]&(SM_MIN_CENTER_OUT|SM_MAX_CENTER_OUT)){
         edge_side_2 = slave_axis;
         //var_limit_to_side=true;
         var_limit_to_side=false;
         edge_col_to = th.to;
         goto edge_col;
      }
                              //detect if collision with master/slave-1 edge
                              //if 'to' is on opposite side of box...
      if(si_to.side_mode[slave_axis]&(SM_MIN_CENTER_OUT|SM_MAX_CENTER_OUT)){
                              //...and dist to master is greater than to slave-1

         float u = normal[slave_axis].Dot(th.dir);
         assert(!IsAbsMrgZero(u));
         if(I3DFabs(u) > .001f){
            hit_dist[slave_axis] = -(normal[slave_axis].Dot(th.from) + (hit_index[slave_axis] ? -d_min[slave_axis] : d_max[slave_axis])) / u;

            if(hit_dist[slave_axis] < hit_dist[master_axis]){
               hit_index[slave_axis] = !hit_index[slave_axis];

               edge_side_2 = slave_axis;
               var_limit_to_side=false;
               edge_col_to = th.to;
               goto edge_col;
            }
         }
      }
                              //detect if collision with master/third edge
      if(si_to.side_mode[third_axis]&(SM_MIN_CENTER_OUT|SM_MAX_CENTER_OUT)){
                              //...and dist to master is greater than to third

                              //flip third axis if necessary
         if(si_from.num_sides_center_out==3)
         if(!(bool((si_to.side_mode[third_axis]&SM_MAX_CENTER_OUT)&hit_index[third_axis]))){
                              //recompute hit dist on third axis
            float u = normal[third_axis].Dot(th.dir);
            assert(!IsAbsMrgZero(u));
            if(I3DFabs(u) > .001f){
               hit_dist[third_axis] = -(normal[third_axis].Dot(th.from) + (hit_index[third_axis] ? -d_min[third_axis] : d_max[third_axis])) / u;
            }else goto solid_col_p;
            hit_index[third_axis] = !hit_index[third_axis];
         }

         if(hit_dist[third_axis] < hit_dist[master_axis]){
            edge_side_2 = third_axis;
            var_limit_to_side=false;
            edge_col_to = th.to;
            goto edge_col;
         }
      }
      goto solid_col_p;

edge_col:
      S_vector edge_pt = bbox.min;
      if(hit_index[master_axis])
         edge_pt += world_side[master_axis];
      if(hit_index[edge_side_2])
         edge_pt += world_side[edge_side_2];
      i = 3-(master_axis+edge_side_2);
      const S_vector &edge_dir = world_side[i];
      const float &edge_dot = world_side_dot[i];

      S_B_DEBUG_DYN("edge collision");
      float u, u1;
                           //should always pass
      if(!LineIntersection(th.from, th.dir, edge_pt, edge_dir, u, u1)){
                              //too small th.dir...? solve as distance of point to line
         S_B_DEBUG_DYN("LineIntersection failed, solving point-to-line distance");

         u = edge_dot;
         if(IsAbsMrgZero(u)) return false;   //is this possible???
         u1 = edge_dir.Dot(edge_col_to-edge_pt) / u;
         u = 1.0f;
      }
      S_B_DEBUG_DYN(C_fstr("Collision of edge and th.dir at distances %.2f, %.2f", u1, u));

      if(u < 1.0f){
                           //limit edge range - we get corners for free at extremities
         u1 = Max(0.0f, Min(1.0f, u1));
         coll_point = edge_pt + edge_dir * u1;

         if(var_limit_to_side){
                           //fail to solid
            goto solid_col_p;
         }else
         if(u<0.0f){
            S_B_DEBUG_DYN("Fast quit - going out of edge");
            return false;
         }else{
            if(IsMrgZeroLess(u)) return false;
            //edge_col_to = th.from + th.dir * u;
            edge_col_to = th.to;

            u = 1.0f/u;
            coll_point = th.from + (coll_point-th.from)*u;
            radius *= u;
         }
      }else{
                     //re-compute closest point on edge - avoid cutting into the edge
         u = edge_dot;
         if(IsAbsMrgZero(u)) return false;   //is this possible???
         u1 = edge_dir.Dot(edge_col_to-edge_pt) / u;
         S_B_DEBUG_DYN(C_fstr("re-computing point on edge at distance %.2f", u1));
                           //limit edge range - we get corners for free at extremities
         u1 = Max(0.0f, Min(1.0f, u1));
         coll_point = edge_pt + edge_dir * u1;
      }
   }

                              //solve collision to given point
   {
      S_vector col_normal = edge_col_to - coll_point;

      float dist_2 = col_normal.Square();
      if(dist_2<MRG_ZERO){
                              //this shoudn't happen
         if(solid_axis_index==-1) return false;
         goto solid_col_p;
      }
      float dist = I3DSqrt(dist_2);

                              //fast quit 5 - missing closest point
      if(dist >= radius){
         S_B_DEBUG_DYN("Fast quit 5 - missing closest point");
         return false;
      }
      if(passive){
         S_B_DEBUG_DYN("hitting edge/corner - test successful");
         vol_flags |= VOLF_DEBUG_TRIGGER;
         return true;
      }
      S_B_DEBUG_DYN("solving edge/corner collision");
                              //normalize collision normal
      col_normal /= dist;
      float col_d = - col_normal.Dot(coll_point);
                              //collision response
                              //save collision for later response
      {
         //S_B_DEBUG_DYN(C_fstr("col_d: %.2f, d_to: %.2f, u: %.2f", col_d, d_to, u));
         th.collision_list.push_back(S_collision_info());
         S_collision_info &ci = th.collision_list.back();
         ci.plane.normal = col_normal;
         ci.plane.d = col_d - radius;
         ci.hit_frm = this;
         ci.vol_source = th.vol_source;
                              //dynamic volumes has not plane in bsp tree, assign one negative
         ci.plane_id = (dword)-1;
                              //compute in distace:
         ci.hit_distance = .0f;
         ClipLineByBox(th, ci.hit_distance);
      }
   }
   vol_flags |= VOLF_DEBUG_TRIGGER;
   return true;

solid_col_p:
   if(passive){
      vol_flags |= VOLF_DEBUG_TRIGGER;
      return true;
   }
solid_col:
   assert(!passive);
   S_B_DEBUG_DYN(C_fstr("solving solid collision in axis %i", solid_axis_index));
                              //perform solid response on side
                              //save collision for later response
   {
      //S_B_DEBUG_DYN(C_fstr("col_d: %.2f, d_to: %.2f, u: %.2f", col_d, d_to, u));
      th.collision_list.push_back(S_collision_info());
      S_collision_info &ci = th.collision_list.back();
      ci.plane.normal = solid_max_side ? normal[solid_axis_index] : -normal[solid_axis_index];
      ci.plane.d = solid_max_side ? d_max[solid_axis_index] - radius : d_min[solid_axis_index] - radius;
      ci.hit_frm = this;
      ci.vol_source = th.vol_source;
                           //dynamic volumes has not plane in bsp tree, assign one negative
      ci.plane_id = (dword)-1;
                           //compute in distace:
      ci.hit_distance = .0f;
      ClipLineByBox(th, ci.hit_distance);
   }
   vol_flags |= VOLF_DEBUG_TRIGGER;
   return true;
}

/*
//----------------------------
// another tried algoritmus.
bool I3D_volume::CheckCol_S_R_dyn(const S_trace_help *th) const{

                              //rectangle clip planes, line and points:
   vector<S_plane>clip_planes;
   vector<S_plane>bevel_lines;
   vector<S_plane>bevel_points;

                              //collect planes
                              //note: this algoritmus not work for zero radius, because 'z' planes clips entire line without tolerance.
                              //for correct result when zero radius passed we should choase and use only one 'z' plane (looking to dir).
   for(int i = 0; i < 3; i++){
      clip_planes.push_back(S_plane(-normal[i], d_min[i]));
      clip_planes.push_back(S_plane(normal[i], d_max[i]));
   }
                              //bevel edges
   {
      const S_vector coll_pt = bbox.min;
      for(i = 0; i < 2; i++){
         S_plane bevel;
         const S_vector &col_side = side[i];
         if(MakeBevelPlane(normal[2], normal[i], coll_pt + col_side, th->radius, bevel))            
            bevel_lines.push_back(bevel);
         if(MakeBevelPlane(-normal[2], normal[i], coll_pt + col_side, th->radius, bevel))
            bevel_lines.push_back(bevel);
                              //opposite side
         if(MakeBevelPlane(normal[2], -normal[i], coll_pt, th->radius, bevel))
            bevel_lines.push_back(bevel);
         if(MakeBevelPlane(-normal[2], -normal[i], coll_pt, th->radius, bevel))
            bevel_lines.push_back(bevel);
      }
   }
                              //bevel points
   {
      S_vector coll_pt = bbox.min;
      for(i = 0; i < 2; i++){
         for(int j = 0; i < 2; i++){
            S_plane bevel;
            S_vector col_side = (i%2) ? side[j] : - side[j];

            S_vector norm1, norm2;
            norm1 = (j) ? normal[0] : -normal[0];
            norm2 = (i%2) ? normal[1] : -normal[1];
            if(MakeBevelPlane(norm1, norm2, coll_pt, th->radius, bevel))
               bevel_points.push_back(bevel);

            coll_pt += col_side;
         }
      }
   }
                              //clip it all
   float d_in(.0f), d_out(th->dir_magn);
   for(i = clip_planes.size(); i--; ){
      const S_plane &pl = clip_planes[i];
      E_CLIP_LINE cr = ClipLineByPlane(th->from, th->dir_norm, pl.normal, pl.d - th->radius, d_in, d_out);
      if(cr == CL_OUTSIDE)
         return false;        //entire segment out of edge plane
   }
#if 1
   for(i = bevel_lines.size(); i--; ){
      const S_plane &pl = bevel_lines[i];
      E_CLIP_LINE cr = ClipLineByPlane(th->from, th->dir_norm, pl.normal, pl.d, d_in, d_out);
      if(cr == CL_OUTSIDE)
         return false;        //entire segment out of edge plane
   }
#endif

#if 0
   for(i = bevel_points.size(); i--; ){
      const S_plane &pl = bevel_points[i];
      E_CLIP_LINE cr = ClipLineByPlane(th->from, th->dir_norm, pl.normal, pl.d, d_in, d_out);
      if(cr == CL_OUTSIDE)
         return false;        //entire segment out of edge plane
   }
#endif

   th->collision_list.push_back(S_collision_info());
   S_collision_info &ci = th->collision_list.back();
   //ci.plane.normal = max_side[2] ? normal[2] : -normal[2];
   //ci.plane.d = max_side[2] ? d_max[2] - th->radius : d_min[2] - th->radius;
   ci.plane.normal = normal[2];
   ci.plane.d = d_max[2] - th->radius;

   ci.hit_frm = this;
   ci.vol_source = th->vol_source;
                        //dynamic volumes has not plane in bsp tree, assign one negative
   ci.plane_id = -1;
   ci.hit_distance = Min(Max(.0f, d_in), th->dir_magn);

   return true;
}
*/

//----------------------------

void I3D_volume::CheckCol_S_R_dyn(const S_trace_help &th) const{

   int i;
   float u;

   float d_from[3];
   float d_to[3];
   float dist_from_max[3];
   float dist_from_min[3];
   float dist_to_max[3];
   float dist_to_min[3];

                              //compute what's necessary
                              //check sides out
   S_R_DEBUG_DYN(C_fstr("volume %s distances:", (const char*)GetName()));
   for(i=3; i--; ){

      d_from[i] = - normal[i].Dot(th.from);
      dist_from_max[i] = d_max[i] - d_from[i];
      dist_from_min[i] = d_min[i] + d_from[i];

      d_to[i] = - normal[i].Dot(th.to);
      dist_to_max[i] = d_max[i] - d_to[i];
      dist_to_min[i] = d_min[i] + d_to[i];

      S_R_DEBUG_DYN(C_xstr("%: d_from: % d_to: % dfmax: % dfmin: % dtmax: % dtmin: %")
         %i
         %d_from[i]
         %d_to[i]
         %dist_from_max[i]
         %dist_from_min[i]
         %dist_to_max[i]
         %dist_to_min[i]
         );

      if((dist_from_max[i] >= th.radius && dist_to_max[i] >= th.radius) ||
         (dist_from_min[i] >= th.radius && dist_to_min[i] >= th.radius)){

         S_R_DEBUG_DYN(C_fstr("out of axis %i - exit", i));
         return;
      }
   }

   bool crossing_z = (dist_from_min[2]*dist_to_min[2])<0.0f;

   int col_edge_count=0;
   bool max_side[3];
   int coll_axis[2];

   max_side[2] = (d_max[2] > d_from[2]);
   bool z_max_dir = (d_to[2] < d_from[2]);

   S_R_DEBUG_DYN(C_fstr("crossing_z = %i z_max_dir = %i", crossing_z, z_max_dir));

   for(i=0; i<2; i++){

      if(crossing_z){
                              //get point on plane
         S_vector pp;
         u = normal[2].Dot(th.dir);
         if(!IsAbsMrgZero(u)){
            u = - (normal[2].Dot(th.from) + d_max[2]) / u;
            pp = th.from + th.dir * u;
            S_R_DEBUG_DYN(C_fstr("getting point on plane at dist. %.2f", u));
         }else{
            S_R_DEBUG_DYN("dir parallel with plane");
         }

         float d = -normal[i].Dot(pp);
         float p_dist_from_max = d_max[i] - d;
         float p_dist_from_min = d_min[i] + d;

         if(p_dist_from_max>0.0f){
                                 //out of rect
                                 //test edge
            coll_axis[col_edge_count++] = i;
            max_side[i] = true;
         }else
         if(p_dist_from_min>0.0f){
            coll_axis[col_edge_count++] = i;
            max_side[i] = false;
         }
      }else{
                              //now it depends on from we are coming
         if(dist_from_min[i] >= 0.0f){
            if(z_max_dir == max_side[2]){
                              //hit closest edge
               if(dist_from_min[i]>0.0f){
                  coll_axis[col_edge_count++] = i;
                  max_side[i] = false;
               }else
               if(dist_from_max[i]>0.0f){
                  coll_axis[col_edge_count++] = i;
                  max_side[i] = true;
               }
            }else
                              //going from min side
            if(dist_to_min[i]>0.0f){
               coll_axis[col_edge_count++] = i;
               max_side[i] = false;                            
            }else
            if(dist_to_max[i]>0.0f){
               coll_axis[col_edge_count++] = i;
               max_side[i] = true;
            }
         }else
         if(dist_from_max[i] >= 0.0f){
            if(z_max_dir == max_side[2]){
                              //hit closest edge
               if(dist_from_max[i]>0.0f){
                  coll_axis[col_edge_count++] = i;
                  max_side[i] = true;
               }else
               if(dist_from_min[i]>0.0f){
                  coll_axis[col_edge_count++] = i;
                  max_side[i] = false;
               }
            }else
                              //going from max side
            if(dist_to_max[i]>0.0f){
               coll_axis[col_edge_count++] = i;
               max_side[i] = true;
            }else
            if(dist_to_min[i]>0.0f){
               coll_axis[col_edge_count++] = i;
               max_side[i] = false;
            }
         }else{
            if(z_max_dir == max_side[2]) continue;

            if(dist_from_max[i] > dist_to_max[i]){
               if(dist_to_min[i]>0.0f){
                  coll_axis[col_edge_count++] = i;
                  max_side[i] = false;
               }
            }else{
               if(dist_to_max[i]>0.0f){
                  coll_axis[col_edge_count++] = i;
                  max_side[i] = true;
               }
            }
         }
      }
   }

   coll_axis[1] = 1-coll_axis[0];   //for case it's not filled

   S_R_DEBUG_DYN(C_fstr("col_edge_count = %i max_side = %i %i",
      col_edge_count, max_side[0], max_side[1]));
   S_R_DEBUG_DYN(C_fstr("sorted axes: %i %i", coll_axis[0], coll_axis[1]));

   if(col_edge_count){

      S_vector coll_pt(bbox.min);
      S_vector col_normal;
      float u1, u2;
      float dist;
      float col_d;
      float d_to1;
      float d_in, d_out;
      //S_vector shared_point;

      d_in = .0f;
      d_out = th.dir_magn;

      --col_edge_count;
      if(!col_edge_count){
                              //pure edge collision
         if(i=coll_axis[0], max_side[i])
            coll_pt += world_side[i];

         bool b = LineIntersection(coll_pt, world_side[coll_axis[1]], th.from, th.dir, u1, u2);
         if(!b)
            return;

         S_R_DEBUG_DYN(C_fstr("solving edge collision: line/line col. at %.2f, %.2f", u1, u2));
         if(u2 <= 0.0f)
            return;

         if(u2<1.0f) 
            col_normal = th.from + th.dir*u2;
         else{
            col_normal = th.to;
                              //recompute closest point on edge
            u1 = world_side[coll_axis[1]].Dot(col_normal-coll_pt) / world_side_dot[coll_axis[1]];
            S_R_DEBUG_DYN(u1);
         }
         coll_pt += world_side[coll_axis[1]]*u1;
      }else{
                              //corner collision (probably)
                              //check both edges
         S_vector coll_pt1 = coll_pt;
         if(i=coll_axis[0], max_side[i])
            coll_pt += world_side[i];
         if(i=coll_axis[1], max_side[i])
            coll_pt1 += world_side[i];

         bool out1, out2;

         out1 = !LineIntersection(coll_pt, world_side[coll_axis[1]], th.from, th.dir, u1, u2);

         float u1a, u2a;
         out2 = !LineIntersection(coll_pt1, world_side[coll_axis[0]], th.from, th.dir, u1a, u2a);

                              //check which edge is out
         if(!out1) out1 = (u1<0.0f || u1>1.0f);
         if(!out2) out2 = (u1a<0.0f || u1a>1.0f);
         if(out1 && out2){
            //if(IsMrgZeroLess(th.dir_squared)) return;
                              //corner
            if(i=coll_axis[1], max_side[i])
               coll_pt += world_side[i];
                              //get dist to the line
            u2 = (coll_pt-th.from).Dot(th.dir) * th.r_dir_squared;

            if(u2 <= 0.0f)
               return;

            S_R_DEBUG_DYN(C_fstr("solving corner collision: line/pt dist at %.2f", u2));
            u2 = Min(1.0f, u2);
            col_normal = th.from + th.dir*u2;
         }else{
                              //pure edge
            if(out1){
                              //swap edges - shouldn't happen in ideal conditions
               u1=u1a;
               u2=u2a;
               coll_pt=coll_pt1;
               swap(coll_axis[1], coll_axis[0]);
            }
            if(u2 <= 0.0f)
               return;
            coll_pt += world_side[coll_axis[1]]*u1;
            if(u2 < 1.0f)
               col_normal = th.from + th.dir*u2;
            else
               col_normal = th.to;
         }
      }

      S_vector col_point_on_line = col_normal;
      col_normal -= coll_pt;
      dist = col_normal.Square();
      if(dist >= (th.radius*th.radius)){
         S_R_DEBUG_DYN("too far, exiting");
         return;
      }
      dist = I3DSqrt(dist);
      if(dist<MRG_ZERO) goto edge_close;
                              //normalize collision normal
      col_normal /= dist;

      col_d = - col_normal.Dot(coll_pt);
      d_to1 = - col_normal.Dot(th.to);
      col_d -= th.radius;

      if(u2<1.0f){
         float delta = d_to1-col_d;
         delta /= u2;
         if(delta > th.dir_magn){
            S_R_DEBUG_DYN("delta truncated");
            delta = th.dir_magn;
         }
         d_to1 = col_d + delta*1.05f;
      }
                              //collision response
      {
         th.collision_list.push_back(S_collision_info());
         S_collision_info &ci = th.collision_list.back();
         ci.plane.normal = col_normal;
         ci.plane.d = col_d;
         ci.hit_frm = this;
         ci.vol_source = th.vol_source;
                              //dynamic volumes has not plane in bsp tree, assign one negative
         ci.plane_id = (dword)-1;
                              //compute in distace:
         ci.hit_distance = .0f;
         ClipLineByBox(th, ci.hit_distance);
      }
      goto exit_true;
   }
edge_close:
   if(!crossing_z){
      if(z_max_dir == max_side[2]) 
         return;
   }

   S_R_DEBUG_DYN("solving solid z-side collision");
   {
      th.collision_list.push_back(S_collision_info());
      S_collision_info &ci = th.collision_list.back();
      ci.plane.normal = max_side[2] ? normal[2] : -normal[2];
      ci.plane.d = max_side[2] ? d_max[2] - th.radius : d_min[2] - th.radius;
      ci.hit_frm = this;
      ci.vol_source = th.vol_source;
                           //dynamic volumes has not plane in bsp tree, assign one negative
      ci.plane_id = (dword)-1;
                           //compute in distace(slow and unaccurate):
      ci.hit_distance = .0f;
      ClipLineByBox(th, ci.hit_distance);
   }

exit_true:
   vol_flags |= VOLF_DEBUG_TRIGGER;
}

//----------------------------
//----------------------------
