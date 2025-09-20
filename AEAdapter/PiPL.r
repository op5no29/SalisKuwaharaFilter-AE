/* Salis Kuwahara Filter — PiPL (canonical, decimal-only) */
#include "AEConfig.h"
#include "AE_EffectVers.h"
#include "AE_General.r"

resource 'PiPL' (16000)
{
  {
    Kind                 { AEEffect },
    Name                 { "Salis Kuwahara Filter" },
    Category             { "Salis Effects" },

    /* 1.0.0 = 65536 (0x00010000) */
    AE_Effect_Version    { 65536 },

#ifdef AE_OS_MAC
    CodeMacARM64         { "EffectMain" },
#else
    CodeWin64X86         { "EffectMain" },
#endif

    /* OutFlags = 512 (DEEP_COLOR_AWARE) + 256 (PIX_INDEPENDENT) + 32 (USE_OUTPUT_EXTENT) = 800 */
    AE_Effect_Global_OutFlags   { 800 },

    /* OutFlags2 = 1024 (SUPPORTS_SMART_RENDER のみ。Float-aware は未広告) */
    AE_Effect_Global_OutFlags_2 { 1024 },

    AE_Effect_Match_Name { "com.salis.ae.kuwaharafilter.v1003" },
    /* PiPL テンプレート仕様：integer を 2 個 */
    AE_PiPL_Version      { 2, 0 },
    AE_Reserved_Info     { 0 }
  }
};

