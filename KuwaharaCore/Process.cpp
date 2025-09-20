/*******************************************************************/
/* Kuwahara Core Algorithm — final build-safe implementation       */
/*******************************************************************/
#include "API.h"

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpragma-pack"
  #pragma clang diagnostic ignored "-Wmacro-redefined"
#endif
#include "AE_EffectCB.h"
#include "AEGP_SuiteHandler.h"
#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

#include <cmath>
#include <algorithm>
#include <vector>

#ifdef _OPENMP
  #include <omp.h>
  #define USE_OPENMP 1
#else
  #define USE_OPENMP 0
#endif

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// ---- Structure tensor field -------------------------------------------------
struct StructureTensorField {
    std::vector<float> e1, e2, vx, vy;
    A_long w=0, h=0;
    void init(A_long W, A_long H) {
        w=W; h=H;
        const size_t N = static_cast<size_t>(W)*H;
        e1.assign(N,0.f); e2.assign(N,0.f); vx.assign(N,1.f); vy.assign(N,0.f);
    }
    inline void get(A_long x, A_long y, float& _e1, float& _e2, float& _vx, float& _vy) const {
        const size_t i = static_cast<size_t>(y)*w + static_cast<size_t>(x);
        _e1=e1[i]; _e2=e2[i]; _vx=vx[i]; _vy=vy[i];
    }
};

void* CreateStructureTensorField()            { return new StructureTensorField(); }
void  DeleteStructureTensorField(void* field) { delete reinterpret_cast<StructureTensorField*>(field); }

// ---- Luma helpers -----------------------------------------------------------
static inline float Luma8 (const PF_EffectWorld* w, A_long x, A_long y) {
    const PF_Pixel8* p = reinterpret_cast<const PF_Pixel8*>(reinterpret_cast<const char*>(w->data) + y*w->rowbytes) + x;
    return (0.299f*p->red + 0.587f*p->green + 0.114f*p->blue) / 255.0f;
}
static inline float Luma16(const PF_EffectWorld* w, A_long x, A_long y) {
    const PF_Pixel16* p = reinterpret_cast<const PF_Pixel16*>(reinterpret_cast<const char*>(w->data) + y*w->rowbytes) + x;
    return (0.299f*p->red + 0.587f*p->green + 0.114f*p->blue) / 32768.0f;
}
static inline float Luma32f(const PF_EffectWorld* w, A_long x, A_long y) {
    const PF_PixelFloat* p = reinterpret_cast<const PF_PixelFloat*>(reinterpret_cast<const char*>(w->data) + y*w->rowbytes) + x;
    return 0.299f*p->red + 0.587f*p->green + 0.114f*p->blue;
}

// Simple separable blur used inside structure-tensor smoothing
static void BoxBlur(std::vector<float>& img, A_long W, A_long H, int K) {
    const A_long half = K/2;
    std::vector<float> tmp(static_cast<size_t>(W)*H);
#if USE_OPENMP
#pragma omp parallel for
#endif
    for (A_long y=0;y<H;++y){
        for (A_long x=0;x<W;++x){
            float s=0; int c=0;
            for (A_long k=-half;k<=half;++k){ A_long nx = std::max<A_long>(0,std::min<A_long>(W-1,x+k)); s+=img[static_cast<size_t>(y)*W+nx]; ++c; }
            tmp[static_cast<size_t>(y)*W+x]=s/(float)c;
        }
    }
#if USE_OPENMP
#pragma omp parallel for
#endif
    for (A_long y=0;y<H;++y){
        for (A_long x=0;x<W;++x){
            float s=0; int c=0;
            for (A_long k=-half;k<=half;++k){ A_long ny = std::max<A_long>(0,std::min<A_long>(H-1,y+k)); s+=tmp[static_cast<size_t>(ny)*W+x]; ++c; }
            img[static_cast<size_t>(y)*W+x]=s/(float)c;
        }
    }
}

