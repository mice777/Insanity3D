//----------------------------

void MakeStrips(const I3D_triface *faces, dword num_faces, C_vector<word> &out_strips, word *face_remap = NULL);

//----------------------------

                              //class used for changing strips to regular triangles
class C_destripifier{
   const word *pool;
   I3D_triface fc;
   dword strip_n;
public:
   void Begin(const word *indx_beg){
      pool = indx_beg;
      strip_n = 0;

      fc[1] = *pool++;
      fc[0] = *pool++;
   }
   const I3D_triface &Next(){
      swap(fc[strip_n], fc[strip_n+1]);
      strip_n ^= 1;
      fc[strip_n*2] = *pool++;
      return fc;
   }

   inline const I3D_triface &GetFace() const{ return fc; }
   inline bool IsDegenerate() const{
      return (fc[0]==fc[1] || fc[0]==fc[2] || fc[1]==fc[2]);
   }
};

//----------------------------
