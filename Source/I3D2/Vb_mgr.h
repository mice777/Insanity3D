#ifndef __RB_MGR_H
#define __RB_MGR_H

#include <set>
#include <list>

//----------------------------

                              //default size of huge RB (in bytes)
#define RB_MANAGER_MEM_SIZE 0x20000

                              //debugging:
#ifdef GL
#define DEBUG_DISABLE_MULTIPLE      //single RB in each cache
#endif

#ifdef _DEBUG
//#define DEBUG_VB_ALLOC        //assign alloc id for each vb, to help find which was not freed
#endif


//----------------------------
// Huge D3D resource buffer - used for allocation of buffer arreas.
// Performs allocation management.
template<class T>
class C_RB_huge{
   C_smart_ptr<T> res_buf_8;  //actual huge resource buffer.

public:
//----------------------------
// Resource blocks - allocation manager, keeping single block.
   struct S_RB_block{
      enum E_status{
         S_FREE,
         S_ALLOC,
      };
      E_status status;        //block status
      dword index;            //block begin, in items
      dword size;             //block size
      S_RB_block *prev, *next;

      S_RB_block(){}
      S_RB_block(E_status st, dword indx1, dword sz,
         S_RB_block *p1, S_RB_block *n1):

         prev(p1), next(n1),
         index(indx1),
         status(st),
         size(sz)
      { }
   };

private:
//----------------------------

   template<class T>
   struct S_RB_block_less{
      bool operator ()(const T &bl1, const T &bl2){
         return ((bl1->status==S_RB_block::S_FREE ? bl1->size : 0) < (bl2->status==S_RB_block::S_FREE ? bl2->size : 0));
      }
   };

                              //allocation manager
   multiset<S_RB_block*, S_RB_block_less<S_RB_block*> > alloc_map;
public:
   C_RB_huge(){}
   ~C_RB_huge(){
      if(alloc_map.size()==1)
         delete *alloc_map.begin();
      alloc_map.clear();
   }
   inline bool operator <(const C_RB_huge &bl) const{
      return (this < &bl);
   }
   inline bool operator ==(const C_RB_huge &bl) const{
      return (this == &bl);
   }
   inline T *GetBuf(){ return res_buf_8; }
   inline const T *GetBuf() const{ return res_buf_8; }
#ifdef DEBUG_VB_ALLOC

   void DebugOutput();
#endif

//----------------------------
// Initialize class - allocate RB, init allocation structures.
   bool Initialize(dword fvf_flags, D3DPOOL mem_pool, dword d3d_usage, PI3D_driver drv,
      dword min_num_items, dword size_of_item, bool allow_cache){

      dword alloc_num = Max(min_num_items, (RB_MANAGER_MEM_SIZE / size_of_item));
#ifdef DEBUG_DISABLE_MULTIPLE
      alloc_num = min_num_items;
#endif
      if(!allow_cache)
         alloc_num = min_num_items;
                              //prepare resource buffer
      HRESULT hr;
      if(!fvf_flags){
         hr = drv->GetDevice1()->CreateIndexBuffer(alloc_num*size_of_item, d3d_usage, D3DFMT_INDEX16, mem_pool, (IDirect3DIndexBuffer9**)&res_buf_8, NULL);
      }else{
                              //use fvf code in creation only for dest buffers (XYZRHW) (due to ProcessVertices)
         dword fvf_flags1 = (fvf_flags&D3DFVF_POSITION_MASK)==D3DFVF_XYZRHW ? fvf_flags : 0;
         hr = drv->GetDevice1()->CreateVertexBuffer(alloc_num*size_of_item, d3d_usage, fvf_flags1, mem_pool, (IDirect3DVertexBuffer9**)&res_buf_8, NULL);
      }
                              //this call is allowed to fail (unsufficient video memory)
      //CHECK_D3D_RESULT("Create???Buffer", hr);
      if(FAILED(hr)){
         res_buf_8 = NULL;
         return false;
      }
      alloc_map.insert(new S_RB_block(S_RB_block::S_FREE, 0, alloc_num, NULL, NULL));
      return true;
   }

//----------------------------
// Close - should finish with empty allocation map.
   void Close(){
      assert(alloc_map.size() <= 1);
      if(alloc_map.size()==1){
         delete *alloc_map.begin();
      }
      alloc_map.clear();
   }

//----------------------------
// Allocate resource buffer segment.
// Returns resource index, or -1 if allocation fails
   int Alloc(dword num_items){

      S_RB_block *best_block = NULL;
      multiset<S_RB_block*, S_RB_block_less<S_RB_block*> >::iterator it;
      for(it = alloc_map.end(); it-- != alloc_map.begin(); ){
         if((*it)->status != S_RB_block::S_FREE)
            break;
         if(num_items > (*it)->size)
            break;
         best_block = (*it);
      }

      if(!best_block)
         return -1;

      if(best_block->size != num_items){
         assert(best_block->size > num_items);
         S_RB_block *new_block = new S_RB_block(S_RB_block::S_FREE,
            best_block->index + num_items,
            best_block->size - num_items,
            best_block, best_block->next);

         best_block->size = num_items;
         best_block->status = S_RB_block::S_ALLOC;
         best_block->next = new_block;

         alloc_map.insert(new_block);
      }else{
         best_block->size = num_items;
         best_block->status = S_RB_block::S_ALLOC;
      }
      return best_block->index;
   }

//----------------------------
// Free resource buffer segment.
// Returns true if there're no more blocks allocated
   bool Free(dword item_index){

      multiset<S_RB_block*, S_RB_block_less<S_RB_block*> >::iterator it;
      for(it = alloc_map.begin(); it != alloc_map.end(); ++it)
         if((*it)->index == item_index)
            break;
      assert(it!=alloc_map.end());
      assert((*it)->status==S_RB_block::S_ALLOC);
      S_RB_block *block = (*it);

      assert(!block->prev || block->prev->next==block);
      assert(!block->next || block->next->prev==block);
                                 //link list, join what's possible
      bool del_ok = false;
      if(block->next){
         if(block->next->status==S_RB_block::S_FREE){
                                 //delete next block
            S_RB_block *new_next = block->next->next;
            block->size += block->next->size;
            {
               multiset<S_RB_block*, S_RB_block_less<S_RB_block*> >::iterator it1;
               for(it1 = alloc_map.begin(); it1 != alloc_map.end(); ++it1)
                  if((*it1) == block->next)
                     break;
               assert(it1 != alloc_map.end());
               alloc_map.erase(it1);
               delete block->next;
            }
            block->next = new_next;
            if(new_next)
               new_next->prev = block;
         }
      } 
      if(block->prev){
         if(block->prev->status == S_RB_block::S_FREE){
            block->prev->next = block->next;
            block->prev->size += block->size;
            if(block->next){
               block->next->prev = block->prev;
            }
            del_ok = true;
         }
      }
      block->status = S_RB_block::S_FREE;

      if(del_ok){
                                 //remove from list
         alloc_map.erase(it);
         delete block;
      }
      return (alloc_map.size()==1 && (*alloc_map.begin())->status==S_RB_block::S_FREE);
   }
};

