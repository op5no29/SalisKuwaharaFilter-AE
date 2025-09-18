/*******************************************************************/
/*                                                                 */
/*                   Salis Kuwahara Filter                         */
/*                                                                 */
/*******************************************************************/

#include "SalisKuwaharaFilter.h"
#include "../KuwaharaCore/API.h"

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
											"%s v%d.%d\r%s",
											STR(StrID_Name), 
											MAJOR_VERSION, 
											MINOR_VERSION, 
											STR(StrID_Description));
	return PF_Err_NONE;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	out_data->out_flags =  PF_OutFlag_DEEP_COLOR_AWARE;	// 16bpc support
	out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;	// MFR support
	
	return PF_Err_NONE;
}

static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	AEFX_CLR_STRUCT(def);

	// Mode parameter
	PF_ADD_POPUP(STR(StrID_Mode_Param_Name),
				 KUWAHARA_MODE_COUNT,
				 KUWAHARA_MODE_CLASSIC + 1,  // default is Classic (1-based for popup)
				 STR(StrID_Mode_Choices),
				 MODE_DISK_ID);

	AEFX_CLR_STRUCT(def);

	// Radius parameter
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Radius_Param_Name),
						 KUWAHARA_RADIUS_MIN,
						 KUWAHARA_RADIUS_MAX,
						 KUWAHARA_RADIUS_MIN,
						 KUWAHARA_RADIUS_MAX,
						 KUWAHARA_RADIUS_DFLT,
						 PF_Precision_INTEGER,
						 0,
						 0,
						 RADIUS_DISK_ID);

	AEFX_CLR_STRUCT(def);

	// Mix parameter
	PF_ADD_PERCENT(STR(StrID_Mix_Param_Name),
				   KUWAHARA_MIX_DFLT,
				   MIX_DISK_ID);
	
	out_data->num_params = KUWAHARA_NUM_PARAMS;

	return err;
}

static PF_Err 
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err				err		= PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	// Get parameters
	// TODO: Read and handle the Mode parameter (params[KUWAHARA_MODE])
	// TODO: Consider updating algorithm to support floating-point radius for higher quality
	A_long radius = (A_long)params[KUWAHARA_RADIUS]->u.fs_d.value;  // Currently casting to integer
	PF_FpLong mix = params[KUWAHARA_MIX]->u.fd.value;
	
	// Clamp radius to valid range
	if (radius < KUWAHARA_RADIUS_MIN) radius = KUWAHARA_RADIUS_MIN;
	if (radius > KUWAHARA_RADIUS_MAX) radius = KUWAHARA_RADIUS_MAX;
	
	// Process based on bit depth
	if (PF_WORLD_IS_DEEP(output)) {
		err = ProcessKuwaharaWorld16(in_data, 
									&params[KUWAHARA_INPUT]->u.ld,
									output,
									radius,
									mix);
	} else {
		err = ProcessKuwaharaWorld8(in_data,
								   &params[KUWAHARA_INPUT]->u.ld,
								   output,
								   radius,
								   mix);
	}

	return err;
}

extern "C" DllExport
PF_Err PluginDataEntryFunction2(
	PF_PluginDataPtr inPtr,
	PF_PluginDataCB2 inPluginDataCallBackPtr,
	SPBasicSuite* inSPBasicSuitePtr,
	const char* inHostName,
	const char* inHostVersion)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;

	result = PF_REGISTER_EFFECT_EXT2(
		inPtr,
		inPluginDataCallBackPtr,
		"Salis Kuwahara Filter",			// Name
		"com.salis.ae.kuwaharafilter",		// Match Name
		"Salis Effects",					// Category
		AE_RESERVED_INFO,					// Reserved Info
		"EffectMain",						// Entry point
		"https://salis.com");				// support URL

	return result;
}

PF_Err
EffectMain(
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:
				err = About(in_data, out_data, params, output);
				break;
				
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(in_data, out_data, params, output);
				break;
				
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(in_data, out_data, params, output);
				break;
				
			case PF_Cmd_RENDER:
				err = Render(in_data, out_data, params, output);
				break;
		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}