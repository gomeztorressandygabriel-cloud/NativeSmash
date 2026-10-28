/* Netplay without rollback ("lockstep"), and the check that makes it possible (docs/NETPLAY.md).
 *
 * The game's local VS, played on two PCs: each runs its own game, player 1 the host's pad and player
 * 2 the other's, and both games advance frame by frame together. A frame begins at the game's input
 * step (7103667300, the main loop's, once a frame): this PC's pad is read and sent, to be used DELAY
 * frames later on both PCs; the step then waits until the other PC's pad for this frame has arrived.
 * With the same pads on the same frames, the same game computes the same battle on both - if
 * everything else it reads is the same too:
 *   - the system tick the main thread reads is a clock of frames (t0, agreed at the start, plus a
 *     sixtieth of a second a frame), not the PC's;
 *   - both games start together, at their first input step (each waits there for the other);
 *   - what the game loads in the background must not change what the frames compute.
 * The last is the open question, and the reason for the check below: if two runs of the same game
 * with the same pads frame by frame compute the same battle, two PCs will.
 *
 * The check: every frame whose simulation runs (71037246a0), the pages the game writes while it runs
 * are hashed. SSBU_LOCKSTEP=record writes a run's pads and hashes to a file (SSBU_LOCKSTEP_FILE,
 * lockstep.bin by default); SSBU_LOCKSTEP=replay plays a later run with the file's pads, frame by
 * frame, and compares its hashes with the file's: where the two runs part, and how. In netplay the two
 * PCs compare theirs every 60 frames. (Some pages other threads write at their own pace - sound,
 * effects - differ every frame on their own; a battle that parts shows as many more.)
 *
 * Netplay: SSBU_NETPLAY=host (player 1; waits on UDP port SSBU_NETPLAY_PORT, 7810 by default) or
 * SSBU_NETPLAY=join with SSBU_NETPLAY_PEER=<host's address>[:port] (player 2). SSBU_NETPLAY_DELAY:
 * frames of delay, the host's counts (2 by default). The host's router must let UDP to that port in,
 * or both PCs be on one network (a LAN, or a VPN such as ZeroTier or Tailscale). Both start the game
 * at the same time: each waits at its first frame for the other (SSBU_NETPLAY_WAIT seconds, 300). If
 * the other stops answering for 15 s, netplay ends: player 2 stands still. (Each game also says it is alive from a thread of its own
 * every 250 ms, so a game stopped a while on its side - the shader loading screen at the start, a long compile - is waited for.)
 *
 * Through the online lobby instead (no VPN, no port to open): SSBU_NETPLAY_ROOM=<code>, with SSBU_NET_RELAY=<the lobby> and
 * SSBU_NET_ROOM=<a private lobby code> (the launcher sets the three). The host makes a room of the runtime's own in the lobby
 * (hle_net.c: ldn_direct_host), the other player finds it by its code and joins it, the lobby introduces the two, and the packets
 * below go between their virtual addresses (net_send) - straight from one PC to the other where their routers let them, else
 * through the lobby. Then the same as above, with no time limit on the wait (SSBU_NETPLAY_WAIT still sets one).
 *
 * Every line it writes to the log starts with [netplay] or [lockstep]. */
#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define closesocket close
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_lockstep.h"
#include "settings.h"
#include "net_rbtest.h"
#include "net.h"

volatile int lks_state = -1;

#ifdef __APPLE__
static long long gfx_watch_pages(const u64 *base, const u64 *size, u32 n, u64 *seen, void (*each)(u64 page, void *user), void *user) {
    (void)base; (void)size; (void)n; (void)seen; (void)each; (void)user;
    return -2;
}
#else
long long gfx_watch_pages(const u64 *base, const u64 *size, u32 n, u64 *seen, void (*each)(u64 page, void *user), void *user);   /* (gpu_d3d11.c) */
#endif

#define PAGE 0x1000ull
#define RING 256                    /* frames of pads kept */
#define MAX_PAGES 8192              /* pages hashed a frame */
#define MAX_REGIONS 4096
#define TICKS_PER_FRAME 320000ull   /* 19.2 MHz / 60 */
#define HASH_EVERY 60
#define MAXSEND 48                  /* frames of pads in one packet at most (a player that fell behind is sent what it lacks) */
#define REDUNDANCY 8                /* frames of pads each packet repeats (a lost packet costs nothing) */
#define MAGIC 0x4B4C534Eu           /* "NSLK" */
#define VERSION 5                   /* (2: up to four players, the sender in every packet; 5: HELLO says the joiner's clock) */

enum { M_OFF, M_RECORD, M_REPLAY, M_HOST, M_JOIN };

static int mode, started, stopped, delay = 2;
static DWORD main_thread;
static u32 frame;                   /* the frame now (the input step's count since the start) */
static u32 frames_max = 15000;      /* (record) */
static u64 t0, tick_reads;
/* The game's other threads see this PC's clock moved to the same base as the main thread's clock of frames (t0 is the host's,
 * or the later of the PCs'): a PC's own clock is how long it has been on, and the movie player, for one, mixes the main thread's
 * times with another thread's - with two bases hours apart, it waited for hours (the joiner's game stood still in the opening
 * movie). */
static i64 other_offset;
static LksInput in_of[4][RING];     /* the players' pads, by frame */
static u32 have[4];                 /* frames known: 0 .. have-1 */
/* Rollback (SSBU_NETPLAY_ROLLBACK=1, with the rewind test's per-frame saves: net_rbtest.c): a frame whose pads from another player
 * have not come yet is not waited for - it is played with a guess (that player's last pads), noted here; when the real ones come and
 * differ, the game goes back to before that frame and plays it again with them (rbt_net_rollback), the main loop's own replay. */
static int rb_on;                   /* (on when the per-frame saves are going: rbt_net_ready) */
#define MAX_GUESS 10                /* frames played on a guess at most (netplay keeps 12 saves) */
static LksInput guessed_with[4][RING];
static u8 guessed[4][RING];
static u32 mispredict_from = ~0u;   /* the first frame played with a guess that turned out wrong */
static u64 ring_of[RING];           /* the per-frame save each frame's input step comes after */
static u64 rollbacks, rolled_frames, guesses, guesses_wrong;
static int me;                      /* this PC's player: 0 host / record / replay, 1.. a joiner */

/* ---- hashes of the pages the simulation writes ----------------------------------------- */
typedef struct PageHash { u32 page; u32 pad; u64 hash; } PageHash;   /* (page: address >> 12) */
static PageHash cur[MAX_PAGES];
static u32 ncur, sim_frames;
static u64 region_base[MAX_REGIONS], region_size[MAX_REGIONS], alias_base[MAX_REGIONS], alias_size[MAX_REGIONS];
static u32 nalias;
static u64 sim_seen;
static int sim_open;

static void read_regions(void) {
    u32 n = kernel_memory_regions(region_base, region_size, MAX_REGIONS);
    if (!n) return;
    nalias = 0;
    for (u32 i = 0; i < n; i++)
        if (region_base[i] >= ALIAS_BASE && region_base[i] + region_size[i] <= ALIAS_BASE + ALIAS_SIZE) {
            alias_base[nalias] = region_base[i]; alias_size[nalias] = region_size[i]; nalias++;
        }
}

static u64 page_hash(const u8 *p) {
    u64 h = 0x9E3779B97F4A7C15ull;
    for (u32 i = 0; i < PAGE; i += 8) {
        u64 w;
        memcpy(&w, p + i, 8);
        h = (h ^ w) * 0xFF51AFD7ED558CCDull;
        h ^= h >> 32;
    }
    return h;
}

static void collect(u64 page, void *user) {
    (void)user;
    if (ncur < MAX_PAGES) cur[ncur++] = (PageHash){(u32)(page >> 12), 0, page_hash(P(u8, page))};
}

static int by_page(const void *a, const void *b) {
    u32 x = ((const PageHash *)a)->page, y = ((const PageHash *)b)->page;
    return x < y ? -1 : x > y;
}

static long long watch(u64 *seen, void (*each)(u64, void *)) {   /* (the renderer's lock may be busy a moment) */
    for (int tries = 0; tries < 200; tries++) {
        long long n = gfx_watch_pages(alias_base, alias_size, nalias, seen, each, NULL);
        if (n != -1) return n;
        Sleep(1);
    }
    return -1;
}

/* Two frames' lists (sorted): how many pages both have with the same hash, with another, and in one only. */
typedef struct Diff { u32 same, differ, only_a, only_b; u32 first[4]; u32 nfirst; } Diff;
static Diff compare(const PageHash *a, u32 na, const PageHash *b, u32 nb) {
    Diff d = {0};
    u32 i = 0, j = 0;
    while (i < na || j < nb) {
        if (j == nb || (i < na && a[i].page < b[j].page)) { d.only_a++; i++; }
        else if (i == na || b[j].page < a[i].page) { d.only_b++; j++; }
        else {
            if (a[i].hash == b[j].hash) d.same++;
            else { d.differ++; if (d.nfirst < 4) d.first[d.nfirst++] = a[i].page; }
            i++; j++;
        }
    }
    return d;
}

