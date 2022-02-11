
/*
enum {
   USE_STOP,      // Zastavit a pockat
   USE_GO,      // Jit za herem
   USE_DISABLE
}use_state = USE_DISABLE;

bool CanBeUsed(){
   return (use_state != USE_DISABLE);
}
*/

table "Discharges"{
   string str[2] "Name"(15);
}

void LogS(string s);

void GameBegin(){
   LogS(str[3]);
   LogS(str[4]);
}

/*
void OnUse(){
   switch(0){
   case 0:
      //use_state = USE_DISABLE;
      //TalkTo("sp_city_Friend_Fear", "hero", true, "",COLOR_BELHER);  hero stop
      ResetAI;
      //Guard();
      //use_state = 1;
      //break;

   case 1:
      //use_state = USE_DISABLE;
      //TalkTo("sp_city_Friend_Fear", "hero", true, "",COLOR_BELHER);  hero go
      ResetAI;
      //Follow(subject: "hero", tolerance: 2);
      //use_state = 2;
      //break;
   }
}
*/

/*
void GameBegin(){
   ObserveActor(true);
   MoveTo("dest bridge", 1.8);
}


void OnSignal(int id){
   Follow(subject: "hero", tolerance: 2);
   use_state = USE_STOP;

}

void OnDestroy(){
   ObserveActor(false);
   SendSignal("detect end");
   SetObjective(TASK_0, TS_FAILED);
}
*/

/*
#include "test.h"

//----------------------------
void PrintI(int);
float Sin(float);
void PrintF(float);

/**/

/*
enum E1{
   A1 = 1,
   A2,
   A3,
};

//typedef int t_timer(int, float);

const int alpha = 4;
float beta = alpha;

char c = 'A';
byte b = 45;
short s = -1200;
word w = 1244;
int i = -203334455;
dword dw = 0x12345678;

string b_variable = "Junaci sme najlepsi\n";
int SinInternal(float f){
   return Sin(f) * 180;
}
*/

/*
struct str1{
   int power;
   float flash;
   E epsi;
};
extern str1 sc;
/**/

/*
str1 sc = {
   543,
   4.3,
   C
};
*/

/*
struct str2{
   float ff;
   int dddd;
};
/**/

/*
table "Abc"{
   //float tf "Test float"(1, .5, 10, "Abcd");
   string tf "Test var"(20, "Help");

   branch "Branch"("Branch Help"){
      bool d "d"(1, "Bool help");
      branch "Inside"{
         float b "b";
         float c "c";
      }
      float is_fool "Fool"(3);
   }
}
/**/

/*
int Foo(string sss){
   PrintI(i);
   PrintF(f);
   //PrintS(sss);
   return 2;
}
*/

//bool PlaySound(string filename, float min_dist, float max_dist, string where = "", float volume = 1.0);


//extern int test_var;
//void PrintS(string s);

//string sstr = "Hello";

//float i = -1;


/*
void main(){
   Func(ii1: 2);
   //int i = 2;
}

/**/

/*

str1 a_variable[2] = {
   7, 12.45, "Fero",
   256, 2.68, "Gee"
};

typedef unsigned short word;
typedef unsigned long dword;
*/

//----------------------------