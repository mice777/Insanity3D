#ifndef __FRAME_H
#define __FRAME_H

//----------------------------

class I3D_frame{
protected:
   dword ref;
   I3D_FRAME_TYPE type;
   mutable dword frm_flags;
public:
                              //hierarchy statistics
   word num_hr_vis, num_hr_occl, num_hr_sct, num_hr_jnt;
private:
                              //hierarchy
   PI3D_frame parent;
   C_vector<I3D_frame*> children;

public:
   C_str name;                //free to be read-accessed inside of engine

#define FRMFLAGS_USER_MASK          0x00fff     //bitmask of flags which may be modified by user (I3D_FRMF_???)

#define FRMFLAGS_INV_MAT_VALID      0x08000     //inverse matrix valid (inv_matrix)
#define FRMFLAGS_SUBMODEL           0x10000     //frame is child of model
#define FRMFLAGS_ON                 0x20000     //visibility/audibility
#define FRMFLAGS_G_MAT_VALID        0x40000     //help for visual objects - pivoted matrix valid
#define FRMFLAGS_CHANGE_MAT         0x80000     //matrix need updating
#define FRMFLAGS_ROT_QUAT_DIRTY     0x100000    //rotation quaternion needs to be recomputed if it's taken
#define FRMFLAGS_BSPHERE_TRANS_VALID 0x200000 //valid (transformed) bound sphere - visual only
#define FRMFLAGS_OCCLUDER_TRANS_OK  0x400000  //help for occluders - world-transformed verts valid, (cleared when matrix changed)
//#define FRMFLAGS_UPDATE_NEEDED      0x800000    //optimize wasted calls to Update()
#define FRMFLAGS_HR_BSPHERE_VALID   0x1000000   //valid (transformed) hierarchy bound sphere - visual only
#define FRMFLAGS_HR_BOUND_VALID     0x2000000   //help for frames with hierarchy bounds
#define FRMFLAGS_SM_BOUND_VALID     0x4000000   //help for single-mesh objects - local bound valid
#define FRMFLAGS_SHD_CASTER_NONVIS  0x10000000  //help during rendering - set when shadow caster is not visible
#define FRMFLAGS_HR_LIGHT_RESET     0x20000000  //hierarchically reseting light info
#define FRMFLAGS_DESTRUCTING        0x40000000  //set to true in destructor, marking that inherited classes are not valid

private:
   S_matrix m_rot;            //rotation - 3x3 part used

   mutable S_quat q_rot;      //rotation
   dword user_data;           //user data


   friend I3D_scene;         //for rendering and fast access
   friend I3D_driver;        //for creation
   friend class I3D_interpolator_PRS;
   friend class I3D_model_imp;
   friend class C_bound_volume;

   bool AddChild1(PI3D_frame frm);
   bool DeleteChild1(PI3D_frame frm);
protected:
   mutable S_matrix matrix;   //resulting world matrix = f_matrix * parnent's matrix
   mutable S_matrix f_matrix;         //resulting local matrix = scale * rot * transl.
   mutable S_matrix inv_matrix;  //inversed world matrix
   S_vector pos;              //translation
   float uniform_scale;
   PI3D_driver drv;

   dword enum_mask;           //mask for enumeration - one of ENUMF_??? flags

   I3D_frame(PI3D_driver);
   ~I3D_frame();

   void UpdateMatrixInternal() const;
protected:
   void UpdateMatrices() const{
                              //resulting matrix is
                              // - scaled rotation matrix
                              // - translated to object position
      f_matrix(0) = m_rot(0) * uniform_scale;
      f_matrix(1) = m_rot(1) * uniform_scale;
      f_matrix(2) = m_rot(2) * uniform_scale;

      f_matrix(3) = pos;
      UpdateMatrixInternal();
   }
public:

