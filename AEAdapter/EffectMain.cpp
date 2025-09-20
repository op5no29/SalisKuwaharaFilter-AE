/*******************************************************************/
/* Salis Kuwahara Filter — SmartFX entry (final)                   */
/*******************************************************************/
#include "SalisKuwaharaFilter.h"
#include "API.h"
#include "AEFX_SuiteHelper.h"

#define SDK_NO_FLOAT_AWARE 1

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpragma-pack"
  #pragma clang diagnostic ignored "-Wmacro-redefined"
#endif
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"
#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

#include <algorithm>
#include <cmath>

#ifndef PF_WORLD_IS_FLOAT
  // Older SDKs don’t expose this macro. When unavailable, we won’t advertise float.
  #define PF_WORLD_IS_FLOAT(w) (0)
#endif

template<typename T>
static inline T clampT(T v, T lo, T hi) { return (v < lo) ? lo : (v > hi ? hi : v); }

extern "C" char* GetStringPtr(int strNum);
#define STR(x) GetStringPtr(x)

// ---- About ------------------------------------------------------------------
static PF_Err About(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef*[], PF_LayerDef*) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg, "%s\r%s",
        STR(StrID_Name), STR(StrID_Description));
    return PF_Err_NONE;
}

// ---- Global setup / setdown --------------------------------------------------
static PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef*[], PF_LayerDef*) {
    // バージョン
    out_data->my_version = PF_VERSION(1,0,0,0,0);

    // ★PiPL.r と完全一致させるため数値で固定（十進）
    //   800 = 512(DEEP_COLOR_AWARE) + 256(PIX_INDEPENDENT) + 32(USE_OUTPUT_EXTENT)
    //  1024 = SUPPORTS_SMART_RENDER
    out_data->out_flags  = 800;
    out_data->out_flags2 = 1024;

    // シーケンスデータ確保
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    out_data->sequence_data = suites.HandleSuite1()->host_new_handle(sizeof(KuwaharaSequenceData));
    if (!out_data->sequence_data) {
        return PF_Err_OUT_OF_MEMORY;
    }

    // 初期化
    auto* seq = reinterpret_cast<KuwaharaSequenceData*>(
        suites.HandleSuite1()->host_lock_handle(out_data->sequence_data));
    if (seq) {
        seq->version = 1;
        seq->cached_width  = 0;
        seq->cached_height = 0;
        seq->cached_radius = -1;
        seq->cached_anisotropy = -1.0;
        seq->structure_tensor_data = nullptr;
        seq->needs_recompute = TRUE;
        suites.HandleSuite1()->host_unlock_handle(out_data->sequence_data);
    }
    
    // Status beacon for verification
    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg, "Salis Kuwahara v1.0 (%s %s)", __DATE__, __TIME__);
    
    return PF_Err_NONE;
}

static PF_Err GlobalSetdown(PF_InData* in_data, PF_OutData*, PF_ParamDef*[], PF_LayerDef*) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    if (in_data->sequence_data) {
        if (auto* seq = reinterpret_cast<KuwaharaSequenceData*>(
                suites.HandleSuite1()->host_lock_handle(in_data->sequence_data))) {
            if (seq->structure_tensor_data) {
                DeleteStructureTensorField(seq->structure_tensor_data);
                seq->structure_tensor_data = nullptr;
            }
            suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data);
        }
        suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);
    }
    return PF_Err_NONE;
}


