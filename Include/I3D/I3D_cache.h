#ifndef __I3D_CACHE_H
#define __I3D_CACHE_H


#include <i3d\i3d2.h>
#include <smartptr.h>
#include <C_str.hpp>

#pragma warning(push,3)
#include <map>
#pragma warning(pop)

#include <direct.h>
#include <time.h>

using namespace std;

/************************************
  Copyright (c) Lonely Cat Games  All rights reserved.

  Created By: Michal Bacik
  Date: 11/22/99 2:02:57 PM
  Description:
   Template used to cache any Insanity 3D interface objects, which support
      AddRef(), Release()
 ************************************/

                              //cache interface
template <class T>
class C_I3D_cache_base{
public:
   virtual void SetCacheSize(dword sz) = 0;
   virtual I3D_RESULT Open(T *t1, const char *filename, PI3D_scene, dword open_flags = 0,
      void (*err_fnc)(const char *msg, void *context) = NULL, void *err_fnc_context = NULL) = 0;
   virtual I3D_RESULT Create(const char *filename, T **ret, PI3D_scene, void (*err_fnc)(const char *msg, void *context) = NULL, void *err_fnc_context = NULL) = 0;
   virtual T *CreateNew(PI3D_scene) const{ return NULL; }
   virtual void Clear() = 0;
   virtual ~C_I3D_cache_base(){};
   virtual void SetDir(const C_str&) = 0;
   virtual const C_str &GetDir() const = 0;
   virtual bool GetRelativeDirectory(char *fname) = 0;
   virtual int GetCacheSize() const{ return -1; }
   virtual int GetMemSize() const{ return -1; }
   virtual bool EraseItem(const char *filename){ return false; };
};

//----------------------------
                              //cache implementation

template<class T>             //this class must be in global scope, in order to avoid compiler warning
struct S_cache_item{
   C_smart_ptr<T> item;
   clock_t last_use_time;
   S_cache_item(){}
   S_cache_item(const S_cache_item &si){
      operator =(si);
   }
   S_cache_item &operator =(const S_cache_item &si){
      item = si.item;
      last_use_time = si.last_use_time;
      return (*this);
   };
};
class C_str_less: public less<C_str>{};


template <class T>
class C_I3D_cache: public C_I3D_cache_base<T>{
                              //dummy class, just to avoid compiler warning 
   class C_al: public std::allocator<S_cache_item<T> >{};
                              //mapping of filename to item data
   typedef map<C_str, S_cache_item<T>, C_str_less> t_cache;
   t_cache cache;
   C_str dir;                 //dir in which we search for file
   dword cache_size;          //max. number of items kept in cache

   struct S_err_call{
      void(*err_fnc)(const char*, void*);
      void *err_fnc_context;
   };
   static bool I3DAPI cbErr(I3D_LOADMESSAGE msg, dword prm1, dword prm2, void *context){
      if(msg==CBM_ERROR){
         S_err_call *ec = (S_err_call*)context;
         (*ec->err_fnc)((const char*)prm1, ec->err_fnc_context);
      }
      return false;
   }

//----------------------------

   void ReleaseOldestItem(clock_t curr_time){
                              //try to release oldest item
      clock_t max_time = 0;
      t_cache::iterator max_it = cache.end();
      for(t_cache::iterator it=cache.begin(); it!=cache.end(); ++it){
         S_cache_item<T> &itm = (*it).second;
         clock_t delta = curr_time - itm.last_use_time;
         if(max_time < delta){
            max_time = delta;
            max_it = it;
         }
      }
      if(max_it!=cache.end())
         cache.erase(max_it);
   }
public:
   C_I3D_cache():
      cache_size(128)
   {}

//----------------------------

   ~C_I3D_cache(){
      Clear();
   }

//----------------------------