   inline const S_matrix &GetMatrixDirect() const{
      assert(!(frm_flags&FRMFLAGS_CHANGE_MAT));
      return matrix;
   }
   inline const S_matrix &GetLocalMatrix1() const{
      ChangedMatrix();
      return f_matrix;
   }
   inline const S_matrix *ChangedMatrix() const{    //NULL if not changed
      if(!(frm_flags&FRMFLAGS_CHANGE_MAT)) return NULL;
      UpdateMatrices();
      if(GetParent1()) matrix = f_matrix * GetParent1()->GetMatrix();
      else matrix = f_matrix;
      frm_flags &= ~(FRMFLAGS_CHANGE_MAT | FRMFLAGS_G_MAT_VALID | FRMFLAGS_INV_MAT_VALID |
         FRMFLAGS_BSPHERE_TRANS_VALID |
         FRMFLAGS_HR_BSPHERE_VALID |
         FRMFLAGS_OCCLUDER_TRANS_OK);
      return &matrix;
   }

   inline const S_matrix &GetInvMatrix1() const{
      if(!(frm_flags&FRMFLAGS_INV_MAT_VALID)){
         inv_matrix = ~GetMatrix();
         frm_flags |= FRMFLAGS_INV_MAT_VALID;
      }
      return inv_matrix;
   }

   void Update1();
   I3D_frame& operator =(const I3D_frame&);

   inline bool IsOn1() const{ return (frm_flags&FRMFLAGS_ON); }
   inline dword GetFrameFlags() const{ return frm_flags; }
   inline void SetFrameFlags(dword d){ frm_flags=d; }
   inline const C_vector<PI3D_frame> &GetChildren1() const{ return children; }

   inline I3D_FRAME_TYPE GetType1() const { return type; }
   inline dword GetEnumMask() const { return enum_mask; }
   inline dword NumChildren1() const{ return children.size(); }
   inline const C_str &GetName1() const{ return name; }
   inline PI3D_frame GetParent1() const{ return parent; }
   inline PI3D_driver GetDriver1() const{ return drv; }
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(I3D_FRAME_TYPE,GetType)() const { return type; }
                              //setup
   I3DMETHOD(SetPos)(const S_vector &v);
   I3DMETHOD(SetScale)(float);            
   I3DMETHOD(SetRot)(const S_quat&);
   I3DMETHOD(SetDir)(const S_vector&, float roll = 0.0f);
   I3DMETHOD(SetDir1)(const S_vector &dir, const S_vector &up);

   I3DMETHOD_(const S_vector&,GetPos)() const{ return pos; }
   I3DMETHOD_(const S_vector&,GetWorldPos)() const;
   I3DMETHOD_(float,GetScale)() const{ return uniform_scale; }
   I3DMETHOD_(float,GetWorldScale)() const;   
   I3DMETHOD_(const S_quat&,GetRot)() const;
   I3DMETHOD_(S_quat,GetWorldRot)() const;
   I3DMETHOD_(const S_vector&,GetDir)() const{ return m_rot(2); }
   I3DMETHOD_(const S_vector&,GetWorldDir)() const;

   I3DMETHOD_(const S_matrix&,GetLocalMatrix)() const{ return GetLocalMatrix1(); }
   I3DMETHOD_(const S_matrix&,GetMatrix)() const{
      ((PI3D_frame)this)->ChangedMatrix();        //provide proper matrix is computed
      return matrix;
   }
   I3DMETHOD_(const S_matrix&,GetInvMatrix)() const{ return GetInvMatrix1(); }
   //I3DMETHOD_(void,_Update)(){}
   I3DMETHOD_(void,SetData)(dword dw){ user_data = dw; }
   I3DMETHOD_(dword,GetData)() const{ return user_data; }

   I3DMETHOD_(void,SetOn)(bool);
   I3DMETHOD_(bool,IsOn)() const{ return IsOn1(); }

