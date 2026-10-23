/* The on-screen controls of the phone: a stick on one side, the console's buttons on the other, read from the fingers SDL reports
 * (window.c calls touch_event) and added to the keyboard's player in hle_hid.c (touch_state). The launcher's touch.ini (enabled, size,
 * opacity, left_handed, in percent) says how they look. They are drawn over the whole window by gpu_overlay.c (touch_draw).
 *
 * The layout is in units of a tenth of the window's height, from its top left; the window is 10 units high and aspect * 10 wide. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL.h>

#include "touch.h"

enum { B_A = 0, B_B = 1, B_X = 2, B_Y = 3, B_L = 6, B_R = 7, B_ZL = 8, B_ZR = 9, B_PLUS = 10, B_MINUS = 11 };
enum { MAX_FINGERS = 10, SHAPE_ROUND = 0, SHAPE_WIDE = 1, SHAPE_SMALL = 2 };

typedef struct Control {
    int bit;              /* the console button */
    float x, y, w, h;     /* centre and size, in units from the left (right-handed layout) */
    int shape;
    const char *label;
} Control;

static const Control controls[] = {
    {B_A, -2.2f, 5.9f, 1.7f, 1.7f, SHAPE_ROUND, "A"},   /* (negative x counts from the right edge) */
    {B_B, -3.5f, 7.4f, 1.7f, 1.7f, SHAPE_ROUND, "B"},
    {B_X, -3.5f, 4.4f, 1.7f, 1.7f, SHAPE_ROUND, "X"},
    {B_Y, -4.8f, 5.9f, 1.7f, 1.7f, SHAPE_ROUND, "Y"},
    {B_R, -1.9f, 1.0f, 3.0f, 1.2f, SHAPE_WIDE, "R"},
    {B_ZR, -1.9f, 2.5f, 3.0f, 1.2f, SHAPE_WIDE, "ZR"},
    {B_L, 1.9f, 1.0f, 3.0f, 1.2f, SHAPE_WIDE, "L"},
    {B_ZL, 1.9f, 2.5f, 3.0f, 1.2f, SHAPE_WIDE, "ZL"},
    {B_PLUS, 0.0f, 0.7f, 1.3f, 0.9f, SHAPE_SMALL, "+"},
    {B_MINUS, 0.0f, 0.7f, 1.3f, 0.9f, SHAPE_SMALL, "-"},
};
#define NCONTROLS ((int)(sizeof controls / sizeof *controls))

static int loaded, enabled = 1, size_pct = 60, opacity_pct = 70, left_handed;
static float aspect = 2.0f;

typedef struct Finger { int used; long long id; float x, y; int stick; } Finger;   /* x, y in units */
static Finger fingers[MAX_FINGERS];
static volatile unsigned pressed;      /* buttons held, by their console bit */
static volatile int out_lx, out_ly, held_stick;

static void load(void) {
    loaded = 1;
    FILE *f = fopen("touch.ini", "r");
    if (!f) return;
    char line[160];
    while (fgets(line, sizeof line, f)) {
        char key[64]; int v;
        if (line[0] == ';' || sscanf(line, " %63[a-z_] = %d", key, &v) != 2) continue;
        if (!strcmp(key, "enabled")) enabled = v;
        else if (!strcmp(key, "size")) size_pct = v;
        else if (!strcmp(key, "opacity")) opacity_pct = v;
        else if (!strcmp(key, "left_handed")) left_handed = v;
    }
    fclose(f);
}

static float scale(void) { return 0.7f + size_pct / 100.0f * 0.6f; }   /* the size slider: 0.7 to 1.3 of the layout */

