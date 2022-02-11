//----------------------------
// Script general header file, automatically included into all compiled scripts.
// Copyright (c) 2001 Lonely Cat Games
//----------------------------

const dword NULL = 0;

const dword I3DANIMOP_SET = 0;
const dword I3DANIMOP_ADD = 0x15;
const dword I3DANIMOP_BLEND = 0x2a;
const dword I3DANIMOP_DAMP_ADD = 0x3f;


//----------------------------

//----------------------------// predefined functions - prototypes here //----------------------------

//----------------------------
//~Main
//Callback function - called after script is loaded. Used to initialize internal variables.
//Consider using GameBegin instead of this function.
void Main();

//----------------------------
//~Exit
//Callback function - called before script is unloaded.
//Consider using GameEnd instead of this function.
void Exit();

//----------------------------
//~GameBegin
//Callback function - called once after mission is loaded, before game starts.
//All initialization work of script should be done here.
void GameBegin();

//----------------------------
//~GameEnd
//Callback function - called once before mission is freed, when game ends.
//All shutdown work of script should be done here.
void GameEnd();

//----------------------------
//~OnHit
//Callback function - called when flying object hits the scripted frame.
void OnHit(dword power);

//----------------------------
//~OnCollision
//Callback function - called when collision occured. Material must be set to "Callback" in order to make this work.
//Return value:
// true to enable object to behave as collision
// false to ignore collision and pass through
bool OnCollision();

//----------------------------
//~OnSignal
//Callback function - called when signal is being sent to the script.
//Parameter 'id' specifies signal identification number.
void OnSignal(int id);

//----------------------------
//~OnDestroy
//Callback function - function called on actor frame when actor is destroyed -
//killed or broken, or whatever destructed.
void OnDestroy();

//----------------------------
//~OnNote
//Callback function - called when animation played on frame, or its children
//encountered note key.
//Parameters:
// id ... note id (string converted to integer)
void OnNote(int);

//----------------------------
//~OnIdle
//Callback function - called when AI is idle - when there's no more commands
//in program buffer, and no script to run.
void OnIdle();

//----------------------------
//~OnUse
//Callback function - called when the scripted is being used by other actor.
void OnUse();



//----------------------------//functions which may be called by scripts //----------------------------

//----------------------------// math //----------------------------

//----------------------------
//~RandomF
//Get random number in range 0.0f to 1.0f
float RandomF();

//----------------------------
//~RandomI
//Get random number in range 0 ... 'max'-1.
//Parameters:
// max ... number until which random numbers are generated
int RandomI(int max);

//----------------------------
//~Pause
//Pause the script for specified time (in msec).
//Parameters:
// time ... time to pause, in milliseconds
void Pause(int time);



//----------------------------// animations //----------------------------

//----------------------------
//~SetAnimation
//Set animation in a stage, with given blending mode and power.
//Parameters:
// name ... name of animation to set
// stage ... stage where to set the animation
// blend_op ...
// scale ... animation scale
// blend ... blend factor
// speed ... speed multiplier
// loop ... flag specifying if looping is desired
void SetAnimation(string name, int stage = 0, int blend_op = 0, float scale = 1.0, float blend = 1.0,
   float speed = 1.0, bool loop = true);

//----------------------------
//~SetPose
//Set static pose. The syntax is similar to SetAnimation, but speed and loop is redundant,
// because only static pose is set.
//Parameters:
// name ... name of pose
// stage ... stage where to set the pose
// blend_op ...
// scale ... animation scale
// blend ... blend factor
void SetPose(string name, int stage = 0, int blend_op = 0, float scale = 1.0, float blend = 1.0);


//----------------------------
// Print debugging string into a log window.
void LogS(string);
void LogI(int);
void LogF(float);

//----------------------------
// Debugging call.
//void GAMEAPI Debug(int code, int time=0);

//----------------------------// frame-based //----------------------------

//----------------------------
//~SetObjectOn
//Set frame on or off.
//Parameters:
// on_off ... true = on, false = off
// what ... name of frame to set on or off ("" = scripted frame)
void SetObjectOn(bool on, string name = "");

//----------------------------
//~SetObjectsParent
//Set frame's parent.
//Parameters:
// who ... name of frame which parent we changing
// name ... name of frame which will be scritped frame's parent
void SetObjectsParent(string who, string name);

//----------------------------
//~LinkObject
//Link object to other object. The frame is linked to specified parent,
//and its position and rotation is set to zero (either immediatelly, or in given time).
//Parameters:
// who ... name of frame which will be re-linked
// name ... name of new frame's parent
// time ... time in which transition is made 
void LinkObject(string who, string name, dword time = 500);

//----------------------------
//~IsObjectOn
//Get on/off state of frame.
//Parameters:
// what ... name of frame we ask for ("" = scripted frame)
int IsObjectOn(string name = "");



//----------------------------// sounds //----------------------------

//----------------------------
//~PlaySoundAmbient
//Play ambient sound.
//Parameters:
// filename ... name of sound without extension
// volume ... volume in range 0.0 to 1.0
void PlaySoundAmbient(string filename, float volume = 1.0);

//----------------------------
//~PlaySound
//Play sound.
//Prameters:
// filename ... filename of sound without extension
// min_dist, max_dist ... minimal and maximal audible distances
// where ... position of frame where to play the sound, set to "" to use scripted frame
// volume ... volume in range 0.0 to 1.0
// variations ... true to use random variation (auto-detected, appending '_n' to filename, where
//    n is number from 0 to 9)
void PlaySound(string filename, float min_dist, float max_dist, string where = "",
   float volume = 1.0, bool variations = false);

//----------------------------
//~FadeVolume
//Fade sound's volume to specified value in given time.
//Parameters:
// time ... time how long fade will last
// volume ... destination volume
// what ... name of sound to fade (set to "" to use scripted frame)
void FadeVolume(int time, float volume, string what = "");

//----------------------------
//~FlashObject
//Flash specified object (and all its children) - add brightness for specified time.
//Parameters:
// attack ... time how long brightness will be increased
// stay ... time how long brightness will stay at maximum value
// decay ... time how long brightness will fade out to zero
// value ... maximum brightness value
// what ... name of object to flash (set to "" to use scripted frame)
void FlashObject(dword attack, dword stay = 0, dword decay, float value = 1, string what = "");

//----------------------------
//~SetBrightness
//Set brightness of specified object (and all its children) - lower brightness of non-abient lights.
//Parameters:
// value ... brightness value
// what ... name of object to flash (set to "" to use scripted frame)
void SetBrightness(float value, string what = "");

//----------------------------
//~SetAnimation
//Set animation in a stage, with given blending mode and power.
//Parameters:
// name ... name of animation to set
// stage ... stage where to set the animation
// blend_op ...
// scale ... animation scale
// blend ... blend factor
// speed ... speed multiplier
// loop ... flag specifying if looping is desired
void SetAnimation(string name, int stage = 0, int blend_op = 0, float scale = 1.0, float blend = 1.0,
   float speed = 1.0);


//----------------------------// game logic //----------------------------

//----------------------------
//~SendSignal
//Send signal to script of another scripted frame.
// 'frm_receiver' may contain wildmasks.
// The signal is send by calling script's OnSignal function, if applicable,
//  and calling actor's OnSignal function, if actor exists.
//Parameters:
// receiver ... name of frame where signal will be sent
// signal_id ... signal ID being sent
void SendSignal(string receiver, int signal_id = 0);

//----------------------------