   I3DMETHOD_(void,SetName)(const C_str &n){ name = n; }
   I3DMETHOD_(const C_str&,GetName)() const{ return name; }
   I3DMETHOD_(const char*,GetOrigName)() const;
   I3DMETHOD_(void,ComputeHRBoundVolume)(PI3D_bound_volume) const;
                              //hierarchy
   I3DMETHOD_(PI3D_frame,GetParent)() const{ return parent; }
   I3DMETHOD(LinkTo)(PI3D_frame frm, dword flags = 0);
   I3DMETHOD_(dword,NumChildren)() const{ return children.size(); }
   //I3DMETHOD_(PI3D_frame,GetChild)(int indx) const{ return indx<(int)children.size() ? children[indx] : NULL; }
   I3DMETHOD_(const PI3D_frame*,GetChildren)() const{ return children.size() ? &children.front() : NULL; }

   I3DMETHOD(EnumFrames)(I3D_ENUMPROC*, dword user, dword flags, const char *mask = NULL) const;
   I3DMETHOD_(PI3D_frame,FindChildFrame)(const char *name, dword flags = ENUMF_ALL) const;

//----------------------------
// Get frame checksum, for purpose of BSP tree validation.
   I3DMETHOD_(void,GetChecksum)(float &matrix_sum, float &vertc_sum, dword &num_v) const;

//----------------------------
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD_(I3D_RESULT,DebugDraw)(PI3D_scene) const{ return I3DERR_UNSUPPORTED; }
                              //flags:
   I3DMETHOD_(dword,SetFlags)(dword new_flags, dword flags_mask){
      flags_mask &= FRMFLAGS_USER_MASK;
      new_flags &= flags_mask;
      frm_flags &= ~flags_mask;
      frm_flags |= new_flags;
      return frm_flags&FRMFLAGS_USER_MASK;
   }
   I3DMETHOD_(dword,GetFlags)() const{
      return frm_flags&FRMFLAGS_USER_MASK;
   }
//----------------------------
                              //non-user methods
//----------------------------
// Function called when matrix of frame is set dirty.
   virtual void SetMatrixDirty() const{
      frm_flags |= FRMFLAGS_CHANGE_MAT;
      frm_flags &= ~(FRMFLAGS_BSPHERE_TRANS_VALID | FRMFLAGS_HR_BSPHERE_VALID |
         FRMFLAGS_G_MAT_VALID | FRMFLAGS_INV_MAT_VALID |
         FRMFLAGS_OCCLUDER_TRANS_OK | FRMFLAGS_SM_BOUND_VALID);
   }
   virtual void PropagateDirty();
   //I3DMETHOD_(void,FrmReserved01)(){}
   I3DMETHOD_(void,FrmReserved02)(){}
   I3DMETHOD_(void,FrmReserved03)(){}
   I3DMETHOD_(void,FrmReserved04)(){}
   I3DMETHOD_(void,FrmReserved05)(){}
   I3DMETHOD_(void,FrmReserved06)(){}
   I3DMETHOD_(void,FrmReserved07)(){}
};

//----------------------------

class C_bound_volume{
public:
   I3D_bound_volume bound_local;
   I3D_bsphere bsphere_world;

                              //compute bounds from children's bounds
   void MakeHierarchyBounds(PI3D_frame);

   inline const I3D_bsphere &GetBoundSphereTrans(const I3D_frame *frm, dword valid_flags){
      if(!(frm->frm_flags&valid_flags)){
                              //recompute real (transformed) bsphere
         const S_matrix &m = frm->GetMatrix();
         float scale = I3DSqrt(m(0, 0)*m(0, 0) + m(1, 0)*m(1, 0) + m(2, 0)*m(2, 0));
         bsphere_world.radius = bound_local.bsphere.radius * scale;
         bsphere_world.pos = bound_local.bsphere.pos;
         bsphere_world.pos *= m;
         frm->frm_flags |= valid_flags;
      }
      return bsphere_world;
   }

   void DebugDrawBounds(PI3D_scene, PI3D_frame, dword valid_flags);
};

//----------------------------

#endif