static void print_diff(const char *tag, u32 f, Diff d, const char *a, const char *b) {
    char line[512];
    int n = snprintf(line, sizeof line, "[%s] frame %u: pages the simulation wrote: same %u, different %u, only %s %u, only %s %u", tag, f, d.same, d.differ,
                     a, d.only_a, b, d.only_b);
    for (u32 k = 0; k < d.nfirst; k++) n += snprintf(line + n, sizeof line - n, "%s%#llx", k ? " " : "; different: ", (unsigned long long)d.first[k] << 12);
    fprintf(stderr, "%s\n", line);
}

/* Over a whole run: how often each page differed (pages that differ every frame are noise; a battle
 * that parts adds more and more). */
#define MAX_TALLY 512
static struct { u32 page, count; } tally[MAX_TALLY];
static u32 ntally, frames_compared, frames_clean, first_bad = ~0u;
static void tally_diffs(const PageHash *a, u32 na, const PageHash *b, u32 nb) {
    u32 i = 0, j = 0;
    while (i < na && j < nb) {
        if (a[i].page < b[j].page) i++;
        else if (b[j].page < a[i].page) j++;
        else {
            if (a[i].hash != b[j].hash) {
                u32 k = 0;
                while (k < ntally && tally[k].page != a[i].page) k++;
                if (k == ntally && ntally < MAX_TALLY) tally[ntally++].page = a[i].page;
                if (k < MAX_TALLY) tally[k].count++;
            }
            i++; j++;
        }
    }
}

static void print_tally(const char *tag) {
    fprintf(stderr, "[%s] %u frames compared, %u with every page the same; the first that was not: %d\n", tag, frames_compared, frames_clean,
            first_bad == ~0u ? -1 : (int)first_bad);
    for (int shown = 0; shown < 12; shown++) {
        u32 best = 0, at = ~0u;
        for (u32 k = 0; k < ntally; k++) if (tally[k].count > best) { best = tally[k].count; at = k; }
        if (at == ~0u) break;
        fprintf(stderr, "[%s]   page %#llx differed in %u frames\n", tag, (unsigned long long)tally[at].page << 12, best);
        tally[at].count = 0;
    }
}

/* ---- record and replay: a file of frames { frame u32, pads 2 x LksInput, pages u32, pages x PageHash } */
static FILE *file;
static PageHash want[MAX_PAGES];
static u32 nwant;
static int want_frame_hashes;

static int sim_ran;                 /* (this frame) */

static void write_frame(u32 f) {     /* (record) frame f, now that it is over */
    u32 n = sim_ran ? ncur : 0;
    LksInput pads[2] = {in_of[0][f % RING], in_of[1][f % RING]};
    fwrite(&f, 4, 1, file);
    fwrite(pads, sizeof pads, 1, file);
    fwrite(&n, 4, 1, file);
    fwrite(cur, sizeof *cur, n, file);
}

static int read_frame(u32 f) {   /* (replay) frame f's pads, and the hashes its simulation should make */
    u32 at, n;
    LksInput pads[2];
    if (fread(&at, 4, 1, file) != 1 || fread(pads, sizeof pads, 1, file) != 1 || fread(&n, 4, 1, file) != 1 || at != f || n > MAX_PAGES ||
        fread(want, sizeof *want, n, file) != n)
        return 0;
    in_of[0][f % RING] = pads[0];
    in_of[1][f % RING] = pads[1];
    have[0] = have[1] = f + 1;
    nwant = n;
    want_frame_hashes = n > 0;
    return 1;
}

/* ---- netplay ----------------------------------------------------------------------------
 * Up to MAXP players. The host is player 1 (index 0); the others get an index from the host as they come
 * (HELLO -> WELCOME), and once all of them are there the host starts the game for everyone (START, with
 * every player's address, so that each one sends its pads to all the others). Every packet says who sent
 * it (its byte 6). Through the online lobby (SSBU_NETPLAY_ROOM) any number up to MAXP; to an address
 * (SSBU_NETPLAY_PEER), two.
 *
 * Besides the pads and the hashes:
 *   - ALIVE / PONG every 250 ms from a thread of its own: a game held a while (its shader loading screen)
 *     is waited for, and each PC knows its ping to the others;
 *   - the delay: each player's own (its pads go DELAY frames ahead). With SSBU_NETPLAY_DELAY=auto it
 *     follows the ping and its jitter (up one at once when it is short, down one after a quiet while);
 *   - pacing: a PC frames ahead of the slowest one waits a millisecond or two each frame, so that both go
 *     at the slower one's pace smoothly, instead of stopping dead when its pads run out;
 *   - the save: through the lobby, the host's save goes to the others before their game starts
 *     (lks_preboot: SAVE_REQ / SAVE_INFO / SAVE_GET / SAVE_PART), so every game starts from it;
 *   - while the opening movie plays, the host's pad presses A now and then (sent like any pad: every game
 *     skips it the same way);
 *   - a window title and a line on screen say what is happening (lks_status_text), and a probable desync
 *     (the hashes parting far more than their usual noise) is said there. */
#define MAXP 4
enum { P_HELLO = 1, P_WELCOME, P_INPUT, P_HASH, P_BYE, P_ALIVE, P_PONG, P_START, P_SAVE_REQ, P_SAVE_INFO, P_SAVE_GET, P_SAVE_PART };
#define HEAD 7   /* magic u32, version u8, type u8, sender u8 */

typedef struct Peer {
    int known;
    u32 vip;                  /* (through the lobby) its virtual address */
    struct sockaddr_in addr;  /* (to an address) */
    u64 last_heard;
    int ready;                /* its game has finished loading its shaders (it says so in its ALIVE) */
    u32 frame;                /* the frame it says it is at */
    u32 acked;                /* how many of this PC's pads it says it has */
    double rtt, jitter;       /* ms, smoothed */
    int rtt_n;
} Peer;

static SOCKET sock = INVALID_SOCKET;
static Peer peer[MAXP];
static int nplayers = 2;          /* (the host's number, told to the others) */
static int welcomed, start_heard, start_sent;
volatile int lks_gfx_loading;   /* (gpu_d3d11.c) the start's shader loading screen is still up: this game is not ready */
static int local_ready(void) { return !lks_gfx_loading; }
static u64 nonce;
static int delay_auto, delay_quiet;
static u32 stalls;                /* frames this PC waited for another one's pads (the last 60) */
static volatile u32 desync_frame = ~0u;
static int compare_with;          /* the player whose hashes this PC compares with its own (the host: player 2) */
static PageHash mine_at[4][MAX_PAGES / 4];
static u32 nmine_at[4], mine_frame[4];
static PageHash theirs[MAX_PAGES / 4];
static u32 ntheirs, theirs_frame = ~0u, theirs_total;
volatile u64 lks_movie_ns;        /* (hle_movie.c) when the movie player last worked */

static void put32(u8 *p, u32 v) { memcpy(p, &v, 4); }
static u32 get32(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }
static void put64(u8 *p, u64 v) { memcpy(p, &v, 8); }
static u64 get64(const u8 *p) { u64 v; memcpy(&v, p, 8); return v; }

/* Through the online lobby: the packets for this game (lks_deliver, on the relay's thread), kept until the main thread reads them. */
static int via_lobby;
static char room_code[64];
static u16 lobby_port = 7810;
#define LOBBY_QUEUE 512
#define LOBBY_PACKET 1400
static struct { u32 src; u16 port, len; u8 data[LOBBY_PACKET]; } lobby_queue[LOBBY_QUEUE];
static u32 lq_head, lq_tail;
static SRWLOCK lq_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE lq_cv = CONDITION_VARIABLE_INIT;

static int header(u8 *p, int type) { put32(p, MAGIC); p[4] = VERSION; p[5] = (u8)type; p[6] = (u8)(welcomed || mode == M_HOST ? me : 0xFF); return HEAD; }

static void send_raw_vip(u32 vip, const u8 *p, int n) { net_send(net_local_ip(), lobby_port, vip, lobby_port, p, (u32)n); }
static void send_to(int i, const u8 *p, int n) {
    if (i < 0 || i >= MAXP || i == me || !peer[i].known) return;
    if (via_lobby) { if (peer[i].vip) send_raw_vip(peer[i].vip, p, n); }
    else sendto(sock, (const char *)p, n, 0, (const struct sockaddr *)&peer[i].addr, sizeof peer[i].addr);
}
static void send_all(const u8 *p, int n) { for (int i = 0; i < MAXP; i++) send_to(i, p, n); }