/* where a control is, in units, with the right edge, the hand and the size taken into account */
static void place(const Control *c, float *x, float *y, float *w, float *h) {
    float s = scale();
    float cx = c->x;
    if (c->bit == B_PLUS || c->bit == B_MINUS) cx = aspect * 5.0f + (c->bit == B_PLUS ? 1.1f : -1.1f);
    else if (cx < 0) cx = aspect * 10.0f + cx * s;
    else cx = cx * s;
    if (left_handed && c->bit != B_PLUS && c->bit != B_MINUS) cx = aspect * 10.0f - cx;
    float cy = c->y * s;
    if (c->y > 4.0f) cy = 10.0f - (10.0f - c->y) * s;   /* the lower ones keep their distance from the bottom */
    *x = cx; *y = cy; *w = c->w * s; *h = c->h * s;
}

static void stick_zone(float *cx, float *cy, float *r) {
    float s = scale();
    *r = 1.9f * s;
    *cx = (left_handed ? aspect * 10.0f - 3.4f * s : 3.4f * s);
    *cy = 10.0f - 3.3f * s;
}

static int hit(const Control *c, float fx, float fy) {
    float x, y, w, h;
    place(c, &x, &y, &w, &h);
    if (c->shape == SHAPE_ROUND) { float dx = fx - x, dy = fy - y, r = w * 0.62f; return dx * dx + dy * dy <= r * r; }
    return fabsf(fx - x) <= w * 0.55f && fabsf(fy - y) <= h * 0.75f;
}

static void recompute(void) {
    unsigned buttons = 0;
    int lx = 0, ly = 0, stick = 0;
    float scx, scy, sr;
    stick_zone(&scx, &scy, &sr);
    for (int i = 0; i < MAX_FINGERS; i++) {
        if (!fingers[i].used) continue;
        if (fingers[i].stick) {
            float dx = (fingers[i].x - scx) / sr, dy = (scy - fingers[i].y) / sr;   /* up is positive */
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 1.0f) { dx /= len; dy /= len; len = 1.0f; }
            if (len > 0.12f) { lx = (int)(dx * 32767); ly = (int)(dy * 32767); }
            stick = 1;
            continue;
        }
        for (int c = 0; c < NCONTROLS; c++)
            if (hit(&controls[c], fingers[i].x, fingers[i].y)) buttons |= 1u << controls[c].bit;
    }
    out_lx = lx; out_ly = ly; held_stick = stick;
    pressed = buttons;
}

void touch_event(int kind, long long id, float nx, float ny, float aspect_ratio) {
    if (!loaded) load();
    if (!enabled) return;
    aspect = aspect_ratio > 0.5f ? aspect_ratio : aspect;
    float x = nx * aspect * 10.0f, y = ny * 10.0f;
    Finger *f = NULL, *free_slot = NULL;
    for (int i = 0; i < MAX_FINGERS; i++) {
        if (fingers[i].used && fingers[i].id == id) f = &fingers[i];
        if (!fingers[i].used && !free_slot) free_slot = &fingers[i];
    }
    if (kind == TOUCH_UP) { if (f) f->used = 0; }
    else {
        if (!f) {
            if (!free_slot) return;
            f = free_slot;
            f->used = 1; f->id = id;
            float scx, scy, sr;
            stick_zone(&scx, &scy, &sr);
            float dx = x - scx, dy = y - scy;
            int on_buttons = 0;
            for (int c = 0; c < NCONTROLS; c++) if (hit(&controls[c], x, y)) on_buttons = 1;
            /* the stick takes a finger that lands in its half of the screen, low down and away from every button */
            int mine = (left_handed ? x > aspect * 5.0f : x < aspect * 5.0f) && y > 3.5f && !on_buttons;
            int taken = 0;
            for (int i = 0; i < MAX_FINGERS; i++) if (fingers[i].used && fingers[i].stick) taken = 1;
            f->stick = mine && !taken && dx * dx + dy * dy < sr * sr * 6.0f;
        }
        f->x = x; f->y = y;
    }
    recompute();
}

unsigned long long touch_state(int *lx, int *ly) {
    if (!loaded) load();
    if (!enabled) return 0;
    if (held_stick) { *lx = out_lx; *ly = out_ly; }
    return pressed;
}

