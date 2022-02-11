#ifndef __TEXTURE_H
#define __TEXTURE_H

#include "common.h"


//----------------------------
enum E_TEXTURE_TYPE{
   TEXTURE_NULL,
   TEXTURE_2D,
   TEXTURE_CUBIC,
};

                              //Class: basic texture, providing pointer to D3D
                              // texture interface on request, managing the contents
                              // in a transparent manner.
                              //This class also specifies rendering mode (alpha),
                              // size of texture and unique sorting value.
class I3D_texture_base{

#define TXTF_CKEY_ALPHA    2     //colorkey info in (possibly) 1-bit alpha
#define TXTF_ALPHA         4     //alpha pixels
//#define TXTF_SELF_ALPHA    8     //used for ADD blending mode
#define TXTF_MIPMAP        0x10  //has mipmap levels

protected:
   PI3D_driver drv;
   mutable dword txt_flags;
   const dword sort_id;       //rendering sort identifier
   dword size_x, size_y;
   S_pixelformat pf;

public:   
   I3D_texture_base(PI3D_driver);

   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD_(const C_str&,GetFileName)(int index = 0) const{
      static const C_str s_null;
      return s_null;
   }
   I3DMETHOD_(dword,SizeX)() const{ return size_x; }
   I3DMETHOD_(dword,SizeY)() const{ return size_y; }
   I3DMETHOD_(PIImage,GetSysmemImage)(){ return NULL; }
   I3DMETHOD_(CPIImage,GetSysmemImage)() const{ return NULL; }
   I3DMETHOD_(PI3D_driver,GetDriver)(){ return drv; }
   I3DMETHOD_(CPI3D_driver,GetDriver)() const{ return drv; }
   I3DMETHOD_(bool,Reload)(){ return false; }
public:

//----------------------------
// Get pointer to D3D texture. Inherited classes may animate contents of texture or
// swap among multiple pointers in this call.
// It is legal to return NULL by this function (in case of internal failure), in which case
// nothing will be rendered.
   virtual IDirect3DBaseTexture9 *GetD3DTexture() const = 0;
   virtual E_TEXTURE_TYPE GetTextureType() const = 0;

//----------------------------
// Get RLE compression mask keeping colorkey info. Optional.
   virtual const byte *GetRLEMask() const{ return NULL; }

#ifdef GL
//----------------------------
   virtual dword GetGlId() const = 0;
#endif

//----------------------------
// Get pixel format in which texture is initialized.
   const S_pixelformat &GetPixelFormat() const{ return pf; }

//----------------------------
// Read access to internal data.
   inline dword SizeX1() const{ return size_x; }
   inline dword SizeY1() const{ return size_y; }

   inline void SetTxtFlags(dword dw){ txt_flags = dw; }
   inline dword GetTxtFlags() const{ return txt_flags; }

   inline dword GetSortID() const{ return sort_id; }
};

typedef I3D_texture_base *PI3D_texture_base;
typedef const I3D_texture_base *CPI3D_texture_base;

//----------------------------
                              //Class: classical static texture, keeping static bitmap.
                              //When dirty (TXTF_UPLOAD_NEEDED set), the sysmem copy is
                              // updated to vidmem copy, using dirty rectangles of
                              // D3D sysmem texture.
class I3D_texture: public I3D_texture_base{

#define TXTF_UPLOAD_NEEDED 0x10000  //need uploading (Manage)

#ifdef GL
   mutable unsigned int txt_id;
#endif

protected:
   dword ref;
   mutable C_smart_ptr<IImage> sysmem_image;   //static copy of texture data
   mutable C_smart_ptr<IImage> vidmem_image;   //vidmem image - placeholder for D3D texture
   mutable C_smart_ptr<IDirect3DBaseTexture9> d3d_tex;
   E_TEXTURE_TYPE t_type;

   dword img_open_flags;      //IMGOPEN_??? flags, used for creation of vidmem image
   mutable dword last_render_time;  //used for caching