/* ---- the save, from the host to the others, before their games start ---- */
#define SAVE_PART_SIZE 1024
#define SAVE_MAX (96u << 20)
static SRWLOCK save_lock = SRWLOCK_INIT;
static u8 *save_blob;             /* "NSS1", count u32, then per file: path length u16, path, size u32, bytes */
static u32 save_size;
static u64 save_hash;
static u8 *save_have;             /* (joiner) which parts came */
static u32 save_parts, save_got;
static int save_info_heard;

static u64 fnv64(const u8 *p, u32 n) { u64 h = 0xcbf29ce484222325ull; for (u32 i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ull; return h; }

static const char *data_dir(void) { const char *r = getenv("SSBU_DATA_ROOT"); return r && *r ? r : NULL; }

typedef struct Blob { u8 *p; u32 n, cap, count; } Blob;
static int blob_add(Blob *b, const void *d, u32 n) {
    if (b->n + n > SAVE_MAX) return 0;
    if (b->n + n > b->cap) {
        u32 cap = b->cap ? b->cap * 2 : 1u << 20;
        while (cap < b->n + n) cap *= 2;
        u8 *q = realloc(b->p, cap);
        if (!q) return 0;
        b->p = q; b->cap = cap;
    }
    memcpy(b->p + b->n, d, n);
    b->n += n;
    return 1;
}
static void save_walk(Blob *b, const char *base, const char *rel) {   /* every file under base/rel, by its path from base */
    char pattern[1024];
    snprintf(pattern, sizeof pattern, "%s/%s%s*", base, rel, *rel ? "/" : "");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        char sub[600];
        snprintf(sub, sizeof sub, "%s%s%s", rel, *rel ? "/" : "", fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { save_walk(b, base, sub); continue; }
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", base, sub);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        u16 pl = (u16)strlen(sub);
        u32 sz = size > 0 ? (u32)size : 0;
        u8 *data = sz ? malloc(sz) : NULL;
        if ((!sz || (data && fread(data, 1, sz, f) == sz)) && blob_add(b, &pl, 2) && blob_add(b, sub, pl) && blob_add(b, &sz, 4) && (!sz || blob_add(b, data, sz)))
            b->count++;
        free(data);
        fclose(f);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
static void save_snapshot(void) {   /* (host) its save as it is now, for the others */
    const char *root = data_dir();
    if (!root) { fprintf(stderr, "[netplay] no SSBU_DATA_ROOT: the save is not sent (each player uses their own)\n"); return; }
    char base[1024];
    snprintf(base, sizeof base, "%s/save", root);
    Blob b = {0};
    blob_add(&b, "NSS1", 4);
    u32 zero = 0;
    blob_add(&b, &zero, 4);
    save_walk(&b, base, "");
    put32(b.p + 4, b.count);
    save_blob = b.p;
    save_size = b.n;
    save_hash = fnv64(b.p, b.n);
    fprintf(stderr, "[netplay] the save to send: %u files, %u KB\n", b.count, b.n >> 10);
}
static int mkdirs(const char *path) {   /* the folders of a file's path */
    char tmp[1200];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *s = tmp + 1; *s; s++)
        if (*s == '/' || *s == '\\') { char c = *s; *s = 0; CreateDirectoryA(tmp, NULL); *s = c; }
    return 1;
}
static int save_write(void) {   /* (joiner) the host's save into this game's data folder */
    const char *root = data_dir();
    if (!root || save_size < 8 || memcmp(save_blob, "NSS1", 4)) return 0;
    u32 count = get32(save_blob + 4), at = 8, written = 0;
    for (u32 i = 0; i < count; i++) {
        if (at + 2 > save_size) return 0;
        u16 pl; memcpy(&pl, save_blob + at, 2); at += 2;
        if (at + pl + 4 > save_size || pl == 0 || pl > 500) return 0;
        char rel[512]; memcpy(rel, save_blob + at, pl); rel[pl] = 0; at += pl;
        u32 sz = get32(save_blob + at); at += 4;
        if (at + sz > save_size || strstr(rel, "..")) return 0;
        char path[1200];
        snprintf(path, sizeof path, "%s/save/%s", root, rel);
        mkdirs(path);
        FILE *f = fopen(path, "wb");
        if (!f) return 0;
        fwrite(save_blob + at, 1, sz, f);
        fclose(f);
        at += sz;
        written++;
    }
    fprintf(stderr, "[netplay] the host's save is here: %u files\n", written);
    return 1;
}

/* (relay thread) the save's packets, answered or kept at once: they come before the game runs */
static void on_save_packet(u32 src, const u8 *p, int n) {
    switch (p[5]) {
    case P_SAVE_REQ:   /* (host) send the size and hash */
        if (mode != M_HOST || !save_blob) return;
        {
            u8 q[32];
            int k = header(q, P_SAVE_INFO);
            put32(q + k, save_size); k += 4;
            put64(q + k, save_hash); k += 8;
            send_raw_vip(src, q, k);
        }
        return;
    case P_SAVE_GET:   /* (host) parts: first u32, count u8 */
        if (mode != M_HOST || !save_blob || n < HEAD + 5) return;
        {
            u32 first = get32(p + HEAD), count = p[HEAD + 4];
            for (u32 i = 0; i < count; i++) {
                u32 part = first + i, off = part * SAVE_PART_SIZE;
                if (off >= save_size) break;
                u32 len = save_size - off < SAVE_PART_SIZE ? save_size - off : SAVE_PART_SIZE;
                u8 q[HEAD + 8 + SAVE_PART_SIZE];
                int k = header(q, P_SAVE_PART);
                put32(q + k, part); k += 4;
                put32(q + k, len); k += 4;
                memcpy(q + k, save_blob + off, len); k += (int)len;
                send_raw_vip(src, q, k);
            }
        }
        return;
    case P_SAVE_INFO:   /* (joiner) */
        if (mode != M_JOIN || n < HEAD + 12) return;
        AcquireSRWLockExclusive(&save_lock);
        if (!save_info_heard) {
            u32 size = get32(p + HEAD);
            if (size <= SAVE_MAX) {
                save_size = size;
                save_hash = get64(p + HEAD + 4);
                save_parts = (size + SAVE_PART_SIZE - 1) / SAVE_PART_SIZE;
                save_blob = calloc(1, size ? size : 1);
                save_have = calloc(1, save_parts ? save_parts : 1);
                save_info_heard = save_blob && save_have;
            }
        }
        ReleaseSRWLockExclusive(&save_lock);
        return;
    case P_SAVE_PART:   /* (joiner) */
        if (mode != M_JOIN || n < HEAD + 8) return;
        AcquireSRWLockExclusive(&save_lock);
        if (save_info_heard) {
            u32 part = get32(p + HEAD), len = get32(p + HEAD + 4), off = part * SAVE_PART_SIZE;
            if (part < save_parts && !save_have[part] && off + len <= save_size && n >= HEAD + 8 + (int)len) {
                memcpy(save_blob + off, p + HEAD + 8, len);
                save_have[part] = 1;
                save_got++;
            }
        }
        ReleaseSRWLockExclusive(&save_lock);
        return;
    }
}

static void on_packet(const u8 *p, int n, u32 src_vip, const struct sockaddr_in *from);
int lks_deliver(u32 src_ip, u16 src_port, u16 dst_port, const void *data, u32 len) {
    if (!via_lobby || dst_port != lobby_port) return 0;
    const u8 *p = data;
    if (len >= HEAD && get32(p) == MAGIC && p[4] == VERSION && p[5] >= P_SAVE_REQ) { on_save_packet(src_ip, p, (int)len); return 1; }
    /* (the ping, here: through the main thread's queue it would count the frames the game took too) */
    if (len >= HEAD && get32(p) == MAGIC && p[4] == VERSION && (p[5] == P_ALIVE || p[5] == P_PONG)) { on_packet(p, (int)len, src_ip, NULL); return 1; }
    if (len <= LOBBY_PACKET) {
        AcquireSRWLockExclusive(&lq_lock);
        if (lq_tail - lq_head < LOBBY_QUEUE) {   /* (full: dropped, as a network would; the next packet repeats the pads) */
            u32 k = lq_tail++ % LOBBY_QUEUE;
            lobby_queue[k].src = src_ip;
            lobby_queue[k].port = src_port;
            lobby_queue[k].len = (u16)len;
            memcpy(lobby_queue[k].data, data, len);
        }
        ReleaseSRWLockExclusive(&lq_lock);
        WakeAllConditionVariable(&lq_cv);
    }
    return 1;
}

/* (hle_net.c) Whether a lobby packet from this address is the VS netplay's own: the game's own nn::ldn calls may have left the room's network. */
int lks_peer_ip(u32 ip) {
    if (!via_lobby || (mode != M_HOST && mode != M_JOIN)) return 0;
    if (mode == M_HOST) return 1;   /* (the room's code is private: whoever sent it is in the room) */
    for (int i = 0; i < MAXP; i++) if (peer[i].known && peer[i].vip == ip) return 1;
    return 0;
}

/* ---- the packets ---- */
static void send_inputs(void) {   /* this PC's pads to each player: from the first one it says it lacks (at least the last REDUNDANCY), and what this PC has of its pads */
    for (int i = 0; i < MAXP; i++) {
        if (i == me || !peer[i].known) continue;
        u8 p[HEAD + 16 + MAXSEND * sizeof(LksInput)];
        int n = header(p, P_INPUT);
        u32 last = have[me], first = last > REDUNDANCY ? last - REDUNDANCY : 0;
        if (peer[i].acked < first && last - peer[i].acked <= MAXSEND) first = peer[i].acked;   /* (it fell behind - a slow load, packets dropped: what it lacks) */
        else if (peer[i].acked < first && last > MAXSEND) first = last - MAXSEND;
        put32(p + n, frame); n += 4;
        put32(p + n, first); n += 4;
        put32(p + n, have[i]); n += 4;   /* (ack: how many of its pads this PC has) */
        p[n++] = (u8)(last - first);
        for (u32 f = first; f < last; f++) { memcpy(p + n, &in_of[me][f % RING], sizeof(LksInput)); n += sizeof(LksInput); }
        send_to(i, p, n);
    }
}

static void send_hashes(u32 f, const PageHash *list, u32 count) {   /* in pieces of 100, to the player compared with */
    for (u32 off = 0; off < count || off == 0; off += 100) {
        u8 p[HEAD + 13 + 100 * 12 + 8];
        int n = header(p, P_HASH);
        u32 k = count - off < 100 ? count - off : 100;
        put32(p + n, f); n += 4;
        put32(p + n, count); n += 4;
        put32(p + n, off); n += 4;
        p[n++] = (u8)k;
        for (u32 i = 0; i < k; i++) { put32(p + n, list[off + i].page); memcpy(p + n + 4, &list[off + i].hash, 8); n += 12; }
        send_to(compare_with, p, n);
        if (!count) break;
    }
}

static void their_hashes_done(void) {
    static u32 base_n, base_sum;
    static int over;
    for (int s = 0; s < 4; s++)
        if (mine_frame[s] == theirs_frame) {
            qsort(theirs, ntheirs, sizeof *theirs, by_page);
            Diff d = compare(mine_at[s], nmine_at[s], theirs, ntheirs);
            frames_compared++;
            if (!d.differ && !d.only_a && !d.only_b) frames_clean++;
            else if (first_bad == ~0u) first_bad = theirs_frame;
            tally_diffs(mine_at[s], nmine_at[s], theirs, ntheirs);
            print_diff("netplay", theirs_frame, d, "here", "there");
            /* A desync: the pages written differently far above their usual noise (sound and effects write some at their own pace), three times running. */
            if (base_n < 10) { base_sum += d.differ; base_n++; }
            else if (d.differ > 200 && d.differ > 4 * (base_sum / base_n) + 60) {
                if (++over >= 3 && desync_frame == ~0u) {
                    desync_frame = theirs_frame;
                    fprintf(stderr, "[netplay] probable desync from frame %u: %u pages differ (usually ~%u)\n", theirs_frame, d.differ, base_sum / base_n);
                }
            } else over = 0;
            return;
        }
}

static int player_of(u32 vip, const struct sockaddr_in *addr) {   /* the index of a known player at this address, else -1 */
    for (int i = 0; i < MAXP; i++) {
        if (!peer[i].known || i == me) continue;
        if (via_lobby ? peer[i].vip == vip : (peer[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr && peer[i].addr.sin_port == addr->sin_port)) return i;
    }
    return -1;
}

static void on_packet(const u8 *p, int n, u32 src_vip, const struct sockaddr_in *from) {
    if (n < HEAD || get32(p) != MAGIC || p[4] != VERSION) return;
    int who = p[6] < MAXP ? p[6] : -1;
    u64 now = rt_now_ns();
    if (who >= 0 && who != me && peer[who].known) peer[who].last_heard = now;
    switch (p[5]) {
    case P_HELLO: {   /* (host) a player comes: an index of its own, the clock and the number of players */
        if (mode != M_HOST || n < HEAD + 8) return;
        int i = player_of(src_vip, from);
        if (i < 0) {
            if (start_sent) return;   /* (the game has started: no one else comes in) */
            for (int k = 1; k < nplayers && i < 0; k++) if (!peer[k].known) i = k;
            if (i < 0) return;
            memset(&peer[i], 0, sizeof peer[i]);
            peer[i].known = 1;
            peer[i].vip = src_vip;
            if (from) peer[i].addr = *from;
            peer[i].last_heard = now;
            fprintf(stderr, "[netplay] player %d joined (%u.%u.%u.%u)\n", i + 1, src_vip >> 24, src_vip >> 16 & 255, src_vip >> 8 & 255, src_vip & 255);
        }
        if (!t0) {   /* the clock of frames starts at the later of the two PCs' clocks (2 s on): neither goes back */
            u64 mine = rt_clock_ns() * 12 / 625, theirs = n >= HEAD + 16 ? get64(p + HEAD + 8) : 0;
            t0 = (mine > theirs ? mine : theirs) + 2 * 19200000ull;
        }
        u8 q[64];
        int k = header(q, P_WELCOME);
        memcpy(q + k, p + HEAD, 8); k += 8;   /* (their nonce) */
        put64(q + k, t0); k += 8;
        q[k++] = (u8)i;
        q[k++] = (u8)nplayers;
        send_to(i, q, k);
        break;
    }
    case P_WELCOME:   /* (joiner) */
        if (mode != M_JOIN || n < HEAD + 18 || welcomed) return;
        if (get64(p + HEAD) != nonce || p[HEAD + 16] == 0 || p[HEAD + 16] >= MAXP) return;
        t0 = get64(p + HEAD + 8);
        me = p[HEAD + 16];
        nplayers = p[HEAD + 17] >= 2 && p[HEAD + 17] <= MAXP ? p[HEAD + 17] : 2;
        welcomed = 1;
        peer[0].last_heard = now;
        fprintf(stderr, "[netplay] joined: player %d of %d, %d frames of delay to start with\n", me + 1, nplayers, delay);
        break;
    case P_START:   /* (joiner) every player's address: send to all of them */
        if (mode != M_JOIN || !welcomed || n < HEAD + 1 + MAXP * 4 || start_heard) return;
        nplayers = p[HEAD] >= 2 && p[HEAD] <= MAXP ? p[HEAD] : nplayers;
        for (int i = 1; i < nplayers; i++) {
            if (i == me) continue;
            u32 v = get32(p + HEAD + 1 + i * 4);
            if (v && via_lobby) { peer[i].known = 1; peer[i].vip = v; peer[i].last_heard = now; }
        }
        start_heard = 1;
        fprintf(stderr, "[netplay] start: %d players\n", nplayers);
        break;
    case P_INPUT: {
        if (who < 0 || who == me || n < HEAD + 13) return;
        u32 at = get32(p + HEAD), first = get32(p + HEAD + 4), ack = get32(p + HEAD + 8), count = p[HEAD + 12];
        if (n < HEAD + 13 + (int)(count * sizeof(LksInput))) return;
        peer[who].frame = at;
        if (ack > peer[who].acked) peer[who].acked = ack;
        for (u32 i = 0; i < count; i++) {
            u32 f = first + i;
            if (f != have[who] || f >= have[who] + RING / 2) continue;
            memcpy(&in_of[who][f % RING], p + HEAD + 13 + i * sizeof(LksInput), sizeof(LksInput));
            have[who] = f + 1;
            if (guessed[who][f % RING]) {   /* (played on a guess: was it right?) */
                guessed[who][f % RING] = 0;
                if (memcmp(&guessed_with[who][f % RING], &in_of[who][f % RING], sizeof(LksInput))) {
                    guesses_wrong++;
                    if (f < mispredict_from) mispredict_from = f;
                }
            }
        }
        break;
    }
    case P_HASH: {
        if (who != compare_with || n < HEAD + 13) return;
        u32 f = get32(p + HEAD), total = get32(p + HEAD + 4), off = get32(p + HEAD + 8), k = p[HEAD + 12];
        if (n < HEAD + 13 + (int)k * 12 || total > MAX_PAGES / 4) return;
        if (f != theirs_frame) { theirs_frame = f; ntheirs = 0; theirs_total = total; }
        if (off != ntheirs) return;   /* (a piece lost or out of order: this frame is not compared) */
        for (u32 i = 0; i < k; i++) { theirs[ntheirs].page = get32(p + HEAD + 13 + i * 12); memcpy(&theirs[ntheirs].hash, p + HEAD + 17 + i * 12, 8); ntheirs++; }
        if (ntheirs == theirs_total) their_hashes_done();
        break;
    }
    case P_ALIVE:   /* answer, so that the sender knows its ping */
        if (who < 0 || who == me || n < HEAD + 8) return;
        if (n >= HEAD + 9 && peer[who].known) peer[who].ready = p[HEAD + 8];
        {
            u8 q[HEAD + 8];
            int k = header(q, P_PONG);
            memcpy(q + k, p + HEAD, 8); k += 8;
            send_to(who, q, k);
        }
        break;
    case P_PONG:
        if (who < 0 || who == me || n < HEAD + 8) return;
        {
            double rtt = (double)(now - get64(p + HEAD)) / 1e6;
            Peer *q = &peer[who];
            if (rtt < 0 || rtt > 5000) break;
            if (!q->rtt_n++) { q->rtt = rtt; q->jitter = 0; }
            else { q->jitter = q->jitter * 0.9 + (rtt > q->rtt ? rtt - q->rtt : q->rtt - rtt) * 0.1; q->rtt = q->rtt * 0.9 + rtt * 0.1; }
        }
        break;
    case P_BYE:
        if (who < 0) return;
        fprintf(stderr, "[netplay] player %d left\n", who + 1);
        peer[who].known = 0;
        stopped = 1;
        break;
    }
}

static void poll_net(int wait_ms) {   /* every packet waiting, after up to wait_ms for the first */
    if (via_lobby) {
        AcquireSRWLockExclusive(&lq_lock);
        if (lq_tail == lq_head && wait_ms > 0) SleepConditionVariableSRW(&lq_cv, &lq_lock, (DWORD)wait_ms, 0);
        while (lq_tail != lq_head) {
            static u8 p[LOBBY_PACKET];
            u32 k = lq_head++ % LOBBY_QUEUE;
            int n = lobby_queue[k].len;
            u32 src = lobby_queue[k].src;
            memcpy(p, lobby_queue[k].data, (size_t)n);
            ReleaseSRWLockExclusive(&lq_lock);
            on_packet(p, n, src, NULL);
            AcquireSRWLockExclusive(&lq_lock);
        }
        ReleaseSRWLockExclusive(&lq_lock);
        return;
    }
    for (;;) {
        fd_set r;
        FD_ZERO(&r);
        FD_SET(sock, &r);
        struct timeval tv = {0, wait_ms * 1000};
        if (select((int)sock + 1, &r, NULL, NULL, &tv) <= 0) return;
        u8 p[2048];
        struct sockaddr_in from;
        socklen_t len = sizeof from;
        int n = (int)recvfrom(sock, (char *)p, sizeof p, 0, (struct sockaddr *)&from, &len);
        if (n > 0) on_packet(p, n, 0, &from);
        wait_ms = 0;
    }
}

static int open_socket(u16 port) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) return 0;
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&a, sizeof a)) {
        fprintf(stderr, "[netplay] cannot use UDP port %u (another program has it?)\n", port);
        closesocket(sock);
        sock = INVALID_SOCKET;
        return 0;
    }
    return 1;
}

