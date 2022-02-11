#include "all.h"
#include "stripifier.h"

//----------------------------

#define CACHE_SIZE 24
#define MAX_SAMPLES 10

#ifdef _DEBUG

//#define DEBUG_BUILD_FIRST_STRIP_ONLY

#endif

#define DEBUG_NO_CACHE_OPTIM

//----------------------------

class C_stripifier{
public:
   typedef C_vector<word> t_word_list;

//----------------------------

   class C_vertex_cache{
      C_buffer<word> buf;
      dword buf_ptr;
   public:
      C_vertex_cache(dword size):
         buf(size),
         buf_ptr(0)
      {
         memset(buf.begin(), 0xff, size*sizeof(word));
      }

   //----------------------------

      inline bool InCache(word entry) const{

#if 1
         bool rtn = false;
         dword buf_size = buf.size();
         const word *buf_ptr = buf.begin();
         __asm{
            push ecx

            mov ax, entry
            mov ecx, buf_size
            mov edi, buf_ptr
            repne scasw
            setz rtn

            pop ecx
         }
         return rtn;
#else
         for(dword i = buf.size(); i--; ){
            if(buf[i] == entry){
               return true;
            }
         }
         return false;
#endif
      }

   //----------------------------

      void AddEntry(word entry){

         buf[buf_ptr] = entry;
         if(++buf_ptr == buf.size())
            buf_ptr = 0;
         /*
                              //push everything right one
         for(int i = buf.size() - 2; i >= 0; i--)
            buf[i + 1] = buf[i];
         buf[0] = entry;
         */
      }
   };

//----------------------------

   class C_face_info: public I3D_triface{
   public:
      int strip_id;           //real strip Id
      int m_testStripId;      //strip Id in an experiment
      int experiment_id;      //in what experiment was it given an experiment Id?
      word face_index;        //original face index
      bool flipped;           //used during finalization

                              //vertex indices
      C_face_info(const I3D_triface &fc, word index):
         I3D_triface(fc),
         face_index(index),
         strip_id(-1),
         m_testStripId(-1),
         experiment_id(-1),
         flipped(false)
      {
      }
      C_face_info(){}
   };

   typedef C_vector<C_face_info*> t_face_list;

//----------------------------
// nice and dumb edge class that points knows its
// indices, the two faces, and the next edge using
// the lesser of the indices
   class C_edge_info: public I3D_edge{
   public:
      //dword ref;
                              //pointers to edge's face
      C_face_info *edge_face;
      C_edge_info *next;

      C_edge_info(word v0, word v1, C_face_info *ef):
                              //we will appear in 2 lists.  this is a good
                              // way to make sure we delete it the second time
                              // we hit it in the edge infos
         //ref(1),
         I3D_edge(v0, v1),
         edge_face(ef),
         next(NULL)
      {
      }
      C_edge_info(){}
   
      //void Release(){ if(!--ref) delete this; }
   };

   typedef C_buffer<C_edge_info*> t_edge_list;

//----------------------------
// This class is a quick summary of parameters used to begin a triangle strip.
// Some operations may want to create lists of such items, so they were
// pulled out into a class
   class C_strip_start_info{
   public:
      C_face_info *m_startFace;
      C_edge_info *m_startEdge;
      bool m_toV1;      

      C_strip_start_info(C_face_info *startFace, C_edge_info *startEdge, bool toV1 = true):
         m_startFace(startFace),
         m_startEdge(startEdge),
         m_toV1(toV1)
      {
      }
   };

//----------------------------
// This is a summary of a strip that has been built
   class C_strip_info{
   public:
      C_strip_start_info start_info;
      t_face_list faces;
      int strip_id;
      int experiment_id;
     
      bool visited;

   //----------------------------
   // A little information about the creation of the triangle strips.
      C_strip_info(const C_strip_start_info &si, int stripId, int experimentId = -1) :
         start_info(si),
         strip_id(stripId),
         experiment_id(experimentId),
         visited(false)
      {
      }
     
   //----------------------------
   // This is an experiment if the experiment id is >= 0.
      inline bool IsExperiment () const{ return experiment_id >= 0; }
     
   //----------------------------
   // Take the given forward and backward strips and combine them together.
      void Combine(const t_face_list &forward, const t_face_list &backward){
   
         faces.reserve(forward.size() + backward.size());
                              //add backward faces
         for(int i = backward.size(); i--; )
            faces.push_back(backward[i]);
   
         faces.insert(faces.end(), forward.begin(), forward.end());
      }
     
