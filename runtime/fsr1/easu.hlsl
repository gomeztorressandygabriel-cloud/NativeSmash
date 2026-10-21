// FidelityFX Super Resolution 1 (AMD, MIT), the upscaling pass (EASU): third_party/fsr1/.
// Compiled to gpu_fsr_shaders.h by tools/make_fsr1.py; the renderer draws it (gpu_fsr.c).
// con0..con3: FsrEasuCon's constants; origin: where the picture's top left is in the target.
cbuffer Params : register(b0) { uint4 con0, con1, con2, con3; uint4 origin; };
Texture2D<float4> InputTexture : register(t0);
SamplerState samLinearClamp : register(s0);
#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"
#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { return InputTexture.GatherRed(samLinearClamp, p, int2(0, 0)); }
AF4 FsrEasuGF(AF2 p) { return InputTexture.GatherGreen(samLinearClamp, p, int2(0, 0)); }
AF4 FsrEasuBF(AF2 p) { return InputTexture.GatherBlue(samLinearClamp, p, int2(0, 0)); }
#include "ffx_fsr1.h"
float4 main(float4 pos : SV_Position) : SV_Target {
    AF3 c;
    FsrEasuF(c, AU2(pos.xy) - origin.xy, con0, con1, con2, con3);
    return float4(c, 1.0);
}