static int resolve(const char *text, u16 default_port, struct sockaddr_in *out) {
    char host[256];
    snprintf(host, sizeof host, "%s", text);
    u16 port = default_port;
    char *colon = strrchr(host, ':');
    if (colon) { *colon = 0; port = (u16)atoi(colon + 1); }
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, NULL, &hints, &res) || !res) return 0;
    *out = *(struct sockaddr_in *)res->ai_addr;
    out->sin_port = htons(port);
    freeaddrinfo(res);
    return 1;
}

/* ---- what is happening, for the window's title and the screen ---- */
static volatile int phase;   /* 0 off, 1 the lobby, 2 the room / waiting for players, 3 the save, 4 playing, 5 over */
static volatile char phase_note[96];
static int spanish(void) {
#ifdef _WIN32
    return (GetUserDefaultUILanguage() & 0x3FF) == 0x0A;
#else
    return 0;
#endif
}
void (*lks_window_status)(const char *text);   /* (gpu_d3d11.c) shows it in the window's title */

const char *lks_status_text(void) {
    static char text[200];
    if (mode != M_HOST && mode != M_JOIN) return NULL;
    int es = spanish(), joined = 0;
    for (int i = 0; i < MAXP; i++) joined += peer[i].known && i != me;
    double ping = 0, jit = 0;
    for (int i = 0; i < MAXP; i++) if (i != me && peer[i].known && peer[i].rtt_n) { if (peer[i].rtt > ping) ping = peer[i].rtt; if (peer[i].jitter > jit) jit = peer[i].jitter; }
    switch (phase) {
    case 1: snprintf(text, sizeof text, es ? "Online Direct: entrando al servidor de salas..." : "Online Direct: entering the lobby..."); break;
    case 2:
        if (mode == M_HOST) snprintf(text, sizeof text, es ? "Online Direct: sala %s - esperando jugadores (%d de %d)" : "Online Direct: room %s - waiting for players (%d of %d)", room_code, joined + 1, nplayers);
        else snprintf(text, sizeof text, es ? "Online Direct: buscando la sala %s..." : "Online Direct: looking for room %s...", room_code);
        break;
    case 3: snprintf(text, sizeof text, es ? "Online Direct: recibiendo el savegame del host (%u%%)" : "Online Direct: getting the host's save (%u%%)", save_parts ? save_got * 100 / save_parts : 0); break;
    case 4:
        if (desync_frame != ~0u) snprintf(text, sizeof text, es ? "Online Direct: posible desincronizacion desde el cuadro %u - reiniciad la partida" : "Online Direct: probable desync from frame %u - restart the game", desync_frame);
        else if (!started && !local_ready()) snprintf(text, sizeof text, es ? "Online Direct: cargando shaders - los demas esperan" : "Online Direct: loading shaders - the others wait");
        else if (!started) snprintf(text, sizeof text, es ? "Online Direct: esperando a que los demas carguen..." : "Online Direct: waiting for the others to load...");
        else snprintf(text, sizeof text, es ? "Online Direct: jugador %d de %d - ping %.0f ms - retraso %d%s" : "Online Direct: player %d of %d - ping %.0f ms - delay %d%s",
                      me + 1, nplayers, ping, delay, stalls > 6 ? (es ? " - tirones (la red o el otro PC)" : " - stuttering (the network or the other PC)") : "");
        break;
    case 5: snprintf(text, sizeof text, "Online Direct: %s", (const char *)phase_note); break;
    default: return NULL;
    }
    (void)jit;
    return text;
}