   //----------------------------
   // Returns true if the face is "unique", i.e. has a vertex which doesn't exist in the faceVec.
      static bool Unique(const t_face_list &faces, const C_face_info &fi){

         //bool bv0, bv1, bv2; //bools to indicate whether a vertex is in the faces or not
         //bv0 = bv1 = bv2 = false;

         for(dword i = 0; i < faces.size(); i++){
            const I3D_triface &fc = *faces[i];
            /*
            if(!bv0){
               if((fc[0] == fi[0]) || 
                  (fc[1] == fi[0]) ||
                  (fc[2] == fi[0]))
                  bv0 = true;
            }
            if(!bv1){
               if((fc[0] == fi[1]) || 
                  (fc[1] == fi[1]) ||
                  (fc[2] == fi[1]))
                  bv1 = true;
            }

            if(!bv2){
               if((fc[0] == fi[2]) || 
                  (fc[1] == fi[2]) ||
                  (fc[2] == fi[2]))
                  bv2 = true;
            }

                              //the face is not unique, all it's vertices exist in the face vector
            if(bv0 && bv1 && bv2)
               return false;
               */
            if(fc.ContainsIndex(fi[0]) &&
               fc.ContainsIndex(fi[1]) &&
               fc.ContainsIndex(fi[2]))
               return false;
         }
                              //if we get out here, it's unique
         return true;
      }
     
   //----------------------------
   // If either the faceInfo has a real strip index because it is
   // already assign to a committed strip OR it is assigned in an
   // experiment and the experiment index is the one we are building
   // for, then it is marked and unavailable
      inline bool IsMarked(const C_face_info &fi) const{
         return (fi.strip_id >= 0) || (IsExperiment() && fi.experiment_id == experiment_id);
      }

   //----------------------------
   // Mark the triangle as taken by this strip.
      inline void MarkTriangle(C_face_info &fi) const{

         assert(!IsMarked(fi));
         if(IsExperiment()){
            fi.experiment_id = experiment_id;
            fi.m_testStripId = strip_id;
         }else{
            assert(fi.strip_id == -1);
            fi.experiment_id = -1;
            fi.strip_id = strip_id;
         }
      }
     
   //----------------------------

      static word GetNextI(const I3D_triface &fc, word v0, word v1){

         word v = fc[0];
         if(v==v0 || v==v1){
            v = fc[1];
            if(v==v0 || v==v1){
               v = fc[2];
               assert(v!=v0 && v!=v1);
            }
         }
         return v;
      }

   //----------------------------
   // Builds a strip forward as far as we can go, then builds backwards, and joins the two lists.
      void Build(const t_edge_list &all_edges, const t_face_list &faceInfos){

                              //build forward... start with the initial face
         t_face_list forwardFaces, backwardFaces;
         forwardFaces.push_back(start_info.m_startFace);

         MarkTriangle(*start_info.m_startFace);
         const I3D_triface &fc = *start_info.m_startFace;
         const I3D_edge &strt_e = *start_info.m_startEdge;
   
         word v0 = strt_e[0];
         word v1 = strt_e[1];
         word v2 = fc[next_tri_indx[fc.FindIndex(v1)]];
         assert(v2!=v0);
   
                              //
                              // build the forward list
                              //
         bool cull_swap = false;
         word nv0 = v1;
         word nv1 = v2;
   
         while(true){
            C_face_info *nextFace = !cull_swap ? FindOtherFace(all_edges, nv0, nv1) : FindOtherFace(all_edges, nv1, nv0);
            if(!nextFace || IsMarked(*nextFace))
               break;
                              //this tests to see if a face is "unique", meaning that its vertices aren't already in the list
                              // so, strips which "wrap-around" are not allowed
            if(!Unique(forwardFaces, *nextFace))
               break;

                              //add this to the strip
            forwardFaces.push_back(nextFace);

            MarkTriangle(*nextFace);
      
                              //get next index
            nv0 = GetNextI((*nextFace), nv0, nv1);
            swap(nv0, nv1);

            cull_swap = !cull_swap;
         }

                              //
                              // build the strip backwards
                              //
         cull_swap = false;
         nv0 = v1;
         nv1 = v0;
         while(true){
            C_face_info *nextFace = cull_swap ? FindOtherFace(all_edges, nv0, nv1) : FindOtherFace(all_edges, nv1, nv0);
            if(!nextFace || IsMarked(*nextFace))
               break;
                              //this tests to see if a face is "unique", meaning that its vertices aren't already in the list
                              // so, strips which "wrap-around" are not allowed
            if(!Unique(forwardFaces, *nextFace) || !Unique(backwardFaces, *nextFace))
               break;

                              //add this to the strip
            backwardFaces.push_back(nextFace);
      
            MarkTriangle(*nextFace);
      
                              //get next index
            nv0 = GetNextI((*nextFace), nv0, nv1);
            swap(nv0, nv1);
      
            cull_swap = !cull_swap;
         }
   
                              //combine the forward and backwards stripification lists and put into our own face vector
         Combine(forwardFaces, backwardFaces);
      }
   };

