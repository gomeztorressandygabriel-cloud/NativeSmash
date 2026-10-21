/* AMD FidelityFX Super Resolution 1 (gpu_fsr.h). The two passes are AMD's own shaders
 * (runtime/fsr1/*.hlsl over third_party/fsr1/, compiled by tools/make_fsr1.py into
 * gpu_fsr_shaders.h); their constants are worked out by AMD's own CPU functions. */
#define COBJMACROS
#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <d3d11.h>

#define A_CPU 1
#include "../third_party/fsr1/ffx_a.h"
#include "../third_party/fsr1/ffx_fsr1.h"

#include "gpu_fsr.h"
#include "gpu_fsr_shaders.h"

static ID3D11PixelShader *easu_ps, *rcas_ps;
static ID3D11Buffer *easu_cb, *rcas_cb;
static ID3D11Texture2D *mid_tex;                 /* EASU's picture, at the output's size, before RCAS */
static ID3D11ShaderResourceView *mid_srv;
static ID3D11RenderTargetView *mid_rtv;
static unsigned mid_w, mid_h;
static int state;                                /* 0 not tried, 1 ready, -1 failed */

static int init(ID3D11Device *dev) {
    D3D11_BUFFER_DESC bd = {80, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
    if (FAILED(ID3D11Device_CreatePixelShader(dev, fsr1_easu_ps, sizeof fsr1_easu_ps, NULL, &easu_ps)) ||
        FAILED(ID3D11Device_CreatePixelShader(dev, fsr1_rcas_ps, sizeof fsr1_rcas_ps, NULL, &rcas_ps)) ||
        FAILED(ID3D11Device_CreateBuffer(dev, &bd, NULL, &easu_cb))) return 0;
    bd.ByteWidth = 32;
    return SUCCEEDED(ID3D11Device_CreateBuffer(dev, &bd, NULL, &rcas_cb));
}

/* EASU's picture, w x h: made again when the size changes. */
static int make_mid(ID3D11Device *dev, unsigned w, unsigned h) {
    if (mid_tex && mid_w == w && mid_h == h) return 1;
    if (mid_srv) ID3D11ShaderResourceView_Release(mid_srv);
    if (mid_rtv) ID3D11RenderTargetView_Release(mid_rtv);
    if (mid_tex) ID3D11Texture2D_Release(mid_tex);
    mid_tex = NULL; mid_srv = NULL; mid_rtv = NULL;
    D3D11_TEXTURE2D_DESC td = {w, h, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT,
                               D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 0};
    if (FAILED(ID3D11Device_CreateTexture2D(dev, &td, NULL, &mid_tex)) ||
        FAILED(ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)mid_tex, NULL, &mid_srv)) ||
        FAILED(ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)mid_tex, NULL, &mid_rtv))) {
        if (mid_srv) ID3D11ShaderResourceView_Release(mid_srv);
        if (mid_rtv) ID3D11RenderTargetView_Release(mid_rtv);
        if (mid_tex) ID3D11Texture2D_Release(mid_tex);
        mid_tex = NULL; mid_srv = NULL; mid_rtv = NULL;
        return 0;
    }
    mid_w = w; mid_h = h;
    return 1;
}

static void put(ID3D11DeviceContext *ctx, ID3D11Buffer *cb, const void *data, size_t size) {
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        memcpy(m.pData, data, size);
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)cb, 0);
    }
}

int fsr_draw(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11ShaderResourceView *src, unsigned in_w, unsigned in_h,
             ID3D11RenderTargetView *dst, unsigned x, unsigned y, unsigned w, unsigned h, ID3D11SamplerState *clamp_linear,
             float sharpness) {
    if (state == 0) {
        state = init(dev) ? 1 : -1;
        if (state < 0) fprintf(stderr, "[gfx] FSR 1: its shaders did not load: the plain picture is drawn\n");
    }
    if (state < 0 || !src || !in_w || !in_h || !w || !h) return 0;
    int rcas = sharpness > 0.0f;
    if (rcas && !make_mid(dev, w, h)) return 0;

    /* EASU: input at (in_w x in_h), output w x h; its pixel position is relative to the picture's
     * corner, which is the target's own when RCAS follows (the picture's own size) and (x, y) when
     * it goes straight into the window */
    struct { AU1 con[16]; AU1 origin[4]; } ec;
    FsrEasuCon(ec.con, ec.con + 4, ec.con + 8, ec.con + 12, (AF1)in_w, (AF1)in_h, (AF1)in_w, (AF1)in_h, (AF1)w, (AF1)h);
    ec.origin[0] = rcas ? 0 : x; ec.origin[1] = rcas ? 0 : y; ec.origin[2] = ec.origin[3] = 0;
    put(ctx, easu_cb, &ec, sizeof ec);
    D3D11_VIEWPORT vp = {rcas ? 0.0f : (float)x, rcas ? 0.0f : (float)y, (float)w, (float)h, 0, 1};
    ID3D11RenderTargetView *target = rcas ? mid_rtv : dst;
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &target, NULL);
    ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
    ID3D11DeviceContext_PSSetShader(ctx, easu_ps, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 1, &easu_cb);
    ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &clamp_linear);
    ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &src);
    ID3D11DeviceContext_Draw(ctx, 3, 0);

    if (rcas) {
        /* RCAS: sharpness in stops (0: the sharpest, 2: a quarter of it), from 1 (most) down to 0 */
        struct { AU1 con[4]; AU1 origin[4]; } rc;
        float stops = 2.0f * (1.0f - (sharpness > 1.0f ? 1.0f : sharpness));
        FsrRcasCon(rc.con, (AF1)stops);
        rc.origin[0] = x; rc.origin[1] = y; rc.origin[2] = rc.origin[3] = 0;
        put(ctx, rcas_cb, &rc, sizeof rc);
        D3D11_VIEWPORT wvp = {(float)x, (float)y, (float)w, (float)h, 0, 1};
        ID3D11ShaderResourceView *none = NULL;
        ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &none);   /* (the mid picture is a target until now) */
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &dst, NULL);
        ID3D11DeviceContext_RSSetViewports(ctx, 1, &wvp);
        ID3D11DeviceContext_PSSetShader(ctx, rcas_ps, NULL, 0);
        ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 1, &rcas_cb);
        ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &mid_srv);
        ID3D11DeviceContext_Draw(ctx, 3, 0);
    }
    ID3D11ShaderResourceView *none = NULL;
    ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &none);
    ID3D11Buffer *no_cb = NULL;
    ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 1, &no_cb);
    return 1;
}
