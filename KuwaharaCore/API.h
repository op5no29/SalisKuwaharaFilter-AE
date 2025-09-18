/*******************************************************************/
/*                                                                 */
/*                   Kuwahara Core Algorithm API                   */
/*                                                                 */
/*******************************************************************/

#pragma once

#ifndef KUWAHARA_CORE_API_H
#define KUWAHARA_CORE_API_H

#include "AE_Effect.h"

// Process a single pixel using Kuwahara filter
PF_Err ProcessKuwaharaPixel8(
    const PF_EffectWorld* input,
    A_long x, 
    A_long y,
    A_long radius,
    PF_Pixel8* outP);

PF_Err ProcessKuwaharaPixel16(
    const PF_EffectWorld* input,
    A_long x, 
    A_long y,
    A_long radius,
    PF_Pixel16* outP);

// Process entire world using Kuwahara filter
PF_Err ProcessKuwaharaWorld8(
    PF_InData* in_data,
    PF_EffectWorld* input,
    PF_EffectWorld* output,
    A_long radius,
    PF_FpLong mix);

PF_Err ProcessKuwaharaWorld16(
    PF_InData* in_data,
    PF_EffectWorld* input,
    PF_EffectWorld* output,
    A_long radius,
    PF_FpLong mix);

#endif // KUWAHARA_CORE_API_H