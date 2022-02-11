#ifndef __C_STACK_HPP
#define __C_STACK_HPP

#include <rules.h>
#include <malloc.h>

typedef dword size_type;

template<class T, class Container>
class C_stack{
   Container c;
public:
   C_stack<T, Container>& operator =(const C_stack<T, Container>&x){
      c = x.c;
      return *this;
   }
   inline bool empty() const{ return !c.size(); }
   inline void pop(){ c.pop_back(); }
   inline void push(const T& t){ c.push_back(t); }
   inline size_type size() const{ return c.size(); }
   inline T& top(){ return c.back(); }
   inline const T& top() const{ return c.back(); }
   inline void reserve(size_type s){ c.reserve(s); }
   inline T &operator [](size_type n){ return c[c.size()-1+n]; }
   inline void clear(){ c.clear(); }
};

#endif