   typedef C_vector<C_strip_info*> t_strip_list;

//----------------------------
private:   
   const I3D_triface *in_faces;
   const dword num_faces;
   const int cache_size;
                              //the number of times to run the experiments
   const dword num_samples;

                              //pool where all face info is stored (we use pointers into this)
   mutable C_buffer<C_face_info> tmp_face_info;
   mutable C_buffer<C_edge_info> tmp_edge_info;

//----------------------------
// Find the edge info for these two indices.
   static C_edge_info *FindEdgeInfo(const t_edge_list &all_edges, dword v0, dword v1){
   
      C_edge_info *infoIter = all_edges[v0];
      while(infoIter != NULL){
         if((*infoIter)[1] == v1)
            return infoIter;
         infoIter = infoIter->next;
      }
      return NULL;
   }

//----------------------------
// Find the other face sharing these vertices exactly like the edge info above.
   static C_face_info *FindOtherFace(const t_edge_list &all_edges, dword v0, dword v1){

      C_edge_info *edgeInfo = FindEdgeInfo(all_edges, v1, v0);
      if(!edgeInfo)
         return NULL;
      return edgeInfo->edge_face;
   }

//----------------------------
// A good reset point is one near other commited areas so that we know that when we've made the longest strips
// its because we're stripifying in the same general orientation.
   static C_face_info *FindGoodResetPoint(const t_face_list &all_faces, const t_edge_list &all_edges, float &mesh_jump, bool &first_time_reset_point){

                              //we hop into different areas of the mesh to try to get
                              //other large open spans done
      int numFaces = all_faces.size();
      int startPoint;
      if(first_time_reset_point){
                              //first time, find a face with few neighbors (look for an edge of the mesh)
         startPoint = FindStartPoint(all_faces, all_edges);
         first_time_reset_point = false;
         if(startPoint == -1)
            startPoint = (int)(((float) numFaces - 1) * mesh_jump);
      }else
         startPoint = (int)(((float) numFaces - 1) * mesh_jump);
   
      C_face_info *result = NULL;
      int i = startPoint;
      C_face_info *const *afp = &all_faces.front();
      do{
      
                              //if this guy isn't visited, try him
         if(afp[i]->strip_id < 0){
            result = afp[i];
            break;
         }
      
                              //update the index and clamp to 0-(numFaces-1)
         if(++i >= numFaces)
            i = 0;
      
      } while(i != startPoint);
   
                              //update the mesh_jump
      mesh_jump += 0.1f;
      if(mesh_jump > 1.0f)
         mesh_jump = .05f;
   
                              //return the best face we found
      return result;
   }
   
//----------------------------

