/*******************************************************************/
/*                                                                 */
/*                   Salis Kuwahara Filter                         */
/*                   _ _ _ _ _ _ _ _ _ _ _                         */
/*                                                                 */
/* Copyright 2025 Salis                                            */
/* All Rights Reserved.                                            */
/*                                                                 */
/*******************************************************************/

#pragma once

#ifndef SALIS_KUWAHARA_FILTER_H
#define SALIS_KUWAHARA_FILTER_H

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned short		u_int16;
typedef unsigned long		u_long;
typedef short int			int16;
#define PF_TABLE_BITS	12
#define PF_TABLE_SZ_16	4096

#define PF_DEEP_COLOR_AWARE 1	// make sure we get 16bpc pixels; 
								// AE_Effect.h checks for this.

#include "AEConfig.h"

#ifdef AE_OS_WIN
	typedef unsigned short PixelType;
	#define NOMINMAX
	#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "Strings.h"

/* Versioning information */

#define	MAJOR_VERSION	1
#define	MINOR_VERSION	0
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1


/* Parameter defaults */

#define	KUWAHARA_RADIUS_MIN		1
#define	KUWAHARA_RADIUS_MAX		50
#define	KUWAHARA_RADIUS_DFLT	5

#define	KUWAHARA_MIX_MIN		0
#define	KUWAHARA_MIX_MAX		100
#define	KUWAHARA_MIX_DFLT		100

#define	KUWAHARA_SECTORS_MIN	3
#define	KUWAHARA_SECTORS_MAX	16
#define	KUWAHARA_SECTORS_DFLT	4  // Changed from 8 to 4

#define	KUWAHARA_ANISOTROPY_MIN		0.0
#define	KUWAHARA_ANISOTROPY_MAX		1.0
#define	KUWAHARA_ANISOTROPY_DFLT	0.0

#define	KUWAHARA_SOFTNESS_MIN		0.0
#define	KUWAHARA_SOFTNESS_MAX		1.0
#define	KUWAHARA_SOFTNESS_DFLT		0.0

enum {
	KUWAHARA_INPUT = 0,
	KUWAHARA_RADIUS,
	KUWAHARA_SECTORS,
	KUWAHARA_ANISOTROPY,
	KUWAHARA_SOFTNESS,
	KUWAHARA_MIX,
	KUWAHARA_NUM_PARAMS
};

enum {
	RADIUS_DISK_ID = 1,
	SECTORS_DISK_ID,
	ANISOTROPY_DISK_ID,
	SOFTNESS_DISK_ID,
	MIX_DISK_ID,
};

typedef struct KuwaharaInfo {
	PF_FpLong	radius;
	A_long		sectorCount;
	PF_FpLong	anisotropy;
	PF_FpLong	softness;
	PF_FpLong	mix;
	// Cache for performance
	A_long		radiusInt;
	PF_EffectWorld	*input;
	A_long		width;
	A_long		height;
} KuwaharaInfo, *KuwaharaInfoP, **KuwaharaInfoH;

#ifndef DllExport
#  if defined(__APPLE__)
#    define DllExport __attribute__((visibility("default")))
#  else
#    define DllExport __declspec(dllexport)
#  endif
#endif

extern "C" {

	DllExport
	PF_Err
	EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output,
		void			*extra);

}

#endif // SALIS_KUWAHARA_FILTER_H