// ---- Structure tensor (8/16/32f) -------------------------------------------
static void ComputeST_Generic(const PF_EffectWorld* in, StructureTensorField* f, float(*L)(const PF_EffectWorld*,A_long,A_long)){
    const A_long W=in->width,H=in->height;
    f->init(W,H);
    std::vector<float> Ix(static_cast<size_t>(W)*H), Iy(static_cast<size_t>(W)*H);

#if USE_OPENMP
#pragma omp parallel for
#endif
    for (A_long y=0;y<H;++y){
        for (A_long x=0;x<W;++x){
            float c=L(in,x,y);
            float r=(x<W-1)?L(in,x+1,y):c;
            float l=(x>0)  ?L(in,x-1,y):c;
            float d=(y<H-1)?L(in,x,y+1):c;
            float u=(y>0)  ?L(in,x,y-1):c;
            Ix[static_cast<size_t>(y)*W+x]=(r-l)*0.5f;
            Iy[static_cast<size_t>(y)*W+x]=(d-u)*0.5f;
        }
    }

    std::vector<float> Jxx(Ix.size()),Jxy(Ix.size()),Jyy(Ix.size());
#if USE_OPENMP
#pragma omp parallel for
#endif
    for (A_long y=0;y<H;++y){
        for (A_long x=0;x<W;++x){
            size_t i=static_cast<size_t>(y)*W+x;
            float ix=Ix[i],iy=Iy[i];
            Jxx[i]=ix*ix; Jxy[i]=ix*iy; Jyy[i]=iy*iy;
        }
    }

    const int K=5; BoxBlur(Jxx,W,H,K); BoxBlur(Jxy,W,H,K); BoxBlur(Jyy,W,H,K);

#if USE_OPENMP
#pragma omp parallel for
#endif
    for (A_long y=0;y<H;++y){
        for (A_long x=0;x<W;++x){
            size_t i=static_cast<size_t>(y)*W+x;
            float a=Jxx[i], b=Jxy[i], c=Jyy[i];
            float tr=a+c;
            float det=a*c-b*b;
            float disc=std::sqrt(std::max(0.f,tr*tr-4.f*det));
            float l1=0.5f*(tr+disc), l2=0.5f*(tr-disc);
            f->e1[i]=l1; f->e2[i]=l2;
            float vx=b, vy=l1-a;
            float n=std::sqrt(vx*vx+vy*vy+1e-6f);
            f->vx[i]=vx/n; f->vy[i]=vy/n;
        }
    }
}

void ComputeStructureTensorField8   (const PF_EffectWorld* in, void* fp){ ComputeST_Generic(in, reinterpret_cast<StructureTensorField*>(fp), &Luma8 ); }
void ComputeStructureTensorField16  (const PF_EffectWorld* in, void* fp){ ComputeST_Generic(in, reinterpret_cast<StructureTensorField*>(fp), &Luma16); }
void ComputeStructureTensorField32f (const PF_EffectWorld* in, void* fp){ ComputeST_Generic(in, reinterpret_cast<StructureTensorField*>(fp), &Luma32f); }

// ---- Pixel I/O (no templates → no deduction issues) -------------------------
static inline void fetchRGB(const PF_Pixel8* p,  float inv, float& r,float& g,float& b){ r=p->red*inv;   g=p->green*inv;   b=p->blue*inv; }
static inline void fetchRGB(const PF_Pixel16* p, float inv, float& r,float& g,float& b){ r=p->red*inv;   g=p->green*inv;   b=p->blue*inv; }
static inline void fetchRGB(const PF_PixelFloat* p, float, float& r,float& g,float& b){ r=p->red;        g=p->green;        b=p->blue; }

static inline void storeRGB(PF_Pixel8* p,  float r,float g,float b){
    p->red  = (A_u_char) std::round(std::max(0.f,std::min(1.f,r))*255.f);
    p->green= (A_u_char) std::round(std::max(0.f,std::min(1.f,g))*255.f);
    p->blue = (A_u_char) std::round(std::max(0.f,std::min(1.f,b))*255.f);
}
static inline void storeRGB(PF_Pixel16* p, float r,float g,float b){
    p->red  = (A_u_short)std::round(std::max(0.f,std::min(1.f,r))*32768.f);
    p->green= (A_u_short)std::round(std::max(0.f,std::min(1.f,g))*32768.f);
    p->blue = (A_u_short)std::round(std::max(0.f,std::min(1.f,b))*32768.f);
}
static inline void storeRGB(PF_PixelFloat* p, float r,float g,float b){
    p->red  = std::max(0.f,std::min(1.f,r));
    p->green= std::max(0.f,std::min(1.f,g));
    p->blue = std::max(0.f,std::min(1.f,b));
}

