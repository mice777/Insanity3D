#include "all.h"

//----------------------------

class C_edit_Properties_ed: public C_editor_item_Properties{
public:

//----------------------------
// Show specialized sheet associated with particular frame.
   virtual void ShowFrameSheet(I3D_FRAME_TYPE type) = 0;
};

//----------------------------