   void FindAllStrips(t_strip_list &all_strips, const t_face_list &all_faces, const t_edge_list &all_edges) const{

                              //the experiments
      int experiment_id = 0;
      int strip_id = 0;

      float mesh_jump = 0.0f;
      bool first_time_reset_point = true;

      C_strip_info *experiments[MAX_SAMPLES * 3];
      //memset(experiments, 0, MAX_SAMPLES * 3 * sizeof(C_strip_info*));

      set<const C_face_info*> reset_points;
      bool done = false;
      while(!done){
                              //
                              // PHASE 1: Set up numSamples * numEdges experiments
                              //

                              //array of experiments vectors
         dword experiment_index = 0;
         reset_points.clear();
         for(dword i = 0; i < num_samples; i++){
         
                              //try to find another good reset point
                              //if there are none to be found, we are done
            C_face_info *next_face = FindGoodResetPoint(all_faces, all_edges, mesh_jump, first_time_reset_point);
            if(!next_face){
               done = true;
               break;
            }
                              //if we have already evaluated starting at this face in this slew
                              // of experiments, then skip going any further
            if(reset_points.find(next_face) != reset_points.end())
               continue;
         
                              //trying it now...
            reset_points.insert(next_face);
         
                              //otherwise, we shall now try experiments for starting on the 01, 12, and 20 edges
            assert(next_face->strip_id < 0);

            const I3D_triface &nfc = *next_face;
         
                              //build the strip off of this face's 0-1 edge
            C_edge_info *edge01 = FindEdgeInfo(all_edges, nfc[0], nfc[1]);
            experiments[experiment_index++] =
               new C_strip_info(C_strip_start_info(next_face, edge01), strip_id++, experiment_id++);
         
                              //build the strip off of this face's 1-2 edge
            C_edge_info *edge12 = FindEdgeInfo(all_edges, nfc[1], nfc[2]);
            experiments[experiment_index++] =
               new C_strip_info(C_strip_start_info(next_face, edge12), strip_id++, experiment_id++);
         
                              //build the strip off of this face's 2-0 edge
            C_edge_info *edge20 = FindEdgeInfo(all_edges, nfc[2], nfc[0]);
            experiments[experiment_index++] =
               new C_strip_info(C_strip_start_info(next_face, edge20), strip_id++, experiment_id++);
         }
         assert(experiment_index <= num_samples * 3);
         if(!experiment_index)
            break;
         
                              //
                              // PHASE 2: Iterate through that we setup in the last phase
                              // and really build each of the strips and strips that follow to see how
                              // far we get
                              //
         for(i=experiment_index; i--; )
            experiments[i]->Build(all_edges, all_faces);
      
                              //
                              // Phase 3: Find the experiment that has the most promise
                              //
         dword bestIndex = 0;
         int bestValue = 0;
         for(i = 0; i < experiment_index; i++){
            int value = experiments[i]->faces.size();
            if(bestValue < value){
               bestValue = value;
               bestIndex = i;
            }
         }
      
                              //
                              // Phase 4: commit the best experiment of the bunch
                              //
         CommitStrips(all_strips, experiments[bestIndex]);
      
                              //and destroy all of the others
         for(i=0; i<experiment_index; i++){
            if(i!=bestIndex)
               delete experiments[i];
         }
#ifdef DEBUG_BUILD_FIRST_STRIP_ONLY
         for(i=all_faces.size(); i--; ){
            C_face_info *fi = all_faces[i];
            if(fi->strip_id==-1){
               C_edge_info *e = FindEdgeInfo(all_edges, (*fi)[0], (*fi)[1]);
               C_strip_info *si = new C_strip_info(C_strip_start_info(fi, e), strip_id++, experiment_id++);
               si->faces.push_back(fi);
               all_strips.push_back(si);
            }
         }
         break;
#endif
      }
   }

//----------------------------