   virtual void SetCacheSize(dword sz){

      if(cache.size() > sz){
                              //cache size is greater than requested size, remove oldest items
         clock_t curr_time = clock();

                              //get sorted list of items by age
         typedef multimap<dword, C_str> t_items;
         t_items items;

         t_cache::iterator it;
         for(it=cache.begin(); it!=cache.end(); ++it){
            clock_t age = curr_time - it->second.last_use_time;
            items.insert(t_items::value_type(age, it->first));
         }
         for(t_items::iterator it1 = items.end(); it1!=items.begin() && cache.size() > sz; ){
            --it1;
            const C_str &n = it1->second;
            it = cache.find(n);
            assert(it!=cache.end());
            cache.erase(it);
         }
      }
      cache_size = sz;
   }

//----------------------------

   virtual void Duplicate(T *dst, T *src) = 0;

//----------------------------

   virtual I3D_RESULT OpenFile(T *t, const char *dir, const char *filename, dword flags,
      I3D_LOAD_CB_PROC *cb_proc, void *cb_context){

      if(strchr(filename, '/')){
         if(cb_proc && (flags&I3DLOAD_ERRORS))
            cb_proc(CBM_ERROR, (dword)(const char*)C_fstr("Invalid character in filename '%s'", filename), 0, cb_context);
         return I3DERR_NOFILE;
      }
      return t->Open(C_fstr("%s%s", dir, filename), flags, cb_proc, cb_context);
   }

//----------------------------

   virtual I3D_RESULT Open(T *t1, const char *filename, PI3D_scene scn,
      dword open_flags = 0, void (*err_fnc)(const char*, void*) = NULL, void *err_fnc_context = NULL){

      C_str str_fname(filename);
      str_fname.ToLower();
      clock_t curr_time = clock();

      t_cache::iterator it = cache.find(str_fname);
      if(it!=cache.end()){
         S_cache_item<T> &itm = (*it).second;
         assert(t1 != itm.item);
                              //reuse the item (duplicate)
         Duplicate(t1, itm.item);
         itm.last_use_time = curr_time;
         return I3D_OK;
      }
      I3D_RESULT ir = I3DERR_NOFILE;   //used if no dir is registered

      S_err_call err_call = {err_fnc, err_fnc_context};

                              //always build sectors if model is loaded
      if(err_fnc) open_flags |= I3DLOAD_ERRORS;

                              //keep unique item in cache, so that it's not affected by modifications
      T *tu = CreateNew(scn);
      assert(tu);
      do{
         ir = OpenFile(tu, dir, filename, open_flags, cbErr, &err_call);
         switch(ir){
         case I3DERR_NOFILE: continue;
         case I3DERR_OUTOFMEM: 
         case I3DERR_FILECORRUPTED:
            tu->Release();
            return ir;
         }
         if(I3D_SUCCESS(ir)){
                              //put into cache
            if(cache.size()>=cache_size)
               ReleaseOldestItem(curr_time);

            it = cache.insert(pair<C_str, S_cache_item<T> >(str_fname, S_cache_item<T>())).first;
            S_cache_item<T> &itm = (*it).second;
            
            Duplicate(t1, tu);
            itm.item = tu;
            itm.last_use_time = curr_time;
            tu->Release();
            return ir;
         }
      }while(false);
                              //failed, but duplicate anyway, so that opened file is effectively closed
      Duplicate(t1, tu);
      tu->Release();
      if(err_fnc)             //report error
         err_fnc(C_fstr("Failed to open file: '%s'", filename), err_fnc_context);
      return ir;
   }

//----------------------------

