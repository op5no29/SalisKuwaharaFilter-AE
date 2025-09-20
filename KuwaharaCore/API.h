/*******************************************************************/
/* Kuwahara Core Algorithm API — build-safe                        */
/*******************************************************************/
#pragma once
#ifndef KUWAHARA_CORE_API_H
#define KUWAHARA_CORE_API_H

#include "AE_Effect.h"
#include "AE_GeneralPlug.h"

// ---- Sequence cache (unified across all translation units) ----
typedef struct {
    A_long     version;
    A_long     cached_width;
    A_long     cached_height;
    A_long     cached_radius;
    PF_FpLong  cached_anisotropy;
    void*      structure_tensor_data;   // opaque
    A_Boolean  needs_recompute;
} KuwaharaSequenceData;

// Opaque tensor
void* CreateStructureTensorField();
void  DeleteStructureTensorField(void* field);

// Tensor computation per depth
void ComputeStructureTensorField8   (const PF_EffectWorld* input, void* field_ptr);
void ComputeStructureTensorField16  (const PF_EffectWorld* input, void* field_ptr);
void ComputeStructureTensorField32f (const PF_EffectWorld* input, void* field_ptr);

// Processing (SmartFX)
PF_Err ProcessKuwaharaWorld8Smart(
    PF_InData*, PF_EffectWorld* in, PF_EffectWorld* out,
    A_long radius, A_long sectorCount, PF_FpLong aniso, PF_FpLong soft, PF_FpLong mix,
    void* seq_data_ptr);

PF_Err ProcessKuwaharaWorld16Smart(
    PF_InData*, PF_EffectWorld* in, PF_EffectWorld* out,
    A_long radius, A_long sectorCount, PF_FpLong aniso, PF_FpLong soft, PF_FpLong mix,
    void* seq_data_ptr);

PF_Err ProcessKuwaharaWorld32fSmart(
    PF_InData*, PF_EffectWorld* in, PF_EffectWorld* out,
    A_long radius, A_long sectorCount, PF_FpLong aniso, PF_FpLong soft, PF_FpLong mix,
    void* seq_data_ptr);

// Legacy (non-smart) wrappers
PF_Err ProcessKuwaharaWorld8 (PF_InData*, PF_EffectWorld*, PF_EffectWorld*, A_long, A_long, PF_FpLong, PF_FpLong, PF_FpLong);
PF_Err ProcessKuwaharaWorld16(PF_InData*, PF_EffectWorld*, PF_EffectWorld*, A_long, A_long, PF_FpLong, PF_FpLong, PF_FpLong);
PF_Err ProcessKuwaharaWorld32f(PF_InData*, PF_EffectWorld*, PF_EffectWorld*, A_long, A_long, PF_FpLong, PF_FpLong, PF_FpLong);

#endif