// ---- UI params ---------------------------------------------------------------
static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef*[], PF_LayerDef*) {
    PF_ParamDef def;  AEFX_CLR_STRUCT(def);

    PF_ADD_FLOAT_SLIDERX(STR(StrID_Radius_Param_Name),
        0.5, 200, 0.5, 100, 8, PF_Precision_TENTHS, 0, PF_ParamFlag_SUPERVISE, RADIUS_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(STR(StrID_Sectors_Param_Name),
        3, 16, 3, 16, 8, PF_Precision_INTEGER, 0, PF_ParamFlag_SUPERVISE, SECTORS_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(STR(StrID_Anisotropy_Param_Name),
        0, 100, 0, 100, 15, PF_Precision_TENTHS, 0, PF_ParamFlag_SUPERVISE, ANISOTROPY_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(STR(StrID_Softness_Param_Name),
        0, 100, 0, 100, 20, PF_Precision_TENTHS, 0, 0, SOFTNESS_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(STR(StrID_Mix_Param_Name),
        0, 100, 0, 100, 100, PF_Precision_TENTHS, 0, 0, MIX_DISK_ID);

    out_data->num_params = KUWAHARA_NUM_PARAMS;
    return PF_Err_NONE;
}

static PF_Err SequenceSetup(PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*)   { return PF_Err_NONE; }
static PF_Err SequenceSetdown(PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*) { return PF_Err_NONE; }

// ---- Smart PreRender ---------------------------------------------------------
static PF_Err PreRender(PF_InData* in_data, PF_OutData*, PF_PreRenderExtra* pre) {
    PF_Err err = PF_Err_NONE;
    
    // Status beacon - PreRender doesn't have out_data, so we'll skip this one

    PF_RenderRequest req = pre->input->output_request;
    req.channel_mask = PF_ChannelMask_ARGB;
    req.preserve_rgb_of_zero_alpha = TRUE;

    PF_CheckoutResult in_res;
    ERR(pre->cb->checkout_layer(in_data->effect_ref, KUWAHARA_INPUT, KUWAHARA_INPUT,
                                &req, in_data->current_time, in_data->time_step, in_data->time_scale, &in_res));

    if (!err) {  // 失敗時に未初期化の矩形へ触れないための安全策
        pre->output->result_rect = in_res.result_rect;
        pre->output->max_result_rect = in_res.max_result_rect;
        pre->output->flags = 0;
    }
    return err;
}

// ---- Smart Render ------------------------------------------------------------
static PF_Err SmartRender(PF_InData* in_data, PF_OutData*, PF_SmartRenderExtra* sren) {
    PF_Err err = PF_Err_NONE;
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    
    // Status beacon - SmartRender also doesn't have out_data

    PF_EffectWorld *input = nullptr, *output = nullptr;
    if (!err) err = sren->cb->checkout_layer_pixels(in_data->effect_ref, KUWAHARA_INPUT, &input);
    if (!err) err = sren->cb->checkout_output(in_data->effect_ref, &output);
    if (err || !input || !output) return err ? err : PF_Err_INTERNAL_STRUCT_DAMAGED;

    PF_ParamDef rp, sp, ap, sop, mp;
    AEFX_CLR_STRUCT(rp); AEFX_CLR_STRUCT(sp); AEFX_CLR_STRUCT(ap); AEFX_CLR_STRUCT(sop); AEFX_CLR_STRUCT(mp);
    if (!err) err = PF_CHECKOUT_PARAM(in_data, KUWAHARA_RADIUS,     in_data->current_time, in_data->time_step, in_data->time_scale, &rp);
    if (!err) err = PF_CHECKOUT_PARAM(in_data, KUWAHARA_SECTORS,    in_data->current_time, in_data->time_step, in_data->time_scale, &sp);
    if (!err) err = PF_CHECKOUT_PARAM(in_data, KUWAHARA_ANISOTROPY, in_data->current_time, in_data->time_step, in_data->time_scale, &ap);
    if (!err) err = PF_CHECKOUT_PARAM(in_data, KUWAHARA_SOFTNESS,   in_data->current_time, in_data->time_step, in_data->time_scale, &sop);
    if (!err) err = PF_CHECKOUT_PARAM(in_data, KUWAHARA_MIX,        in_data->current_time, in_data->time_step, in_data->time_scale, &mp);
    if (err) { sren->cb->checkin_layer_pixels(in_data->effect_ref, KUWAHARA_INPUT); return err; }

    PF_FpLong radius     = rp.u.fs_d.value;
    A_long    sectors    = (A_long)sp.u.fs_d.value;
    PF_FpLong anisotropy = ap.u.fs_d.value / 100.0;
    PF_FpLong softness   = sop.u.fs_d.value / 100.0;
    PF_FpLong mix        = mp.u.fs_d.value / 100.0;

    if (radius > 50) radius = 50 + (radius - 50) * 2;
    radius = clampT<PF_FpLong>(radius, 0.5, 400.0);
    sectors = clampT<A_long>(sectors, 3, 16);

    KuwaharaSequenceData* seq = nullptr;
    if (in_data->sequence_data) {
        seq = reinterpret_cast<KuwaharaSequenceData*>(suites.HandleSuite1()->host_lock_handle(in_data->sequence_data));
        if (seq) {
            const A_Boolean needs_update =
                (seq->cached_width  != input->width)  ||
                (seq->cached_height != input->height) ||
                (seq->cached_radius != (A_long)radius)||
                (std::fabs(seq->cached_anisotropy - anisotropy) > 0.01);

            if (needs_update) {
                if (seq->structure_tensor_data) { DeleteStructureTensorField(seq->structure_tensor_data); seq->structure_tensor_data = nullptr; }
                seq->cached_width      = input->width;
                seq->cached_height     = input->height;
                seq->cached_radius     = (A_long)radius;
                seq->cached_anisotropy = anisotropy;
                seq->needs_recompute   = TRUE;
            } else if (!seq->structure_tensor_data) {
                seq->needs_recompute = TRUE;
            }
            if (!seq->structure_tensor_data) seq->structure_tensor_data = CreateStructureTensorField();
        }
    }

    if (PF_WORLD_IS_FLOAT(output)) {
        err = ProcessKuwaharaWorld32fSmart(in_data, input, output, (A_long)radius, sectors, anisotropy, softness, mix, seq);
    } else if (PF_WORLD_IS_DEEP(output)) {
        err = ProcessKuwaharaWorld16Smart(in_data, input, output, (A_long)radius, sectors, anisotropy, softness, mix, seq);
    } else {
        err = ProcessKuwaharaWorld8Smart (in_data, input, output, (A_long)radius, sectors, anisotropy, softness, mix, seq);
    }

    if (seq) suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data);

    PF_CHECKIN_PARAM(in_data, &rp);
    PF_CHECKIN_PARAM(in_data, &sp);
    PF_CHECKIN_PARAM(in_data, &ap);
    PF_CHECKIN_PARAM(in_data, &sop);
    PF_CHECKIN_PARAM(in_data, &mp);
    sren->cb->checkin_layer_pixels(in_data->effect_ref, KUWAHARA_INPUT);
    return err;
}

// ---- Legacy Render (8/16 only) ----------------------------------------------
static PF_Err Render(PF_InData* in_data, PF_OutData*, PF_ParamDef* params[], PF_LayerDef* output) {
    PF_FpLong radius     = params[KUWAHARA_RADIUS]->u.fs_d.value;
    A_long    sectors    = (A_long)params[KUWAHARA_SECTORS]->u.fs_d.value;
    PF_FpLong anisotropy = params[KUWAHARA_ANISOTROPY]->u.fs_d.value / 100.0;
    PF_FpLong softness   = params[KUWAHARA_SOFTNESS]->u.fs_d.value   / 100.0;
    PF_FpLong mix        = params[KUWAHARA_MIX]->u.fs_d.value        / 100.0;

    if (radius > 50) radius = 50 + (radius - 50) * 2;
    radius = clampT<PF_FpLong>(radius, 0.5, 400.0);
    sectors = clampT<A_long>(sectors, 3, 16);

    if (PF_WORLD_IS_DEEP(output)) {
        return ProcessKuwaharaWorld16(in_data, &params[KUWAHARA_INPUT]->u.ld, output,
                                      (A_long)radius, sectors, anisotropy, softness, mix);
    } else {
        return ProcessKuwaharaWorld8 (in_data, &params[KUWAHARA_INPUT]->u.ld, output,
                                      (A_long)radius, sectors, anisotropy, softness, mix);
    }
}

static PF_Err UpdateParameterUI(PF_InData* in_data, PF_OutData*, PF_ParamDef*[], PF_LayerDef*) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    if (in_data->sequence_data) {
        auto* seq = reinterpret_cast<KuwaharaSequenceData*>(suites.HandleSuite1()->host_lock_handle(in_data->sequence_data));
        if (seq) { seq->needs_recompute = TRUE; suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data); }
    }
    return PF_Err_NONE;
}

// ---- Registration ------------------------------------------------------------
extern "C" DllExport
PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr, PF_PluginDataCB2 cb2, SPBasicSuite*, const char*, const char*) {
    A_Err result = A_Err_NONE; // required by macro
    PF_REGISTER_EFFECT_EXT2(inPtr, cb2,
        "Salis Kuwahara Filter", "com.salis.ae.kuwaharafilter.v1003",
        "Salis Effects", AE_RESERVED_INFO,
        "EffectMain", "https://x.com/c_y_l_i");
    return result;
}