// ---- Core Kuwahara (shared for 8/16/32f) -----------------------------------
template<typename PIX>
static PF_Err KuwaharaCore(
    PF_InData*, PF_EffectWorld* input, PF_EffectWorld* output,
    A_long radius, A_long sectorCount, PF_FpLong anisotropy, PF_FpLong softness, PF_FpLong mix,
    KuwaharaSequenceData* seq,
    void(*ComputeST)(const PF_EffectWorld*, void*),
    float invMax)
{
    PF_Err err = PF_Err_NONE;

    // Acquire tensor
    StructureTensorField* tensor = nullptr;
    if (seq && seq->structure_tensor_data) tensor = reinterpret_cast<StructureTensorField*>(seq->structure_tensor_data);
    else {
        tensor = reinterpret_cast<StructureTensorField*>(CreateStructureTensorField());
        if (seq) seq->structure_tensor_data = tensor;
    }

    const bool needCompute = (!seq || seq->needs_recompute ||
                              seq->cached_width  != input->width ||
                              seq->cached_height != input->height ||
                              seq->cached_radius != radius ||
                              std::fabs(seq->cached_anisotropy - anisotropy) > 0.01);
    if (needCompute) {
        tensor->init(input->width, input->height);
        ComputeST(input, tensor);
        if (seq) {
            seq->cached_width = input->width; seq->cached_height = input->height;
            seq->cached_radius = radius; seq->cached_anisotropy = anisotropy;
            seq->needs_recompute = FALSE;
        }
    }

    const A_long W=input->width, H=input->height;
#if USE_OPENMP
#pragma omp parallel for
#endif
    for (A_long y=0;y<H;++y){
        const PIX* inRow  = reinterpret_cast<const PIX*>(reinterpret_cast<const char*>(input->data)  + y*input->rowbytes);
        PIX*       outRow = reinterpret_cast<PIX*>      (reinterpret_cast<char*>      (output->data) + y*output->rowbytes);
        for (A_long x=0;x<W;++x){

            float e1=0,e2=0,vx=1,vy=0;
            if (anisotropy>0.01) tensor->get(x,y,e1,e2,vx,vy);

            // Build a simple anisotropic scaling aligned to eigenvector
            float m00=1.f,m01=0.f,m10=0.f,m11=1.f;
            if (anisotropy>0.01f){
                float local = (e1 - e2) / (e1 + e2 + 1e-6f);
                float eff = static_cast<float>(anisotropy) * local;
                float alpha = 0.25f;
                float sx = alpha/(eff+alpha);
                float sy = (eff+alpha)/alpha;
                m00 =  vx * sx; m01 = -vy * sy;
                m10 =  vy * sx; m11 =  vx * sy;
            }

            struct Sector { double mR=0,mG=0,mB=0,sR2=0,sG2=0,sB2=0,c=0; } S[16];
            float minVar=1e10f, maxVar=0.f; int best=-1;

            const float half_ang = (float)M_PI / (float)sectorCount;
            const float step_a   = half_ang / 2.0f; // ~5 angular taps/sector

            for (int s=0;s<sectorCount;++s){
                Sector& T = S[s];
                const float base = (float)s * 2.0f * (float)M_PI / (float)sectorCount;

                for (float r=1.f; r<= (float)radius; r+=2.f){
                    for (float a=-half_ang; a<=half_ang+1e-6f; a+=step_a){
                        float ca = std::cos(base+a), sa = std::sin(base+a);
                        float sx = r*ca, sy=r*sa;
                        float ox = m00*sx + m01*sy;
                        float oy = m10*sx + m11*sy;
                        A_long xx = x + (A_long)std::lround(ox);
                        A_long yy = y + (A_long)std::lround(oy);
                        if ((unsigned)xx >= (unsigned)W || (unsigned)yy >= (unsigned)H) continue;

                        const PIX* p = reinterpret_cast<const PIX*>(reinterpret_cast<const char*>(input->data) + yy*input->rowbytes) + xx;
                        float rV,gV,bV; fetchRGB(p, invMax, rV,gV,bV);
                        T.mR += rV; T.mG += gV; T.mB += bV;
                        T.sR2 += rV*rV; T.sG2 += gV*gV; T.sB2 += bV*bV; T.c += 1.0;
                    }
                }
                if (T.c>0.0){
                    double invC = 1.0/T.c;
                    double mR=T.mR*invC, mG=T.mG*invC, mB=T.mB*invC;
                    double vR=std::max(0.0, T.sR2*invC - mR*mR);
                    double vG=std::max(0.0, T.sG2*invC - mG*mG);
                    double vB=std::max(0.0, T.sB2*invC - mB*mB);
                    float var = (float)(0.299*vR + 0.587*vG + 0.114*vB);
                    if (var<minVar){ minVar=var; best=s; }
                    if (var>maxVar){ maxVar=var; }
                }
            }

            float fR=0,fG=0,fB=0,wSum=0;
            float thr = minVar + (float)softness * (maxVar - minVar);
            for (int s=0;s<sectorCount;++s){
                const Sector& T = S[s];
                if (T.c<=0.0) continue;
                double invC = 1.0/T.c;
                double mR=T.mR*invC, mG=T.mG*invC, mB=T.mB*invC;
                double vR=std::max(0.0, T.sR2*invC - mR*mR);
                double vG=std::max(0.0, T.sG2*invC - mG*mG);
                double vB=std::max(0.0, T.sB2*invC - mB*mB);
                float var = (float)(0.299*vR + 0.587*vG + 0.114*vB);
                if (var<=thr){
                    float w = 1.0f / (1.0f + (var - minVar));
                    fR += (float)mR * w; fG += (float)mG * w; fB += (float)mB * w; wSum += w;
                }
            }
            if (wSum>0){ fR/=wSum; fG/=wSum; fB/=wSum; }
            else if (best>=0){
                const Sector& T = S[best]; double invC=1.0/T.c;
                fR=(float)(T.mR*invC); fG=(float)(T.mG*invC); fB=(float)(T.mB*invC);
            } else { outRow[x]=inRow[x]; continue; }

            // Mix with original
            float oR,oG,oB; fetchRGB(&inRow[x], invMax, oR,oG,oB);
            fR = fR * (float)mix + oR * (1.f - (float)mix);
            fG = fG * (float)mix + oG * (1.f - (float)mix);
            fB = fB * (float)mix + oB * (1.f - (float)mix);

            storeRGB(&outRow[x], fR,fG,fB);
            outRow[x].alpha = inRow[x].alpha;
        }
    }
    return err;
}

