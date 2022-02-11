/**********************************************************************
 *<
	FILE: prim.h

	DESCRIPTION:

	CREATED BY: Dan Silva

	HISTORY:

 *>	Copyright (c) 1994, All Rights Reserved.
 **********************************************************************/

#ifndef __PRIM__H
#define __PRIM__H

#include "resource.h"

TCHAR *GetString(int id);

extern ClassDesc* GetBoxobjDesc();
extern ClassDesc* GetSphereDesc();
extern ClassDesc* GetCylinderDesc();
extern ClassDesc* GetSimpleCamDesc();
extern ClassDesc* GetOmniLightDesc();
extern ClassDesc* GetDirLightDesc();
extern ClassDesc *GetTDirLightDesc();
extern ClassDesc* GetFSpotLightDesc();
extern ClassDesc* GetTSpotLightDesc();
extern ClassDesc* GetLookatCamDesc();
extern ClassDesc* GetSplineDesc();
#ifdef DESIGN_VER
extern ClassDesc* GetOrthoSplineDesc();
#endif
extern ClassDesc* GetNGonDesc();
extern ClassDesc* GetDonutDesc();
extern ClassDesc* GetTargetObjDesc();
extern ClassDesc* GetBonesDesc();
extern ClassDesc* GetRingMasterDesc();
extern ClassDesc* GetSlaveControlDesc();
extern ClassDesc* GetQuadPatchDesc();
extern ClassDesc* GetTriPatchDesc();
extern ClassDesc* GetTorusDesc();
extern ClassDesc* GetMorphObjDesc();
extern ClassDesc* GetCubicMorphContDesc();
extern ClassDesc* GetRectangleDesc();
extern ClassDesc* GetBoolObjDesc();
extern ClassDesc* GetTapeHelpDesc();
extern ClassDesc* GetProtHelpDesc();
extern ClassDesc* GetTubeDesc();
extern ClassDesc* GetConeDesc();
#ifndef NO_EXTENDED_PRIMITIVES
extern ClassDesc* GetHedraDesc();
#endif // NO_EXTENDED_PRIMITIVES
extern ClassDesc* GetCircleDesc();
extern ClassDesc* GetEllipseDesc();
extern ClassDesc* GetArcDesc();
extern ClassDesc* GetStarDesc();
extern ClassDesc* GetHelixDesc();
#ifndef NO_PARTICLES
extern ClassDesc* GetRainDesc();
extern ClassDesc* GetSnowDesc();
#endif // NO_PARTICLES
extern ClassDesc* GetTextDesc();
extern ClassDesc* GetTeapotDesc();
extern ClassDesc* GetBaryMorphContDesc();
#ifdef DESIGN_VER
extern ClassDesc* GetOrthoSplineDesc();
extern ClassDesc* GetParallelCamDesc();
#endif
extern ClassDesc* GetGridobjDesc();
extern ClassDesc* GetNewBonesDesc();

extern HINSTANCE hInstance;


#endif