extern "C" DllExport
PF_Err EffectMain(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra) {
    PF_Err err = PF_Err_NONE;
    try {
        switch (cmd) {
        case PF_Cmd_ABOUT:             err = About(in_data, out_data, params, output); break;
        case PF_Cmd_GLOBAL_SETUP:      err = GlobalSetup(in_data, out_data, params, output); break;
        case PF_Cmd_GLOBAL_SETDOWN:    err = GlobalSetdown(in_data, out_data, params, output); break;
        case PF_Cmd_PARAMS_SETUP:      err = ParamsSetup(in_data, out_data, params, output); break;
        case PF_Cmd_SEQUENCE_SETUP:    err = SequenceSetup(in_data, out_data, params, output); break;
        case PF_Cmd_SEQUENCE_SETDOWN:  err = SequenceSetdown(in_data, out_data, params, output); break;
        case PF_Cmd_SMART_PRE_RENDER:  err = PreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra*>(extra)); break;
        case PF_Cmd_SMART_RENDER:      err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra)); break;
        case PF_Cmd_RENDER:            err = Render(in_data, out_data, params, output); break;
        case PF_Cmd_UPDATE_PARAMS_UI:  err = UpdateParameterUI(in_data, out_data, params, output); break;
        default: break;
        }
    } catch (PF_Err& e) { err = e; }
      catch (...)      { err = PF_Err_INTERNAL_STRUCT_DAMAGED; }
    return err;
}

