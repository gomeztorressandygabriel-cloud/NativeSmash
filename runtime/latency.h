/* Latency counters for build\play.log (gpu_latency.c): on only while a file called latency.on
 * exists beside the game (or SSBU_LATENCY=1). Off, every hook is one load and a branch. */
#pragma once
#include "host.h"

enum {   /* histograms of times, in the order the log prints them */
    LAT_SEND_GAP,        /* between two datagrams the game sends to the same address and port */
    LAT_RECV_GAP,        /* between two datagrams that arrive from the same address and port */
    LAT_RECV_WAIT,       /* a datagram's time in the game's socket queue: arrival until the game reads it */
    LAT_PAD_AGE,         /* how old the pad state is when the game asks for it */
    LAT_PAD_TO_SHOWN,    /* the game's last pad read before a frame was queued, until that frame was handed to the display */
    LAT_QUEUE_TO_SHOWN,  /* a frame queued by the game until it was handed to the display */
    LAT_FRAME_GAP,       /* between two frames the game queues */
    LAT_GAME_WORK,       /* the vsync event until the game queues a frame (its simulation and drawing) */
    LAT_TICK_GAP,        /* between two vsync ticks */
    LAT_TICK_LATE,       /* the clock's wake-up after the instant the tick was due */
    LAT_GPU_WAIT,        /* the vsync thread waiting for the GPU thread to finish the frame to show */
    LAT_PRESENT_FRAME,   /* the vsync thread in present_frame (the renderer's lock, drawing, Present) */
    LAT_PRESENT_CALL,    /* IDXGISwapChain::Present itself */
    LAT_FRAME_WAIT,      /* present_wait = on: the vsync thread waiting for a free slot in the swap chain, outside the renderer's lock */
    LAT_LOCK_WAIT,       /* the vsync thread waiting to enter the renderer (its lock is the GPU thread's too) */
    LAT_HISTS
};
enum { LAT_TICKS, LAT_REPEATS, LAT_FRAMES, LAT_SENT, LAT_RECEIVED, LAT_RESYNCS, LAT_COUNTERS };

extern volatile int lat_state;   /* -1: not asked yet, 0 off, 1 on */
extern volatile u64 lat_pad_read_ns;   /* when the game last read a pad (rt_now_ns), 0 before */
int lat_probe(void);
static inline int lat_on(void) {
    int s = lat_state;
    return s > 0 || (s < 0 && lat_probe());
}

void lat_add(int hist, u64 ns);
void lat_count(int counter);
void lat_gap(int hist, u64 key);                 /* the time since the last call with this key, into the histogram */
void lat_pad_read(u64 published_ns);             /* the game read a pad whose state was published at published_ns */
void lat_frame_queued(u32 pending);              /* the game queued a frame; `pending` frames now wait (this one included) */
void lat_frame_shown(u64 queued_ns, u64 pad_ns); /* a frame was handed to the display */
void lat_tick_signalled(void);                   /* the vsync event was signalled */
void lat_report(void);                           /* from the vsync thread, every tick: prints every few seconds */
