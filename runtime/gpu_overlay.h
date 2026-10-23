/* The debug overlay: frame rate, a frame time graph and statistics, drawn over each
 * presented frame (gpu_overlay.c); and the settings menu's drawing (gpu_menu.c). */
#pragma once
#include <d3d11.h>

/* Called once per presented frame, after the picture is in rtv (at x, y, w x h): notes the
 * frame's time and, if show, draws the overlay over the picture; detail (lines separated by
 * '\n', or NULL) goes under the graph. The settings menu, while open, is drawn instead of the
 * overlay (menu_draw). Leaves the pipeline state changed. */
void overlay_present(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11RenderTargetView *rtv, unsigned x, unsigned y, unsigned w,
                     unsigned h, int show, const char *detail);

#ifdef __ANDROID__
/* The phone's on-screen controls over the whole window (bw x bh), after overlay_present. */
void overlay_touch(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11RenderTargetView *rtv, unsigned bw, unsigned bh);
#endif

/* The start's loading screen into rtv (w x h): "Preparing shaders" and a bar, done of total. */
void overlay_loading(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11RenderTargetView *rtv, unsigned w, unsigned h, long done,
                     long total, double seconds);

/* For menu_draw, in the picture's pixels (0, 0 its top left): a rectangle, and a line of text
 * whose letters are `size` pixels high and size / 2 wide (printable ASCII; '\n' starts a line).
 * Colours are 0xAABBGGRR (OVL_RGBA). ovl_text returns the y below the text. */
#define OVL_RGBA(r, g, b, a) ((unsigned)(r) | (unsigned)(g) << 8 | (unsigned)(b) << 16 | (unsigned)(a) << 24)
void ovl_rect(float x, float y, float w, float h, unsigned color);
float ovl_text(float x, float y, float size, unsigned color, const char *s);
