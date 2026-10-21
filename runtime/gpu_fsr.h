/* AMD FidelityFX Super Resolution 1 (third_party/fsr1/, MIT): the picture the game drew, made as big
 * as its place in the window by EASU (the upscaling) and then sharpened by RCAS - in gamma space, as
 * AMD's own samples do. Direct3D 11 only. */
#pragma once
#include <d3d11.h>

/* Draws src (in_w x in_h, plain bytes as sRGB-encoded) into the rectangle (x, y, w, h) of dst, which
 * the caller's blit vertex shader and primitive state are set up for. sharpness: 0 none (EASU alone,
 * straight into dst), up to 1 the most RCAS gives. Returns 0, having drawn nothing, when it cannot
 * (its shaders did not load): the caller draws the picture itself. Leaves the pixel shader, its
 * constant buffer, resources and the render target for the caller to set again. */
int fsr_draw(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11ShaderResourceView *src, unsigned in_w, unsigned in_h,
             ID3D11RenderTargetView *dst, unsigned x, unsigned y, unsigned w, unsigned h, ID3D11SamplerState *clamp_linear,
             float sharpness);