/* (its own thread, from the start) ALIVE every 250 ms whatever this game's main thread is doing, and the window's title */
static DWORD WINAPI alive_main(void *unused) {
    (void)unused;
    SetThreadDescription(GetCurrentThread(), L"netplay alive");
    char last[200] = "";
    for (;;) {
        if (stopped && phase != 5) { phase = 5; snprintf((char *)phase_note, sizeof phase_note, "%s", spanish() ? "red apagada" : "netplay off"); }
        if (!stopped) {
            u8 p[HEAD + 9];
            int k = header(p, P_ALIVE);
            put64(p + k, rt_now_ns()); k += 8;
            p[k++] = (u8)local_ready();
            send_all(p, k);
        }
        const char *s = lks_status_text();
        if (s && lks_window_status && strcmp(s, last)) { snprintf(last, sizeof last, "%s", s); lks_window_status(s); }
        Sleep(250);
    }
    return 0;
}

/* ---- before the game starts: the lobby, the room, and (joiners) the host's save ---- */
static void room_tick(int *in_lobby, int *room_ready, u64 *next_try) {
    if (*room_ready) return;
    if (!*in_lobby) {
        if (net_local_ip()) { *in_lobby = 1; phase = 2; fprintf(stderr, "[netplay] in the lobby as %u.%u.%u.%u\n", net_local_ip() >> 24, net_local_ip() >> 16 & 255, net_local_ip() >> 8 & 255, net_local_ip() & 255); }
        else if (net_unreachable() && rt_now_ns() >= *next_try) { fprintf(stderr, "[netplay] the lobby does not answer (internet? the lobby's address?)\n"); *next_try = rt_now_ns() + 30000000000ull; }
        return;
    }
    if (rt_now_ns() < *next_try) return;
    *next_try = rt_now_ns() + 2000000000ull;
    if (mode == M_HOST) *room_ready = ldn_direct_host(room_code, nplayers);
    else {
        u32 host = ldn_direct_join(room_code);
        if (host) { peer[0].known = 1; peer[0].vip = host; peer[0].last_heard = rt_now_ns(); *room_ready = 1; }
    }
}