   static void SplitUpStripsAndOptimize(const t_strip_list &all_strips, t_strip_list &outStrips, t_edge_list &all_edges, dword cache_size){

      dword threshold = cache_size - 4;
      t_strip_list tempStrips;
   
      const C_strip_info *const *p_strips = &all_strips.front();
      dword num_strips = all_strips.size();
                                 //split up strips into threshold-sized pieces
      for(dword i = 0; i < num_strips; i++){
         C_strip_info *currentStrip;
         C_strip_start_info startInfo(NULL, NULL, false);
         const C_strip_info &si = *p_strips[i];
      
         if(si.faces.size() > threshold){
         
            int numTimes = si.faces.size() / threshold;
            int numLeftover = si.faces.size() % threshold;
         
            for(int j = 0; j < numTimes; j++){
               currentStrip = new C_strip_info(startInfo, 0, -1);
            
               for(dword faceCtr = j*threshold; faceCtr < threshold+(j*threshold); faceCtr++){
                  currentStrip->faces.push_back(si.faces[faceCtr]);
               }
            
               tempStrips.push_back(currentStrip);
            }
         
            int leftOff = j * threshold;
         
            if(numLeftover != 0){
               currentStrip = new C_strip_info(startInfo, 0, -1);   
            
               for(int k = 0; k < numLeftover; k++){
                  currentStrip->faces.push_back(si.faces[leftOff++]);
               }
            
               tempStrips.push_back(currentStrip);
            }
         }else{
            //we're not just doing a tempStrips.push_back(allBigStrips[i]) because
            // this way we can delete allBigStrips later to free the memory
            currentStrip = new C_strip_info(startInfo, 0, -1);
         
            for(dword j = 0; j < si.faces.size(); j++)
               currentStrip->faces.push_back(si.faces[j]);
         
            tempStrips.push_back(currentStrip);
         }
      }

                              //add small strips to face list
      //t_strip_list tempStrips2;
      //RemoveSmall_strips(tempStrips, tempStrips2, outFaceList);
   
      outStrips.clear();

      //dword num_tmp_strips = tempStrips2.size();
      //C_strip_info *const *p_tmp_strips = tempStrips2.begin();
      if(tempStrips.size() != 0){
         outStrips.reserve(all_strips.size());

         //const t_strip_list &tempStrips2 = tempStrips;
         t_strip_list tempStrips2 = tempStrips;
         C_strip_info *const *p_tmp_strips = &tempStrips2.front();

                                 //optimize for the vertex cache
         C_vertex_cache *vcache = new C_vertex_cache(cache_size);

         int first_index = 0;
         int min_neighbours = 1000000;
      
         for(i = tempStrips2.size(); i--; ){
            int num_neighbors = 0;
         
                              //find strip with least number of neighbors per face
            for(dword j = 0; j < p_tmp_strips[i]->faces.size(); j++)
               num_neighbors += NumNeighbors(*p_tmp_strips[i]->faces[j], all_edges);
         
            if(min_neighbours > num_neighbors){
               min_neighbours = num_neighbors;
               first_index = i;
            }
         }
      
         UpdateCacheStrip(vcache, *p_tmp_strips[first_index]);
         outStrips.push_back(p_tmp_strips[first_index]);
         //p_tmp_strips[first_index]->visited = true;
         tempStrips2[first_index] = tempStrips2.back(); tempStrips2.pop_back();
      
                              //this n^2 algo is what slows down stripification so much....
                              // needs to be improved
         while(tempStrips2.size()){
            int best_num_hits = -1;
            int best_index = -1;

            p_tmp_strips = &tempStrips2.front();
                              //find best strip to add next, given the current cache
            for(int i = tempStrips2.size(); i--; ){
               //if(p_tmp_strips[i]->visited)
                  //continue;
            
               int num_hits = CalcNumHitsStrip(vcache, *p_tmp_strips[i]);
               if(best_num_hits < num_hits){
                  best_num_hits = num_hits;
                  best_index = i;
               }
            }
            //if(best_num_hits == -1)
               //break;
            //p_tmp_strips[best_index]->visited = true;
            UpdateCacheStrip(vcache, *p_tmp_strips[best_index]);
            outStrips.push_back(p_tmp_strips[best_index]);
            tempStrips2[best_index] = tempStrips2.back(); tempStrips2.pop_back();
         }
         delete vcache; 
      }
   }

//----------------------------
// "Commits" the input strips by setting their experiment_id to -1 and adding to the allStrips vector
   static void CommitStrips(t_strip_list &all_strips, C_strip_info *strip){

                              //tell the strip that it is now real
      strip->experiment_id = -1;
   
                           //add to the list of real strips
      all_strips.push_back(strip);
   
                           //iterate through the faces of the strip
                           // tell the faces of the strip that they belong to a real strip now
      const t_face_list &faces = strip->faces;
      for(int j=faces.size(); j--; )
         strip->MarkTriangle(*faces[j]);
   }

//----------------------------
// Finds a good starting point, namely one which has only one neighbour.
// Returns index into 'all_faces'.
   static int FindStartPoint(const t_face_list &all_faces, const t_edge_list &all_edges){

      const C_face_info *const *afp = &all_faces.front();
      for(dword i = all_faces.size(); i--; ){
         int ctr = 0;
      
         const C_face_info &fi = *afp[i];
         if(!FindOtherFace(all_edges, fi[0], fi[1]))
            ++ctr;
         if(!FindOtherFace(all_edges, fi[1], fi[2]))
            ++ctr;
         if(!FindOtherFace(all_edges, fi[2], fi[0]))
            ++ctr;
         if(ctr > 1)
            return i;
      }
      return -1;
   }
   
//----------------------------
// Updates the input vertex cache with this strip's vertices.
   static void UpdateCacheStrip(C_vertex_cache *vcache, const C_strip_info &strip){

      dword num_faces = strip.faces.size();
      const C_face_info *const *p_s_faces = &strip.faces.front();
      for(dword i = 0; i < num_faces; i++){
         const I3D_triface &fc = *p_s_faces[i];
         if(!vcache->InCache(fc[0]))
            vcache->AddEntry(fc[0]);
      
         if(!vcache->InCache(fc[1]))
            vcache->AddEntry(fc[1]);
      
         if(!vcache->InCache(fc[2]))
            vcache->AddEntry(fc[2]);
      }
   }

//----------------------------
// Return the number of cache hits per face in the strip.
   static int CalcNumHitsStrip(const C_vertex_cache *vcache, const C_strip_info &strip){

      int num_hits = 0;
   
      dword num_faces = strip.faces.size();
      const C_face_info *const *p_s_faces = &strip.faces.front();
      for(dword i = 0; i < num_faces; i++){
         const I3D_triface &fc = *p_s_faces[i];
         if(vcache->InCache(fc[0]))
            ++num_hits;
      
         if(vcache->InCache(fc[1]))
            ++num_hits;
      
         if(vcache->InCache(fc[2]))
            ++num_hits;
      }
      return num_hits;
   }

//----------------------------
// Return the number of neighbors that this face has.
   static int NumNeighbors(const C_face_info &fi, const t_edge_list &all_edges){

      int numNeighbors = 0;
   
      if(FindOtherFace(all_edges, fi[0], fi[1]) != NULL)
         numNeighbors++;
   
      if(FindOtherFace(all_edges, fi[1], fi[2]) != NULL)
         numNeighbors++;
   
      if(FindOtherFace(all_edges, fi[2], fi[0]) != NULL)
         numNeighbors++;
   
      return numNeighbors;
   }
   
//----------------------------
// Builds the list of all face and edge infos.
   void BuildStripifyInfo(t_face_list &all_faces, t_edge_list &all_edges) const{

                              //find max vertex index referenced
      int max_vertex = 0;
      for(dword i=num_faces; i--; ){
         const I3D_triface &fc = in_faces[i];
         max_vertex = Max(max_vertex, (int)fc[0]);
         max_vertex = Max(max_vertex, (int)fc[1]);
         max_vertex = Max(max_vertex, (int)fc[2]);
      }
      ++max_vertex;

                              //reserve space for the face infos, but do not resize them
      all_faces.reserve(num_faces);
   
                              //we actually resize the edge infos, so we must initialize to NULL
      all_edges.resize(max_vertex);
      memset(all_edges.begin(), 0, max_vertex*sizeof(C_edge_info*));
   
      tmp_face_info.assign(num_faces);
      tmp_edge_info.assign(num_faces * 3);
                              //iterate through the triangles of the triangle list
      for(i=num_faces; i--; ){
         const I3D_triface &fc = in_faces[i];
      
                              //create the face info and add it to the list of faces, 
         C_face_info *faceInfo = new(&tmp_face_info[i]) C_face_info(fc, (word)i);
         all_faces.push_back(faceInfo);
                              //add info for 3 edges
         for(dword j=3; j--; ){
            word v0 = fc[j];
            word v1 = fc[next_tri_indx[j]];
                              //create the info
            C_edge_info *ei = new(&tmp_edge_info[i*3+j]) C_edge_info(v0, v1, faceInfo);
         
                              //update the linked list of edge
            ei->next = all_edges[v0];
            all_edges[v0] = ei;
         }
      }
   }

//----------------------------