//----------------------------
//----------------------------
                              //RB allocation manager - operates on list of C_RB_huge

template<class T>
class C_RB_manager: public C_unknown{
                              //saved RB creation flags

   dword fvf_flags;           //non-zero only for vertex buffers
   dword d3d_usage;           //combination of D3DUSAGE_??? flags
   D3DPOOL mem_pool;
   dword size_of_item;        //size of single element (e.g. sizeof(word) for index buffers)

   list<C_RB_huge<T> > vl_list;   //currently created buffers

#ifdef DEBUG_VB_ALLOC
   dword alloc_id;
#endif

                              //don't allow copy
   C_RB_manager &operator =(const C_RB_manager&);
public:

#ifdef DEBUG_VB_ALLOC
   void SetAllocID(dword id){ alloc_id = id; }
   dword GetAllocID()const{return alloc_id; }
#endif

//----------------------------

   C_RB_manager(D3DPOOL mem_pool1, dword d3d_usage1, dword soi, dword fvf_f = 0):
      fvf_flags(fvf_f),
      d3d_usage(d3d_usage1),
      mem_pool(mem_pool1),
      size_of_item(soi)
   {}

//----------------------------
// Allocate buffer area.
   bool Allocate(dword num_items, PI3D_driver drv, T **rb_ret, dword *beg_indx_ret, bool allow_cache = true){

      list<C_RB_huge<T> >::iterator it;
      int ret_i = -1;
                              //try find free space in existing buffers
      for(it=vl_list.begin(); it!=vl_list.end(); it++){
         ret_i = (*it).Alloc(num_items);
         if(ret_i!=-1){
            assert((ret_i+num_items) * size_of_item <= RB_MANAGER_MEM_SIZE);
            break;
         }
      }

      if(it==vl_list.end()){
                              //not enough space in existing buffers, make new
         vl_list.push_back(C_RB_huge<T>());
         bool b = vl_list.back().Initialize(fvf_flags, mem_pool, d3d_usage, drv, num_items, size_of_item, allow_cache);
         if(!b){
            *rb_ret = NULL;
            *beg_indx_ret = 0;
            vl_list.pop_back();
            return false;
         }
         it = vl_list.end(); --it;
         ret_i = (*it).Alloc(num_items);
         assert(ret_i!=-1);
      }

      *rb_ret = (*it).GetBuf();
      *beg_indx_ret = ret_i;
      return true;
   }

//----------------------------
// Free resource area, represented by resource buffer and base index.
   bool Free(T *rb, dword item_indx){

      list<C_RB_huge<T> >::iterator it;
                              //find RB in which it is allocated
      for(it=vl_list.begin(); it!=vl_list.end(); it++)
      if((*it).GetBuf()==rb){

         bool b = (*it).Free(item_indx);
         if(b){
                              //empty RB, erase it
            (*it).Close();
            vl_list.erase(it);
         }
         return true;
      }
      return false;
   }

   inline dword GetFVFlags() const{ return fvf_flags; }
   inline dword GetUsage() const{ return d3d_usage; }
   inline D3DPOOL GetPool() const{ return mem_pool; }
   inline dword GetSizeOfItem() const{ return size_of_item; }
   inline bool IsEmpty() const{ return (vl_list.size() == 0); }
   inline list<C_RB_huge<T> > &GetRBList(){ return vl_list; }
};

//----------------------------

#endif