static DWORD WINAPI host_room_main(void *unused) {   /* (host) the room made as soon as the lobby lets: the others find it while this game loads */
    (void)unused;
    SetThreadDescription(GetCurrentThread(), L"netplay room");
    int in_lobby = 0, ready = 0;
    u64 next = 0;
    while (!ready && !stopped) { room_tick(&in_lobby, &ready, &next); Sleep(50); }
    return 0;
}

void lks_preboot(void) {
    if (!lks_on()) return;
    if ((mode != M_HOST && mode != M_JOIN) || !via_lobby) return;
    phase = 1;
    CloseHandle(CreateThread(NULL, 0, alive_main, NULL, 0, NULL));
    if (mode == M_HOST) {
        save_snapshot();
        CloseHandle(CreateThread(NULL, 0, host_room_main, NULL, 0, NULL));
        return;
    }
    /* joiner: the room, then the host's save, before this game reads its own */
    int in_lobby = 0, ready = 0;
    u64 next = 0;
    while (!ready) { room_tick(&in_lobby, &ready, &next); Sleep(50); }
    if (!data_dir()) return;
    phase = 3;
    u64 next_req = 0, t_start = rt_now_ns();
    u32 cursor = 0;
    for (;;) {
        u64 now = rt_now_ns();
        AcquireSRWLockShared(&save_lock);
        int info = save_info_heard;
        u32 got = save_got, parts = save_parts;
        ReleaseSRWLockShared(&save_lock);
        if (!info) {
            if (now >= next_req) { u8 p[HEAD]; send_raw_vip(peer[0].vip, p, header(p, P_SAVE_REQ)); next_req = now + 1000000000ull; }
            if (now - t_start > 120000000000ull) { fprintf(stderr, "[netplay] the host did not send its save: this game uses its own\n"); break; }
            Sleep(20);
            continue;
        }
        if (got >= parts) break;
        if (now >= next_req) {   /* the next missing parts, 32 at a time */
            AcquireSRWLockShared(&save_lock);
            u32 tries = 0;
            while (tries++ < parts && save_have[cursor % parts]) cursor++;
            ReleaseSRWLockShared(&save_lock);
            u8 p[HEAD + 5];
            int k = header(p, P_SAVE_GET);
            u32 first = cursor % parts, count = parts - first < 32 ? parts - first : 32;
            put32(p + k, first); k += 4;
            p[k++] = (u8)count;
            send_raw_vip(peer[0].vip, p, k);
            cursor = first + count;
            next_req = now + 30000000ull;
        }
        Sleep(2);
    }
    if (save_info_heard && save_got >= save_parts) {
        if (fnv64(save_blob, save_size) == save_hash) save_write();
        else fprintf(stderr, "[netplay] the host's save came damaged: this game uses its own\n");
    }
}

/* At the first input step: everyone there. The host waits for all its players and starts them; a joiner says hello and waits for the start. */
/* SSBU_NETPLAY=fake: a second player that is not there, for trying rollback on one PC - host (player 1) with player 2's pads
 * made up here (a pattern that changes every few frames, from SSBU_NETPLAY_FAKE_FROM on, 0 before: the menus are player 1's) and
 * arriving SSBU_NETPLAY_FAKE_LAG frames late (3 by default, plus 0-2 of jitter), as through the network. No sockets. */
static int fake;
static u32 fake_lag = 3, fake_from = 3600;
static LksInput fake_pad(u32 f) {
    LksInput in = {0};
    if (f < fake_from) return in;
    u32 t = f - fake_from;
    if (t % 40 < 6) in.buttons |= 1;                                    /* (A, now and then) */
    in.lx = (t / 30) % 3 == 0 ? -32767 : (t / 30) % 3 == 1 ? 32767 : 0;  /* (the stick: left, right, still) */
    return in;
}
static void fake_deliver(void) {   /* (each input step) player 2's pads up to some frames ago, as P_INPUT gives them */
    u32 lag = rb_on ? fake_lag + (frame / 7) % 3 : 0, upto = frame > lag ? frame - lag : 0;   /* (before rollback is on: at once - lockstep would wait for ever) */
    while (have[1] <= upto) {
        u32 f = have[1];
        in_of[1][f % RING] = fake_pad(f);
        have[1] = f + 1;
        peer[1].frame = frame;
        peer[1].last_heard = rt_now_ns();
        if (guessed[1][f % RING]) {
            guessed[1][f % RING] = 0;
            if (memcmp(&guessed_with[1][f % RING], &in_of[1][f % RING], sizeof(LksInput))) {
                guesses_wrong++;
                if (f < mispredict_from) mispredict_from = f;
            }
        }
    }
}

static int connect_peer(void) {
    if (fake) { peer[1].known = 0; return 1; }
    const char *w = getenv("SSBU_NETPLAY_WAIT");
    double wait_s = w && *w ? atof(w) : via_lobby ? 3600.0 : 300.0;   /* (through the lobby: as long as the others take to start) */
    u64 until = rt_now_ns() + (u64)(wait_s * 1e9), next_hello = 0, next_note = 0, next_room = 0;
    int in_lobby = net_local_ip() != 0, room_ready = mode == M_HOST ? 0 : peer[0].known;
    if (mode == M_HOST && via_lobby) in_lobby = 1;   /* (its room is made by host_room_main) */
    phase = via_lobby ? 2 : 4;
    if (mode == M_HOST) fprintf(stderr, "[netplay] waiting for %d players (up to %.0f s)\n", nplayers - 1, wait_s);
    else fprintf(stderr, "[netplay] joining the host (up to %.0f s)\n", wait_s);
    while (rt_now_ns() < until && !stopped) {
        if (via_lobby && mode == M_JOIN && !room_ready) { room_tick(&in_lobby, &room_ready, &next_room); if (!room_ready) { Sleep(50); continue; } }
        u64 now = rt_now_ns();
        if (mode == M_JOIN && !welcomed && now >= next_hello) {
            u8 p[HEAD + 16];
            int n = header(p, P_HELLO);
            put64(p + n, nonce); n += 8;
            put64(p + n, rt_clock_ns() * 12 / 625); n += 8;   /* (this PC's clock: the host's t0 is never behind it) */
            send_to(0, p, n);
            next_hello = now + 200000000ull;
        }
        poll_net(20);
        /* Nobody starts until every game has finished its shader loading screen - however long that takes (a game
         * that is not ready still answers ALIVE, and says so there). The host starts them all with START. */
        int waiting_for = -1;
        if (mode == M_HOST) {
            int joined = 0;
            for (int i = 1; i < nplayers; i++) { joined += peer[i].known; if (peer[i].known && !peer[i].ready && waiting_for < 0) waiting_for = i; }
            if (joined == nplayers - 1) {
                phase = 4;
                if (waiting_for < 0 && local_ready()) return 1;
            }
        }
        if (mode == M_JOIN && welcomed) {
            phase = 4;
            if (start_heard && local_ready()) return 1;
        }
        if (now >= next_note) {
            if (next_note) {
                if (!local_ready()) fprintf(stderr, "[netplay] this game is still loading its shaders: the others wait\n");
                else if (waiting_for >= 0) fprintf(stderr, "[netplay] player %d is still loading its shaders: waiting\n", waiting_for + 1);
                else fprintf(stderr, "[netplay] still waiting...\n");
            }
            next_note = now + 10000000000ull;
        }
    }
    fprintf(stderr, "[netplay] nobody came: netplay off\n");
    return 0;
}

static void send_start(void) {   /* (host) every player's address, to all */
    u8 p[HEAD + 1 + MAXP * 4];
    int k = header(p, P_START);
    p[k++] = (u8)nplayers;
    for (int i = 0; i < MAXP; i++) { put32(p + k, i == 0 ? net_local_ip() : peer[i].known ? peer[i].vip : 0); k += 4; }
    send_all(p, k);
}

