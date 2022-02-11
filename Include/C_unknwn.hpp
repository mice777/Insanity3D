#ifndef __C_UNKNWN_H
#define __C_UNKNWN_H

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// Unknown interface - keeping object reference count
//----------------------------

class C_unknown{
   unsigned long ref;
protected:
   virtual ~C_unknown(){};
public:
   inline C_unknown(): ref(1) {}
   virtual unsigned long AddRef(){ return ++ref; }
   virtual unsigned long Release(){
      if(--ref) return ref;
      delete this;
      return 0;
   }
};
typedef C_unknown *PC_unknown;

//----------------------------

#endif
