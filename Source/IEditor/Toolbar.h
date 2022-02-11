
//----------------------------
                              //slightly specialized version of general toolbar,
                              // adding some manipulation methods for access by IEditor
class C_toolbar_special: public C_toolbar{
public:
//----------------------------
// Get HWND of toolbar dialog.
   virtual void *GetHWND() = 0;

//----------------------------
// Resize toolbar to suggested size, specifying the client area of window.
// Note that this is only a hint, actual size is computed exactly, so that buttons fit in it properly.
   virtual void Resize(dword sx, dword sy) = 0;

//----------------------------
// Size dialog to have specified number of lines of buttons. This is a suggestion, which may not be possible to do.
   virtual void SetNumLines(dword) = 0;
};

//----------------------------