/* ---- the switches --------------------------------------------------------------------------- */
int lks_probe(void) {
    static int once;
    if (!__atomic_exchange_n(&once, 1, __ATOMIC_ACQ_REL)) {
        const char *np = getenv("SSBU_NETPLAY"), *ls = getenv("SSBU_LOCKSTEP");
        if (np && !strcmp(np, "host")) mode = M_HOST;
        else if (np && !strcmp(np, "fake")) {
            mode = M_HOST;
            fake = 1;
            const char *fl = getenv("SSBU_NETPLAY_FAKE_LAG"), *ff = getenv("SSBU_NETPLAY_FAKE_FROM");
            if (fl && *fl) fake_lag = (u32)atoi(fl);
            if (ff && *ff) fake_from = (u32)atoi(ff);
            fprintf(stderr, "[netplay] on: a made-up player 2, its pads %u-%u frames late (from frame %u)\n", fake_lag, fake_lag + 2, fake_from);
        }
        else if (np && !strcmp(np, "join")) mode = M_JOIN;
        else if (ls && !strcmp(ls, "record")) mode = M_RECORD;
        else if (ls && !strcmp(ls, "replay")) mode = M_REPLAY;
        const char *d = getenv("SSBU_NETPLAY_DELAY"), *fr = getenv("SSBU_LOCKSTEP_FRAMES"), *fn = getenv("SSBU_LOCKSTEP_FILE"), *pl = getenv("SSBU_NETPLAY_PLAYERS");
        if (d && !strcmp(d, "auto")) { delay_auto = 1; delay = 2; }
        else if (d && *d) delay = atoi(d) < 0 ? 0 : atoi(d) > 30 ? 30 : atoi(d);
        if (pl && *pl) nplayers = atoi(pl) < 2 ? 2 : atoi(pl) > MAXP ? MAXP : atoi(pl);
        if (fr && *fr) frames_max = (u32)atoi(fr);
        const char *name = fn && *fn ? fn : "lockstep.bin";
        if (mode == M_RECORD || mode == M_REPLAY) {
            file = fopen(name, mode == M_RECORD ? "wb" : "rb");
            if (!file) { fprintf(stderr, "[lockstep] cannot open %s: off\n", name); mode = M_OFF; }
            else if (mode == M_REPLAY && fread(&t0, 8, 1, file) != 1) { fprintf(stderr, "[lockstep] %s is empty: off\n", name); mode = M_OFF; }
        }
        u64 r = rt_clock_ns();
        nonce = r ^ (r << 21) ^ 0x5DEECE66Dull;
        me = mode == M_JOIN ? -1 : 0;   /* (a joiner has no index until the host gives it one: WELCOME) */
        const char *rc = getenv("SSBU_NETPLAY_ROOM");
        if ((mode == M_HOST || mode == M_JOIN) && rc && *rc) {   /* (through the online lobby) */
            const char *pt = getenv("SSBU_NETPLAY_PORT");
            if (pt && *pt) lobby_port = (u16)atoi(pt);
            snprintf(room_code, sizeof room_code, "%s", rc);
            via_lobby = 1;
            if (!net_configured()) {
                fprintf(stderr, "[netplay] SSBU_NETPLAY_ROOM without a lobby (SSBU_NET_RELAY): off\n");
                via_lobby = 0;
                mode = M_OFF;
            } else
                fprintf(stderr, "[netplay] on: %s, room %s in the lobby %s, %s%d frames of delay%s\n", mode == M_HOST ? "host (player 1)" : "join",
                        room_code, net_relay_address() ? net_relay_address() : "?", delay_auto ? "auto from " : "", delay,
                        mode == M_HOST && nplayers > 2 ? ", waiting for more players" : "");
        } else if ((mode == M_HOST || mode == M_JOIN) && !fake) {
            const char *pt = getenv("SSBU_NETPLAY_PORT"), *pe = getenv("SSBU_NETPLAY_PEER");
            u16 port = pt && *pt ? (u16)atoi(pt) : 7810;
            nplayers = 2;   /* (to an address: two) */
            if (!open_socket(mode == M_HOST ? port : 0)) mode = M_OFF;
            else if (mode == M_JOIN && !(pe && *pe && resolve(pe, port, &peer[0].addr))) {
                fprintf(stderr, "[netplay] SSBU_NETPLAY_PEER: the host's address is missing or unknown: off\n");
                mode = M_OFF;
            } else if (mode == M_JOIN) { peer[0].known = 1; peer[0].last_heard = rt_now_ns(); }
            if (mode == M_HOST) fprintf(stderr, "[netplay] on: host (player 1), UDP port %u, %d frames of delay\n", port, delay);
            if (mode == M_JOIN) fprintf(stderr, "[netplay] on: join, the host at %s:%u\n", inet_ntoa(peer[0].addr.sin_addr), ntohs(peer[0].addr.sin_port));
        }
        if (mode == M_RECORD || mode == M_REPLAY)
            fprintf(stderr, "[lockstep] on: %s %s\n", mode == M_RECORD ? "recording to" : "replaying", name);
        __atomic_store_n(&lks_state, mode != M_OFF, __ATOMIC_RELEASE);
    }
    return __atomic_load_n(&lks_state, __ATOMIC_ACQUIRE) > 0;
}

static int active(void) { return started && !stopped; }
static int on_main(void) { return main_thread && GetCurrentThreadId() == main_thread; }

static void stop(const char *why) {
    if (stopped) return;
    stopped = 1;
    fprintf(stderr, "%s\n", why);
    snprintf((char *)phase_note, sizeof phase_note, "%s", why + (why[0] == '[' ? 10 : 0));
    phase = 5;
    if (mode == M_REPLAY || mode == M_HOST || mode == M_JOIN) print_tally(mode == M_REPLAY ? "lockstep" : "netplay");
    if (mode == M_RECORD && file) { fclose(file); file = NULL; }
    if (mode == M_HOST || mode == M_JOIN) { u8 p[HEAD]; send_all(p, header(p, P_BYE)); }
}

/* (auto delay) every second: the frames the pads need to travel to the farthest player, with room for its jitter */
static void adapt_delay(void) {
    if (!delay_auto || frame % 60) return;
    double ping = 0, jit = 0;
    for (int i = 0; i < MAXP; i++) if (i != me && peer[i].known && peer[i].rtt_n >= 4) { if (peer[i].rtt > ping) ping = peer[i].rtt; if (peer[i].jitter > jit) jit = peer[i].jitter; }
    if (ping <= 0) return;
    int need = (int)((ping / 2 + 2 * jit + 3.0) / 16.67) + 1;
    if (stalls > 6) need++;   /* (it still waited often: one more) */
    need = need < 1 ? 1 : need > 8 ? 8 : need;
    if (need > delay) { delay++; delay_quiet = 0; fprintf(stderr, "[netplay] delay up to %d (ping %.0f ms, jitter %.0f ms)\n", delay, ping, jit); }
    else if (need < delay) { if (++delay_quiet >= 5) { delay--; delay_quiet = 0; fprintf(stderr, "[netplay] delay down to %d (ping %.0f ms, jitter %.0f ms)\n", delay, ping, jit); } }
    else delay_quiet = 0;
}