/* 5 x 7 letters for the buttons: the overlay has no font on the phone */
static const char *glyph(const char *c) {
    switch (c[0]) {
    case 'A': return "01110" "10001" "10001" "11111" "10001" "10001" "10001";
    case 'B': return "11110" "10001" "10001" "11110" "10001" "10001" "11110";
    case 'X': return "10001" "10001" "01010" "00100" "01010" "10001" "10001";
    case 'Y': return "10001" "10001" "01010" "00100" "00100" "00100" "00100";
    case 'L': return "10000" "10000" "10000" "10000" "10000" "10000" "11111";
    case 'R': return "11110" "10001" "10001" "11110" "10100" "10010" "10001";
    case 'Z': return "11111" "00001" "00010" "00100" "01000" "10000" "11111";
    case '+': return "00000" "00100" "00100" "11111" "00100" "00100" "00000";
    case '-': return "00000" "00000" "00000" "11111" "00000" "00000" "00000";
    }
    return "00000" "00000" "00000" "00000" "00000" "00000" "00000";
}

static void label(float cx, float cy, float cell, const char *s, unsigned color) {
    int n = (int)strlen(s);
    float total = (float)n * 5 * cell + (float)(n - 1) * cell;
    float x0 = cx - total / 2, y0 = cy - 3.5f * cell;
    for (int k = 0; k < n; k++) {
        const char *g = glyph(s + k);
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 5; c++)
                if (g[r * 5 + c] == '1') ovl_rect(x0 + ((float)k * 6 + (float)c) * cell, y0 + (float)r * cell, cell, cell, color);
    }
}

static void disc(float cx, float cy, float r, unsigned color) {   /* (a round shape of bars, one pixel row apart at least) */
    int rows = (int)(r / 3.0f);
    if (rows < 6) rows = 6;
    if (rows > 48) rows = 48;
    for (int i = 0; i < rows; i++) {
        float y0 = -r + 2.0f * r * (float)i / (float)rows, y1 = -r + 2.0f * r * (float)(i + 1) / (float)rows, m = (y0 + y1) / 2;
        float half = sqrtf(r * r - m * m);
        ovl_rect(cx - half, cy + y0, half * 2, y1 - y0, color);
    }
}

void touch_draw(unsigned bw, unsigned bh) {
    if (!loaded) load();
    if (!enabled) return;
    float u = (float)bh / 10.0f;
    unsigned a = (unsigned)(opacity_pct * 255 / 100);
    float scx, scy, sr;
    stick_zone(&scx, &scy, &sr);
    unsigned pr = pressed;
    disc(scx * u, scy * u, sr * u, OVL_RGBA(255, 255, 255, a / 4));
    float kx = scx, ky = scy;
    if (held_stick) { kx += (float)out_lx / 32767.0f * sr; ky -= (float)out_ly / 32767.0f * sr; }
    disc(kx * u, ky * u, sr * 0.45f * u, OVL_RGBA(255, 255, 255, held_stick ? a : a * 2 / 3));
    for (int c = 0; c < NCONTROLS; c++) {
        float x, y, w, h;
        place(&controls[c], &x, &y, &w, &h);
        int down = (pr >> controls[c].bit) & 1;
        unsigned fill = OVL_RGBA(255, 255, 255, down ? a : a / 3), ink = OVL_RGBA(20, 20, 30, down ? 255 : 230);
        if (controls[c].shape == SHAPE_ROUND) disc(x * u, y * u, w / 2 * u, fill);
        else ovl_rect((x - w / 2) * u, (y - h / 2) * u, w * u, h * u, fill);
        label(x * u, y * u, (controls[c].shape == SHAPE_SMALL ? 0.09f : 0.12f) * u * scale() / 0.7f * 0.8f, controls[c].label, ink);
    }
    (void)bw;
}
