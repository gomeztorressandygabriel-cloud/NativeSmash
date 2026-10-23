/* The phone's on-screen controls (touch.c). */
#pragma once

enum { TOUCH_DOWN, TOUCH_MOVE, TOUCH_UP };

/* A finger: nx, ny in 0..1 over the window, aspect = its width over its height. */
void touch_event(int kind, long long id, float nx, float ny, float aspect);
/* The console buttons held (bit numbers of nn::hid::NpadButton) and, if the stick is held, its left stick (+y up). */
unsigned long long touch_state(int *lx, int *ly);
/* The controls, for the overlay to draw over a window of bw x bh pixels (between ovl_rect calls of gpu_overlay.h). */
void touch_draw(unsigned bw, unsigned bh);

#ifndef OVL_RGBA
#define OVL_RGBA(r, g, b, a) ((unsigned)(r) | (unsigned)(g) << 8 | (unsigned)(b) << 16 | (unsigned)(a) << 24)
void ovl_rect(float x, float y, float w, float h, unsigned color);
#endif