/* The game's input step begins: frame + 1, and its pads. */
void lks_input_step(void) {
    if (stopped) return;
    if (!started) {
        main_thread = GetCurrentThreadId();
        read_regions();
        if (mode == M_RECORD) { t0 = rt_clock_ns() * 12 / 625; fwrite(&t0, 8, 1, file); }
        if (mode == M_HOST || mode == M_JOIN) {
            if (!via_lobby || mode == M_JOIN) {   /* (the host's lobby thread runs from lks_preboot) */
                static int alive;
                if (!alive) { alive = 1; if (!via_lobby) CloseHandle(CreateThread(NULL, 0, alive_main, NULL, 0, NULL)); }
            }
            if (!connect_peer()) { stopped = 1; phase = 5; return; }
            if (mode == M_HOST) { if (!t0) t0 = rt_clock_ns() * 12 / 625; send_start(); start_sent = 1; }
            compare_with = me == 0 ? 1 : 0;
            phase = 4;
            other_offset = (i64)(t0 - rt_clock_ns() * 12 / 625);
            fprintf(stderr, "[netplay] the game's clock moved %+.1f s to the shared one\n", other_offset / 19200000.0);
        }
        started = 1;
        frame = 0;
        memset(in_of, 0, sizeof in_of);
        memset(have, 0, sizeof have);
        if (mode == M_HOST || mode == M_JOIN) have[me] = (u32)delay;
        fprintf(stderr, "[%s] started: frame 0\n", mode == M_RECORD || mode == M_REPLAY ? "lockstep" : "netplay");
    } else {
        if (!on_main()) return;
        if (mode == M_RECORD) write_frame(frame);
        if (mode == M_REPLAY && want_frame_hashes && !sim_ran) {   /* (the recording simulated this frame; this run did not) */
            frames_compared++;
            if (first_bad == ~0u) { first_bad = frame; fprintf(stderr, "[lockstep] frame %u: the recording simulated this frame, this run did not\n", frame); }
        }
        frame++;
        if (mode == M_RECORD && frame >= frames_max) { stop("[lockstep] recorded enough frames: lockstep off"); return; }
    }
    if (!on_main()) return;
    tick_reads = 0;
    sim_ran = 0;
    ncur = 0;
    switch (mode) {
    case M_RECORD: {
        LksInput local;
        hid_local_input(&local);
        in_of[0][frame % RING] = local;
        memset(&in_of[1][frame % RING], 0, sizeof(LksInput));
        have[0] = have[1] = frame + 1;
        break;
    }
    case M_REPLAY:
        if (!read_frame(frame)) { stop("[lockstep] the recording ends here: lockstep off"); return; }
        break;
    case M_HOST:
    case M_JOIN: {
        if (mode == M_HOST && frame < 300 && frame % 15 == 0) send_start();   /* (a START lost: the others would wait) */
        adapt_delay();
        LksInput local;
        hid_local_input(&local);
        if (me == 0 && lks_movie_ns && rt_now_ns() - lks_movie_ns < 300000000ull && (frame & 8)) local.buttons |= 1;   /* (the opening movie: A, to skip it) */
        while (have[me] < frame + (u32)delay + 1 && have[me] < frame + RING / 2) { in_of[me][have[me] % RING] = local; have[me]++; }
        send_inputs();
        /* pacing: frames ahead of the slowest player - a little wait now, not a stop later */
        u32 slowest = frame;
        for (int i = 0; i < nplayers; i++) if (i != me && peer[i].known && peer[i].frame < slowest) slowest = peer[i].frame;
        if (frame > slowest + 2 && frame > 120) Sleep(frame - slowest - 2 > 4 ? 4 : frame - slowest - 2);
        if (frame % 60 == 0) stalls = 0;
        u64 waited_from = rt_now_ns(), next_send = waited_from + 10000000ull;
        int waited = 0;
        if (fake) fake_deliver();
        if (!rb_on && rbt_net_ready()) { rb_on = 1; fprintf(stderr, "[netplay] rollback on from frame %u: the other players' pads that have not come are guessed\n", frame); }
        if (rb_on) {
            ring_of[frame % RING] = rbt_net_ring_frame(frame);
            if (mispredict_from != ~0u) {   /* (a guess was wrong: back to before that frame, the next frame) */
                u32 g = mispredict_from;
                if (frame - g >= MAX_GUESS + 1 || !rbt_net_rollback(ring_of[g % RING], g)) {
                    fprintf(stderr, "[netplay] frame %u: a wrong guess at frame %u cannot be undone any more: the games may part\n", frame, g);
                    mispredict_from = ~0u;
                    rbt_net_cancel();
                }
            }
            for (int i = 0; i < nplayers; i++) {   /* (the pads not come yet, guessed: that player's last ones) */
                if (i == me || have[i] > frame || frame >= have[i] + MAX_GUESS) continue;
                LksInput guess = have[i] ? in_of[i][(have[i] - 1) % RING] : (LksInput){0};
                in_of[i][frame % RING] = guess;
                guessed_with[i][frame % RING] = guess;
                guessed[i][frame % RING] = 1;
                guesses++;
            }
        }
        for (;;) {
            int ready = 1;
            for (int i = 0; i < nplayers; i++) if (i != me && have[i] <= frame && !(rb_on && guessed[i][frame % RING])) ready = 0;
            if (ready) break;
            waited = 1;
            poll_net(2);
            if (stopped) { stop("[netplay] netplay off"); return; }
            u64 now = rt_now_ns();
            if (now >= next_send) { send_inputs(); if (mode == M_HOST && frame < 300) send_start(); next_send = now + 10000000ull; }
            for (int i = 0; i < nplayers; i++) {
                if (i == me || have[i] > frame) continue;
                u64 heard = peer[i].last_heard > waited_from ? peer[i].last_heard : waited_from;   /* (its game may be busy - loading shaders - but alive: wait) */
                if (now - heard > 60000000000ull) {
                    char why[120];
                    snprintf(why, sizeof why, "[netplay] player %d is not answering: netplay off", i + 1);
                    stop(why);
                    return;
                }
            }
        }
        if (waited && rt_now_ns() - waited_from > 2000000ull) stalls++;
        poll_net(0);
        break;
    }
    }
}

void lks_sim_begin(void) {
    if (!active() || !on_main()) return;
    read_regions();
    sim_seen = 0;
    sim_open = watch(&sim_seen, NULL) >= 0;
}

void lks_sim_end(void) {
    if (!active() || !on_main() || !sim_open) return;
    sim_open = 0;
    ncur = 0;
    if (watch(&sim_seen, collect) < 0) return;
    qsort(cur, ncur, sizeof *cur, by_page);
    sim_ran = 1;
    if (++sim_frames == 1) fprintf(stderr, "[%s] frame %u: the first frame the simulation ran\n", mode == M_RECORD || mode == M_REPLAY ? "lockstep" : "netplay", frame);
    switch (mode) {
    case M_REPLAY: {
        if (!want_frame_hashes) {
            frames_compared++;
            if (first_bad == ~0u) { first_bad = frame; fprintf(stderr, "[lockstep] frame %u: this run simulated this frame, the recording did not\n", frame); }
            break;
        }
        Diff d = compare(cur, ncur, want, nwant);
        frames_compared++;
        if (!d.differ && !d.only_a && !d.only_b) frames_clean++;
        else if (first_bad == ~0u) first_bad = frame;
        tally_diffs(cur, ncur, want, nwant);
        if (sim_frames <= 10 || sim_frames % 300 == 0 || frame == first_bad) print_diff("lockstep", frame, d, "now", "then");
        break;
    }
    case M_HOST:
    case M_JOIN:
        if (rb_on) for (int i = 0; i < nplayers; i++) if (i != me && guessed[i][frame % RING]) return;   /* (a frame played on a guess: its hashes when it is played with the real pads) */
        if (frame % HASH_EVERY == 0) {
            int s = (int)(frame / HASH_EVERY % 4);
            u32 n = ncur < MAX_PAGES / 4 ? ncur : MAX_PAGES / 4;
            memcpy(mine_at[s], cur, n * sizeof *cur);
            nmine_at[s] = n;
            mine_frame[s] = frame;
            send_hashes(frame, cur, n);
            if (theirs_frame == frame && ntheirs == theirs_total) their_hashes_done();
        }
        break;
    }
}

int lks_pad(u32 id, LksInput *in, u64 *sampling) {
    if (!active()) return 0;
    if (mode == M_RECORD || mode == M_REPLAY) {
        if (id > 1 || id == 1) return 0;   /* (record, replay: player 2 is whatever it is - the CPU) */
    } else if ((int)id >= nplayers) return 0;
    *in = in_of[id][frame % RING];
    *sampling = 1000 + frame;
    return 1;
}

int lks_style(u32 id, u32 *style) {
    if (!active() || (mode != M_HOST && mode != M_JOIN)) return 0;
    *style = (int)id < nplayers ? 1u /* full key */ : 0;
    return 1;
}

/* Pictures at the same frames of two runs (or of two PCs), to compare by eye or byte by byte: every
 * SSBU_LOCKSTEP_PICTURES frames (0: none; 600 by default with SSBU_LOCKSTEP). The renderer asks at
 * each frame it shows; the frame it shows may be the game's last or the one before. */
int lks_picture_due(u32 *f) {
    static u32 every = ~0u, next;
    if (!active()) return 0;
    if (every == ~0u) {
        const char *e = getenv("SSBU_LOCKSTEP_PICTURES");
        every = e && *e ? (u32)atoi(e) : mode == M_RECORD || mode == M_REPLAY ? 600 : 0;
        next = every;
    }
    if (!every || frame < next) return 0;
    *f = next;
    next += every;
    return 1;
}

/* (net_rbtest.c) the game was put back to before frame g's input step: the frames from g are played again, with the pads known now */
void lks_rewind(u32 g) {
    rollbacks++;
    rolled_frames += frame - g + 1;
    frame = g - 1;
    mispredict_from = ~0u;
    if (rollbacks <= 20 || rollbacks % 100 == 0)
        fprintf(stderr, "[netplay] rollback %llu: back to frame %u (guesses %llu, wrong %llu, frames played again %llu)\n", (unsigned long long)rollbacks, g,
                (unsigned long long)guesses, (unsigned long long)guesses_wrong, (unsigned long long)rolled_frames);
}
u32 lks_frame(void) { return frame; }
u32 lks_confirmed(void) {   /* the frames whose pads from every player are known for good: 0 .. this - 1 */
    u32 c = ~0u;
    for (int i = 0; i < nplayers; i++) if (i != me && have[i] < c) c = have[i];
    return c;
}
int lks_rollback_on(void) {   /* SSBU_NETPLAY_ROLLBACK=1, or the launcher's "Rollback (testing)" (online_netcode.txt), with the VS netplay */
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("SSBU_NETPLAY_ROLLBACK"), *np = getenv("SSBU_NETPLAY");
        char line[64];
        on = e && *e ? atoi(e) != 0 : np && *np && settings_line_read("online_netcode.txt", line, sizeof line) && !_strnicmp(line, "rollback", 8);
    }
    return on;
}

u64 lks_ticks(u64 now) {
    if (!active()) return now;
    if (!on_main()) return now + (u64)other_offset;
    return t0 + (u64)frame * TICKS_PER_FRAME + 16 * ++tick_reads;
}