   // let our strip info classes and the other classes get
   // to these protected stripificaton methods if they want
   friend C_strip_info;
public:
   C_stripifier(const I3D_triface *f, dword nf, dword in_cacheSize, dword in_num_samples):
      in_faces(f),
      num_faces(nf),
      cache_size(in_cacheSize),
      num_samples(in_num_samples)
   {}

//----------------------------
// the target vertex cache size, the structure to place the strips in, and the input indices
   void Stripify(t_face_list &all_faces, t_strip_list &out_strips) const{

                              //build the stripification info
      t_edge_list all_edges;
      BuildStripifyInfo(all_faces, all_edges);
   
                              //stripify
#ifdef DEBUG_NO_CACHE_OPTIM
      out_strips.reserve(all_faces.size());
      FindAllStrips(out_strips, all_faces, all_edges);
#else
      t_strip_list tmp_strips;
      tmp_strips.reserve(all_faces.size());
      FindAllStrips(tmp_strips, all_faces, all_edges);
                                 //split up the strips into cache friendly pieces, optimize them,
                                 // then dump these into outStrips
      SplitUpStripsAndOptimize(tmp_strips, outStrips, all_edges, cache_size);

                                 //clean up
      for(dword i = 0; i < tmp_strips.size(); i++)
         delete tmp_strips[i];
#endif
      /*
                              //clean-up edges
      for(i = 0; i < all_edges.size(); i++){
         C_edge_info *info = all_edges[i];
         while(info){
            C_edge_info *next = info->next;
            info->Release();
            info = next;
         }
      }
      */
   }
};

/*
//----------------------------
// Returns true if the next face should be ordered in CW fashion
inline bool NextIsCW(dword numIndices){
	return ((numIndices % 2) == 0);
}

//----------------------------
// Returns true if the face is ordered in CW fashion.
inline bool IsCW(const I3D_triface &fc, word v0, word v1){

	if(fc[0] == v0)
		return (fc[1] == v1);
	
	if(fc[1] == v0)
		return (fc[2] == v1);

	return (fc[0] == v1);
}
*/

//----------------------------

static void GetPackedFaces(const word *beg, dword num_i, C_vector<I3D_triface> &faces){

   C_destripifier destrip;
   destrip.Begin(beg);
   for(dword j=num_i-2; j--; ){
      destrip.Next();
      if(!destrip.IsDegenerate())
         faces.push_back(destrip.GetFace());
   }
}

//----------------------------

static bool Equals(const I3D_triface &f0, const I3D_triface &f1){

   int i = f0.FindIndex(f1[0]);
   if(i==-1)
      return false;
   i = next_tri_indx[i];
   if(f1[1]!=f0[i])
      return false;
   i = next_tri_indx[i];
   return (f1[2]==f0[i]);
}

