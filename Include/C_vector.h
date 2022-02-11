//----------------------------
// Multi-platform mobile library
// (c) Lonely Cat Games
//----------------------------

#ifndef __C_VECTOR_HPP
#define __C_VECTOR_HPP

#include <Rules.h>

// Implementation of C++ vector<> -like class, using thin template.

//----------------------------

class C_vector_any{
   enum{ VECTOR_SIZE_GROW = 512 };
   dword elem_size;

protected:
   dword res_size, used_size, grow_size; //measured in size of element
   byte *array;
   C_vector_any(int elem_size, int grow_size = 0);
   virtual ~C_vector_any();

   void _CopyVector(const C_vector_any &v);
   void _Clear();
   void *_End();
   void _Insert(dword dst_i, const void *src, dword num);
   void _PushBack(const void *val);
   void _PopBack();
   void _Reserve(dword s);
   void *_At(dword i);
   void _Erase(dword beg_i, dword num);
   void _Resize(dword n, const void *val);

   virtual void _Construct(void *ptr) = 0;
   virtual void _Destruct(void *ptr) = 0;
   virtual void _Copy(void *dst, const void *src) = 0;
public:
};

//----------------------------

template<class T>
class C_vector: public C_vector_any{
   virtual void _Construct(void *ptr){ new(ptr) T; }
   virtual void _Destruct(void *ptr){ ((T*)ptr)->~T(); }
   virtual void _Copy(void *dst, const void *src){ *(T*)dst = *(T*)src; }
public:

   inline C_vector(): C_vector_any(sizeof(T)){}
   //inline C_vector(int _grow_size): C_vector_any(sizeof(T), _grow_size){}
   inline C_vector(int n, const T &val = T()): C_vector_any(sizeof(T)){ resize(n, val); }
   inline C_vector(const C_vector<T> &v): C_vector_any(sizeof(T)){ operator =(v); }
   inline ~C_vector(){ _Clear(); }

   inline C_vector<T> &operator=(const C_vector<T>&v){ _CopyVector(v); return *this; }
                              //iterators
   typedef T *iterator;
   typedef const T *const_iterator;
   inline iterator begin(){ return (T*)array; }
   inline const_iterator begin() const{ return (const T*)array; }
   inline iterator end(){ return (iterator)_End(); }
   inline const_iterator end() const{ return (const_iterator)const_cast<C_vector<T>*>(this)->_End(); }

   inline T &operator[](int i){ return *(T*)_At(i); }
   inline const T &operator[](int i) const{ return *(const T*)const_cast<C_vector<T>*>(this)->_At(i); }

   inline T &front(){ return *(T*)array; }
   inline const T &front() const{ return *(const T*)array; }

   inline T &back(){ return end()[-1]; }
   inline const T &back() const{ return end()[-1]; }

   inline int capacity(){ return res_size; }
   inline void clear(){ _Clear(); }
   inline bool empty() const{ return !used_size; }
   inline void erase(iterator first, iterator last){ _Erase(first-begin(), last-first); }
   inline void erase(iterator pos){ _Erase(pos-begin(), 1); }
   inline void insert(iterator pos, const T &val){ _Insert(pos-begin(), &val, 1); }

   inline void insert(iterator pos, const_iterator first, const_iterator last){ _Insert(pos-begin(), first, last-first); }
   //int max_size() const{ return -1; }
   inline T &push_back(const T &val){ _PushBack(&val); return back(); }
   inline void pop_back(){ _PopBack(); }
   inline T &push_front(const T &val){ _Insert(0, &val, 1); return front(); }
   inline void pop_front(){ erase(begin()); }

   inline void reserve(int s){ _Reserve(s); }
   void resize(int n, const T &val = T()){ _Resize(n, &val); }
   void assign(int n, const T &val = T()){ clear(); resize(n, val); }
   void assign(const_iterator first, const_iterator last){ clear(); _Insert(0, first, last-first); }
   inline dword size() const{ return used_size; }
   inline void remove_index(dword i){ _Erase(i, 1); }
   inline T &insert_index(dword i, const T &val = T()){ _Insert(i, &val, 1); return operator[](i); }
};
//----------------------------

#endif