   virtual I3D_RESULT Create(const char *filename, T **ret, PI3D_scene scn,
      void (*err_fnc)(const char*, void*) = NULL, void *err_fnc_context = NULL){

      *ret = NULL;

      C_str str_fname(filename);
      str_fname.ToLower();
      clock_t curr_time = clock();
                              //look into cache first
      t_cache::iterator it = cache.find(str_fname);
      if(it!=cache.end()){
         S_cache_item<T> &itm = (*it).second;
                              //reuse the item (add ref)
         itm.item->AddRef();
         *ret = itm.item;
         itm.last_use_time = curr_time;
         return I3D_OK;
      }
      I3D_RESULT ir = I3DERR_NOFILE;   //used if no dir is registered

      T *tp = CreateNew(scn);
      if(!tp) return NULL;
      S_err_call err_call = {err_fnc, err_fnc_context};

      do{
         ir = OpenFile(tp, dir, filename, err_fnc ? I3DLOAD_ERRORS : 0, cbErr, &err_call);
         if(I3D_FAIL(ir)){
            if(ir==I3DERR_NOFILE)
               continue;
            break;
         }
                              //put into cache
         if(cache.size()>=cache_size)
            ReleaseOldestItem(curr_time);

         it = cache.insert(pair<C_str, S_cache_item<T> >(str_fname, S_cache_item<T>())).first;
         S_cache_item<T> &itm = (*it).second;

         itm.item = tp;
         itm.last_use_time = curr_time;
         *ret = tp;
         return ir;
      }while(false);
      tp->Release();
      if(err_fnc)             //report error
         err_fnc(C_fstr("Failed to load file: '%s'", filename), err_fnc_context);
      return ir;
   }

//----------------------------

   virtual void Clear(){
      cache.clear();
   }

//----------------------------

   virtual int GetCacheSize() const{ return cache.size(); }

//----------------------------
                              //directory functions
   virtual void SetDir(const C_str &d){
      dir = d;
      if(dir.Size() && dir[dir.Size()-1] != '\\') dir += "\\";
   }
   virtual const C_str &GetDir() const{ return dir; }


//----------------------------
                              //check is full-path filename 'fname' is relative of
                              //any registered directories
                              //if match found, func returns true and fname is stripped
                              //by removing base directory

   bool GetRelativeDirectory(char *fn1){
                              //try to strip current working directory from fname
      char cd[260];
      _getcwd(cd, sizeof(cd));
      C_fstr wild_dir("%s\\*", cd);
      C_str fname(fn1);
      if(fname.Matchi(wild_dir))
         fname = &fname[wild_dir.Size()-1];

      {
         C_str wild_dir = dir + "*";
         if(fname.Matchi(wild_dir)){
            strcpy(fn1, &fname[dir.Size()]);
            return true;
         }
      }
      return false;
   }

//----------------------------

   virtual bool EraseItem(const char *filename){

      C_str str_fname(filename);
      str_fname.ToLower();
                              //look into cache first
      t_cache::iterator it = cache.find(str_fname);
      if(it!=cache.end()){
         cache.erase(it);
         return true;
      }
      return false;
   };
};

//----------------------------
                              //cache for loading models
class C_I3D_model_cache: public C_I3D_cache<I3D_model>{
   
   static void mod_err_fnc(const char *str, void *context){

      C_str &err_string = *(C_str*)context;
      err_string = str;
   }

   virtual void Duplicate(PI3D_model dst, PI3D_model src){
                              //we're not duplicating basic I3D_frame values
                              // thus we save it now and restore after duplication
      C_str save_name = dst->GetName();
      S_vector pos = dst->GetPos();
      S_quat rot = dst->GetRot();
      float scale = dst->GetScale();
      dword save_data = dst->GetData();
      dword frm_flags = dst->GetFlags();

      dst->Duplicate(src);
      dst->SetName(save_name);
      dst->SetPos(pos);
      dst->SetRot(rot);
      dst->SetScale(scale);
      dst->SetData(save_data);
      dst->SetFlags(frm_flags);
                              //zero user data of duplicated model's frames
      const PI3D_frame *mod_frms = dst->GetFrames();
      for(int i=dst->NumFrames(); i--; ){
         mod_frms[i]->SetData(0);
      }
   }
   virtual PI3D_model CreateNew(PI3D_scene scn) const{
      return I3DCAST_MODEL(scn->CreateFrame(FRAME_MODEL));
   }
public:

   virtual I3D_RESULT OpenFile(PI3D_model mod, const char *dir, const char *filename, dword flags,
      I3D_LOAD_CB_PROC *cb_proc, void *cb_context){

#ifdef _DEBUG                 //make sure filename doesn't contain extension
      for(int i=strlen(filename); i--; ){
         char c = filename[i];
         if(c=='\\')
            break;
         if(c=='.'){
            if(cb_proc && (flags&I3DLOAD_ERRORS))
               cb_proc(CBM_ERROR, (dword)(const char*)C_fstr("Invalid character in filename '%s'", filename), 0, cb_context);
            return I3DERR_NOFILE;
         }
      }
#endif
      if(strchr(filename, '/')){
         if(cb_proc && (flags&I3DLOAD_ERRORS))
            cb_proc(CBM_ERROR, (dword)(const char*)C_fstr("Invalid character in filename '%s'", filename), 0, cb_context);
         return I3DERR_NOFILE;
      }
                                 //load file with extension
      I3D_RESULT ir = C_I3D_cache<I3D_model>::OpenFile(mod, dir,
         C_fstr("%s.i3d", filename),
         flags | I3DLOAD_BUILD_SECTORS, cb_proc, cb_context);
      return ir;
   }
};

//----------------------------
                              //cache for loading sounds
class C_I3D_sound_cache: public C_I3D_cache<I3D_sound>{

   static void snd_err_fnc(const char *str, void *context){

      C_str &err_string = *(C_str*)context;
      err_string = str;
   }

   virtual void Duplicate(PI3D_sound dst, PI3D_sound src){
                              //duplication means letting all params except sound source
      dst->Open(src->GetFileName(), src->GetOpenFlags());
   }
   virtual PI3D_sound CreateNew(PI3D_scene scn) const{
      return I3DCAST_SOUND(scn->CreateFrame(FRAME_SOUND));
   }
public:
   virtual I3D_RESULT Open(PI3D_sound t1, const char *filename, PI3D_scene scn,
      dword open_flags = 0, void (*err_fnc)(const char*, void*) = NULL, void *err_fnc_context = NULL){

      C_str err_string;
      I3D_RESULT ir;
#ifdef _DEBUG                 //make sure filename doesn't contain extension
      {
         for(int i=strlen(filename); i--; ){
            char c = filename[i];
            if(c=='\\')
               break;
            if(c=='.'){
               if(err_fnc)
                  err_fnc(C_fstr("Cannot specify extension in sound cache (file '%s')", filename), err_fnc_context);
               return I3DERR_NOFILE;
            }
         }
      }
#endif
                              //try 2 extensions
      ir = C_I3D_cache<I3D_sound>::Open(t1, C_fstr("%s.ogg", filename),
         scn, open_flags, err_fnc ? snd_err_fnc : NULL, &err_string);
      if(I3D_FAIL(ir)){
         ir = C_I3D_cache<I3D_sound>::Open(t1, C_fstr("%s.wav", filename),
            scn, open_flags, err_fnc ? snd_err_fnc : NULL, &err_string);
      }
      t1->StoreFileName(filename);
      if(I3D_FAIL(ir)){
         if(err_fnc && err_string.Size())
            err_fnc(err_string, err_fnc_context);
      }
      return ir;
   }
};

//----------------------------
                              //cache for loading animations
class C_I3D_anim_cache: public C_I3D_cache<I3D_animation_set>{
   virtual void Duplicate(PI3D_animation_set dst, PI3D_animation_set src){}
   virtual PI3D_animation_set CreateNew(PI3D_scene scn) const{
      return scn->GetDriver()->CreateAnimationSet();
   }
public:
   virtual I3D_RESULT Create(const char *filename, PI3D_animation_set *ret, PI3D_scene scn,
      void (*err_fnc)(const char*, void*) = NULL, void *err_fnc_context = NULL){

#ifdef _DEBUG                 //make sure filename doesn't contain extension
      for(int i=strlen(filename); i--; ){
         char c = filename[i];
         if(c=='\\')
            break;
         if(c=='.'){
            assert(err_fnc);
            err_fnc(C_fstr("Cannot specify extension in anim cache (file '%s')", filename), err_fnc_context);
            return I3DERR_NOFILE;
         }
      }
#endif
      return C_I3D_cache<I3D_animation_set>::Create(C_fstr("%s.i3d", filename), ret, scn, err_fnc, err_fnc_context);
   }
};

//----------------------------

#endif