//----------------------------

void MakeStrips(const I3D_triface *faces, dword num_faces, C_vector<word> &out_strips, word *face_remap){

                              //determine number of experiment samples
                              // (the more faces, the less experiments we make)
   dword num_samples = MAX_SAMPLES;
   if(num_faces > 8000){
      num_samples = Max(1, int(MAX_SAMPLES - (num_faces-8000)/2500));
   }

   C_stripifier ns(faces, num_faces, CACHE_SIZE, num_samples);
   C_stripifier::t_strip_list tmp_strips;
   C_stripifier::t_face_list all_faces;
   ns.Stripify(all_faces, tmp_strips);

   out_strips.clear();
   out_strips.reserve(num_faces*3);

	assert(tmp_strips.size());
   dword dst_index = 0;
   dword i;

                              //preprocessing - order 1st face
   for(i=tmp_strips.size(); i--; ){
      C_stripifier::C_strip_info *strip = tmp_strips[i];
		dword num_strip_faces = strip->faces.size();
		assert(num_strip_faces);

      C_stripifier::C_face_info &face0 = *strip->faces.front();

			                        //if there is a second face, reorder vertices such that the unique vertex is first
		if(num_strip_faces > 1){
         const I3D_triface &face1 = *strip->faces[1];
         int uni_index = face0.FindSharedEdge(face1);
         assert(uni_index!=-1);
         uni_index = (uni_index+2)%3;
         if(uni_index==1)
            (I3D_triface&)face0 = I3D_triface(face0[1], face0[2], face0[0]);
         else
         if(uni_index==2)
            (I3D_triface&)face0 = I3D_triface(face0[2], face0[0], face0[1]);

				                       //if there is a third face, reorder vertices such that the shared vertex is last
			if(num_strip_faces > 2){
            const I3D_triface &face2 = *strip->faces[2];
            if(face2.ContainsIndex(face0[1])){
               swap(face0[1], face0[2]);
               //ccw_changed = true;
               face0.flipped = true;
            }
			}
      }
   }

   I3D_triface face_last(0, 0, 0);
#if 1
   while(tmp_strips.size()){
                              //choose best next strip
      int best_ratio = 0;
      int best_index = 0;
      int out_strips_size = out_strips.size();
      for(dword i=tmp_strips.size(); i--; ){
         const C_stripifier::C_strip_info *s = tmp_strips[i];
         dword num_f = s->faces.size();

                              //ones with more strips win
         int r = num_f;
         if(out_strips_size){
            const C_stripifier::C_face_info &f0 = *s->faces.front();
            if(num_f==1){
                              //for along tris, it's ok to shuffle it, so accept if it simply contains last face's index
               if(f0.ContainsIndex(face_last.vi[2]))
                  r = 1002;
            }else{
                              //if the strip contain's last face's index...
               if(f0.vi[0]==face_last.vi[2]){
                  r = 1000;
                              //accept it if it won't need to be negated
                  bool cull_ok = ((out_strips_size-2)&1);
                  if(cull_ok == !f0.flipped)
                     r = 1001;
               }
            }
         }
         if(best_ratio < r){
            best_ratio = r;
            best_index = i;
            if(best_ratio >= 1000)
               break;
         }
      }
      C_stripifier::C_strip_info *strip = tmp_strips[best_index];
      tmp_strips[best_index] = tmp_strips.back();
      tmp_strips.pop_back();

		dword num_strip_faces = strip->faces.size();
		assert(num_strip_faces);
		                            //handle the first face in the strip
		{
         I3D_triface face0 = *strip->faces[0];

         bool ccw_changed = strip->faces.front()->flipped;

			if(out_strips.size()){
            if(num_strip_faces==1){
                              //if we share index with last face, put it to front
               if(face0.FindIndex(face_last[2]) > 0){
                  do{
                     word v = face0[0];
                     face0[0] = face0[1];
                     face0[1] = face0[2];
                     face0[2] = v;
                  }while(face0.FindIndex(face_last[2])!=0);
               }
            }
                              //tap the first in the new strip
            out_strips.push_back(face_last[2]);
                              //tap also the new face, if it's 1st index is not equal to last face's last index
            if(face0[0]!=face_last[2])
               out_strips.push_back(face0[0]);

                              //make sure our new face is properly culled
            if((out_strips.size()-2)&1)
               ccw_changed = !ccw_changed;
			}
                              //correct culling
         if(ccw_changed){
            if(num_strip_faces <= 2){
                              //for 2 faces, culling is corrected by simply swapping last 2 indicies
               swap(face0[1], face0[2]);
            }else{
                              //add one dummy degenerate index
               out_strips.push_back(face0[0]);
            }
         }
                              //output this face
			out_strips.push_back(face0[0]);
         out_strips.push_back(face0[1]);
         out_strips.push_back(face0[2]);
         if(face_remap)
            face_remap[dst_index++] = strip->faces[0]->face_index;

			                           //update last face info
         face_last = face0;
		}

		for(dword j = 1; j < num_strip_faces; j++){
         const I3D_triface &face_n = *strip->faces[j];
         int uni_index = face_n.FindSharedEdge(face_last);
         assert(uni_index!=-1);
         uni_index = (uni_index+2)%3;
         word v = face_n[uni_index];
         out_strips.push_back(v);
			face_last[0] = face_last[1];
         face_last[1] = face_last[2];
			face_last[2] = v;

         if(face_remap)
            //face_remap[strip->m_faces[j]->face_index] = dst_index++;
            face_remap[dst_index++] = strip->faces[j]->face_index;
		}

      delete strip;
   }
#else
   dword num_strips = tmp_strips.size();
	for(i = 0; i < num_strips; i++){
		const C_stripifier::C_strip_info *strip = tmp_strips[i];
		dword num_strip_faces = strip->faces.size();
		assert(num_strip_faces);
		                            //handle the first face in the strip
		{
         I3D_triface face0 = *strip->faces.front();

         bool ccw_changed = strip->faces.front()->flipped;
			if(i==0){
                              //nothing to do, 
            /*
            if(!IsCW(*strip->faces[0], face0[0], face0[1])){
               //out_strips.push_back(face0[0]);
               ccw_changed = true;
            }
            */
         }else{
            if(num_strip_faces==1){
                              //if we share index with last face, put it to front
               if(face0.FindIndex(face_last[2]) > 0){
                  do{
                     word v = face0[0];
                     face0[0] = face0[1];
                     face0[1] = face0[2];
                     face0[2] = v;
                  }while(face0.FindIndex(face_last[2])!=0);
               }
            }
                              //tap the first in the new strip
            out_strips.push_back(face_last[2]);
                              //tap also the new face, if it's 1st index is not equal to last face's last index
            if(face0[0]!=face_last[2])
               out_strips.push_back(face0[0]);

                              //make sure our new face is properly culled
            if((out_strips.size()-2)&1)
               ccw_changed = !ccw_changed;
               /**/
            /*
				                          //check CW/CCW ordering
				if(NextIsCW(out_strips.size()) != IsCW(*strip->faces[0], face0[0], face0[1])){
               //out_strips.push_back(tFirstFace.m_v0);
               out_strips.push_back(face0[0]);
				}
            /**/
			}
                              //correct culling
         //*
         if(ccw_changed){
            if(num_strip_faces <= 2){
                              //for 2 faces, culling is corrected by simply swapping last 2 indicies
               swap(face0[1], face0[2]);
            }else{
                              //add one dummy degenerate index
               out_strips.push_back(face0[0]);
            }
         }
         /**/

			out_strips.push_back(face0[0]);
         out_strips.push_back(face0[1]);
         out_strips.push_back(face0[2]);
         if(face_remap)
            face_remap[dst_index++] = strip->faces[0]->face_index;

			                           //update last face info
         face_last = face0;
		}

		for(dword j = 1; j < num_strip_faces; j++){
         const I3D_triface &face_n = *strip->faces[j];
         int uni_index = face_n.FindSharedEdge(face_last);
         assert(uni_index!=-1);
         uni_index = (uni_index+2)%3;
         word v = face_n[uni_index];
         out_strips.push_back(v);
			face_last[0] = face_last[1];
         face_last[1] = face_last[2];
			face_last[2] = v;

         if(face_remap)
            //face_remap[strip->m_faces[j]->face_index] = dst_index++;
            face_remap[dst_index++] = strip->faces[j]->face_index;
		}
      delete strip;
	}
#endif

#if defined _DEBUG && 0
                              //validity check
   {
      C_vector<I3D_triface> fb;
      fb.reserve(num_faces);
      GetPackedFaces(out_strips.begin(), out_strips.size(), fb);
      assert(fb.size()==num_faces);
                              //make sure each input face appears in decompressed faces
      C_buffer<bool> ok(num_faces, false);
      for(dword i=num_faces; i--; ){
         const I3D_triface &fc = faces[i];
                              //search in decompressed faces
         for(int j=num_faces; j--; ){
            if(!ok[j] && Equals(fb[j], fc))
               break;
         }
         assert(j!=-1);
         assert(!ok[j]);
         ok[j] = true;
      }
   }
#endif
                              //clean up
   //for(i=all_faces.size(); i--; )
      //delete all_faces[i];
}

//----------------------------