                              //creation params, used when texture lost
   C_str file_names[2];       //pic_name, opt_name
   dword create_flags;

   I3D_texture(const I3D_texture&);
   const I3D_texture& operator =(const I3D_texture&);

public:
   I3D_texture(PI3D_driver);
   ~I3D_texture();

   inline void SetDirty() const{ txt_flags |= TXTF_UPLOAD_NEEDED; }
   inline bool IsCkeyAlpha() const{ return txt_flags&TXTF_CKEY_ALPHA; }

   enum E_manage_cmd{
      MANAGE_FREE,            //free of video memory, release resources
      MANAGE_UPLOAD,          //load to video memory
      MANAGE_UPLOAD_IF_FREE,  //load to video memory, if there's some space
      MANAGE_CREATE_ONLY,     //only create vidmem image, no upload
   };
   void Manage(E_manage_cmd cmd) const;
   I3D_RESULT Open(const struct I3D_CREATETEXTURE&, I3D_LOAD_CB_PROC*, void *cb_context, C_str *err_msg);
   bool Match(const I3D_CREATETEXTURE&);

   virtual IDirect3DBaseTexture9 *GetD3DTexture() const;
   virtual E_TEXTURE_TYPE GetTextureType() const{ return t_type; }
   virtual const byte *GetRLEMask() const{
      return (const byte*)sysmem_image->GetColorkeyInfo();
   }
#ifdef GL
   virtual dword GetGlId() const;
#endif
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(const C_str&,GetFileName)(int index = 0) const{
      if(index<0 || index>1)
         //return NULL;
         throw C_except("invalid index");
      //return file_names[index].Size() ? (const char*)file_names[index] : NULL;
      return file_names[index];
   }
   I3DMETHOD_(PIImage,GetSysmemImage)(){ SetDirty(); return sysmem_image; }
   I3DMETHOD_(CPIImage,GetSysmemImage)() const{ return sysmem_image; }

   I3DMETHOD_(bool,Reload)();
};

//----------------------------
                              //Class: animated texture sequence.
class I3D_animated_texture: public I3D_texture_base{
protected:
   dword ref;
   int anim_delay;            //in ms

                              //run-time:
   mutable dword last_render_time;
   mutable int anim_count;
   mutable int curr_phase;
   C_buffer<C_smart_ptr<I3D_texture> > texture_list;
public:
   I3D_animated_texture(PI3D_driver d1);

   /*
   inline void AddTexture(PI3D_texture tp){
      if(!texture_list.size())
         txt_flags = tp->GetTxtFlags();
      texture_list.push_back(tp);
   }
   */
   void SetTextures(PI3D_texture *txts, dword num){
      texture_list.assign(num);
      for(dword i=num; i--; )
         texture_list[i] = txts[i];
      SetTxtFlags(!num ? 0 : texture_list[0]->GetTxtFlags());
   }
   inline void SetAnimSpeed(int as){ anim_delay = as; }

   virtual IDirect3DBaseTexture9 *GetD3DTexture() const;
#ifdef GL
   virtual dword GetGlId() const{ return 0; }
#endif
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
   I3DMETHOD_(const C_str&,GetFileName)(int i = 0) const{
      static const C_str s_null;
      return texture_list.size() ? texture_list.front()->GetFileName(i) : s_null;
   }
   virtual E_TEXTURE_TYPE GetTextureType() const{
      return texture_list.size() ? texture_list.front()->GetTextureType() : TEXTURE_NULL;
   }
};

//----------------------------

class I3D_camera_texture: public I3D_texture_base{
   mutable C_smart_ptr<IImage> vidmem_image;   //vidmem image - placeholder for D3D texture
protected:
   dword ref;
public:
   I3D_camera_texture(PI3D_driver d1);
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
   virtual IDirect3DBaseTexture9 *GetD3DTexture() const;
   virtual E_TEXTURE_TYPE GetTextureType() const{ return TEXTURE_2D; }
#ifdef GL
   virtual dword GetGlId() const{ return 0; }
#endif
};

//----------------------------

#endif