// ---- Smart wrappers ----------------------------------------------------------
PF_Err ProcessKuwaharaWorld8Smart (PF_InData* in, PF_EffectWorld* i, PF_EffectWorld* o, A_long r, A_long s, PF_FpLong a, PF_FpLong so, PF_FpLong m, void* q){
    return KuwaharaCore<PF_Pixel8>(in, i, o, r, s, a, so, m, reinterpret_cast<KuwaharaSequenceData*>(q), &ComputeStructureTensorField8, 1.0f/255.0f);
}
PF_Err ProcessKuwaharaWorld16Smart(PF_InData* in, PF_EffectWorld* i, PF_EffectWorld* o, A_long r, A_long s, PF_FpLong a, PF_FpLong so, PF_FpLong m, void* q){
    return KuwaharaCore<PF_Pixel16>(in, i, o, r, s, a, so, m, reinterpret_cast<KuwaharaSequenceData*>(q), &ComputeStructureTensorField16, 1.0f/32768.0f);
}
PF_Err ProcessKuwaharaWorld32fSmart(PF_InData* in, PF_EffectWorld* i, PF_EffectWorld* o, A_long r, A_long s, PF_FpLong a, PF_FpLong so, PF_FpLong m, void* q){
    return KuwaharaCore<PF_PixelFloat>(in, i, o, r, s, a, so, m, reinterpret_cast<KuwaharaSequenceData*>(q), &ComputeStructureTensorField32f, 1.0f);
}

// ---- Legacy wrappers ---------------------------------------------------------
PF_Err ProcessKuwaharaWorld8 (PF_InData* in, PF_EffectWorld* i, PF_EffectWorld* o, A_long r, A_long s, PF_FpLong a, PF_FpLong so, PF_FpLong m){
    return ProcessKuwaharaWorld8Smart (in, i, o, r, s, a, so, m, nullptr);
}
PF_Err ProcessKuwaharaWorld16(PF_InData* in, PF_EffectWorld* i, PF_EffectWorld* o, A_long r, A_long s, PF_FpLong a, PF_FpLong so, PF_FpLong m){
    return ProcessKuwaharaWorld16Smart(in, i, o, r, s, a, so, m, nullptr);
}
PF_Err ProcessKuwaharaWorld32f(PF_InData* in, PF_EffectWorld* i, PF_EffectWorld* o, A_long r, A_long s, PF_FpLong a, PF_FpLong so, PF_FpLong m){
    return ProcessKuwaharaWorld32fSmart(in, i, o, r, s, a, so, m, nullptr);
}

