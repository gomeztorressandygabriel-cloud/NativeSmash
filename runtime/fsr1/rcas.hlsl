// FidelityFX Super Resolution 1 (AMD, MIT), the sharpening pass (RCAS): third_party/fsr1/.
// Compiled to gpu_fsr_shaders.h by tools/make_fsr1.py: Direct3D 11's DXBC (gpu_fsr.c), and for Vulkan the
// source with AMD's headers in it, made SPIR-V when FSR is first drawn (gpu_vk.c, FSR_VULKAN).
// con: FsrRcasCon's constants; origin: where the picture's top left is in the target.
cbuffer Params : register(b0) { uint4 con; uint4 origin; };
#ifdef FSR_VULKAN   /* (gpu_vk.c's binding map: textures from t64) */
Texture2D<float4> InputTexture : register(t64);
#else
Texture2D<float4> InputTexture : register(t0);
#endif
#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"
#define FSR_RCAS_F 1
AF4 FsrRcasLoadF(ASU2 p) { return InputTexture.Load(int3(ASU2(p), 0)); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
#include "ffx_fsr1.h"
float4 main(float4 pos : SV_Position) : SV_Target {
    AF3 c;
    FsrRcasF(c.r, c.g, c.b, AU2(pos.xy) - origin.xy, con);
    return float4(c, 1.0);
}
