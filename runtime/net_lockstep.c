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
#define REDUNDANCY 8                /* frames of pads each packet repeats (a lost packet costs nothing) */
#define MAGIC 0x4B4C534Eu           /* "NSLK" */
#define VERSION 1

enum { M_OFF, M_RECORD, M_REPLAY, M_HOST, M_JOIN };
enum { P_HELLO = 1, P_WELCOME, P_INPUT, P_HASH, P_BYE, P_ALIVE };

static int mode, started, stopped, delay = 2;
static DWORD main_thread;
static u32 frame;                   /* the frame now (the input step's count since the start) */
static u32 frames_max = 15000;      /* (record) */
static u64 t0, tick_reads;
static LksInput in_of[2][RING];     /* the two players' pads, by frame */
static u32 have[2];                 /* frames known: 0 .. have-1 */
static int me;                      /* this PC's player: 0 host / record / replay, 1 joiner */

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

/* ---- netplay ---------------------------------------------------------------------------- */
static SOCKET sock = INVALID_SOCKET;
static struct sockaddr_in peer;
static int peer_known;
static u64 nonce, last_heard_ns;
static PageHash mine_at[4][MAX_PAGES / 4];   /* (the last frames' lists, to compare with the other PC's) */
static u32 nmine_at[4], mine_frame[4];
static PageHash theirs[MAX_PAGES / 4];
static u32 ntheirs, theirs_frame = ~0u, theirs_total;

static void put32(u8 *p, u32 v) { memcpy(p, &v, 4); }
static u32 get32(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }

/* Through the online lobby: the other player's virtual address, and the packets for this game (lks_deliver, on the relay's thread),
 * kept until the main thread reads them (poll_net). */
static int via_lobby;
static char room_code[64];
static u16 lobby_port = 7810;
static u32 peer_vip;
#define LOBBY_QUEUE 256
#define LOBBY_PACKET 1400
static struct { u32 src; u16 port, len; u8 data[LOBBY_PACKET]; } lobby_queue[LOBBY_QUEUE];
static u32 lq_head, lq_tail;   /* (lq_tail - lq_head: packets waiting) */
static SRWLOCK lq_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE lq_cv = CONDITION_VARIABLE_INIT;

int lks_deliver(u32 src_ip, u16 src_port, u16 dst_port, const void *data, u32 len) {
    if (!via_lobby || dst_port != lobby_port) return 0;
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

static void send_packet(const u8 *p, int n) {
    if (via_lobby) {
        if (peer_vip) net_send(net_local_ip(), lobby_port, peer_vip, lobby_port, p, (u32)n);
        return;
    }
    if (peer_known) sendto(sock, (const char *)p, n, 0, (const struct sockaddr *)&peer, sizeof peer);
}

static int header(u8 *p, int type) { put32(p, MAGIC); p[4] = VERSION; p[5] = (u8)type; return 6; }

static void send_inputs(void) {   /* this PC's pads, the last REDUNDANCY frames of them, and what it has of the other's */
    u8 p[64 + REDUNDANCY * sizeof(LksInput)];
    int n = header(p, P_INPUT);
    u32 last = have[me], first = last > REDUNDANCY ? last - REDUNDANCY : 0;
    put32(p + n, first); n += 4;
    p[n++] = (u8)(last - first);
    put32(p + n, have[!me]); n += 4;
    for (u32 f = first; f < last; f++) { memcpy(p + n, &in_of[me][f % RING], sizeof(LksInput)); n += sizeof(LksInput); }
    send_packet(p, n);
}

static void send_hashes(u32 f, const PageHash *list, u32 count) {   /* in pieces of 100 */
    for (u32 off = 0; off < count || off == 0; off += 100) {
        u8 p[32 + 100 * 12];   /* (a header of 19 bytes, then up to 100 entries of 12: 16 + 1200 was 3 bytes short, and a full piece overran the stack) */
        int n = header(p, P_HASH);
        u32 k = count - off < 100 ? count - off : 100;
        put32(p + n, f); n += 4;
        put32(p + n, count); n += 4;
        put32(p + n, off); n += 4;
        p[n++] = (u8)k;
        for (u32 i = 0; i < k; i++) { put32(p + n, list[off + i].page); memcpy(p + n + 4, &list[off + i].hash, 8); n += 12; }
        send_packet(p, n);
        if (!count) break;
    }
}

static void their_hashes_done(void) {
    for (int s = 0; s < 4; s++)
        if (mine_frame[s] == theirs_frame) {
            qsort(theirs, ntheirs, sizeof *theirs, by_page);
            Diff d = compare(mine_at[s], nmine_at[s], theirs, ntheirs);
            frames_compared++;
            if (!d.differ && !d.only_a && !d.only_b) frames_clean++;
            else if (first_bad == ~0u) first_bad = theirs_frame;
            tally_diffs(mine_at[s], nmine_at[s], theirs, ntheirs);
            print_diff("netplay", theirs_frame, d, "here", "there");
            return;
        }
}

static void on_packet(const u8 *p, int n, const struct sockaddr_in *from) {
    if (n < 6 || get32(p) != MAGIC || p[4] != VERSION) return;
    last_heard_ns = rt_now_ns();
    switch (p[5]) {
    case P_HELLO:   /* (host) a player joins: the delay and the clock are the host's */
        if (mode != M_HOST || n < 14) return;
        if (!t0) t0 = rt_clock_ns() * 12 / 625;
        if (!peer_known) fprintf(stderr, "[netplay] player 2 joined from %s:%u\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
        peer = *from;
        if (via_lobby) peer_vip = ntohl(from->sin_addr.s_addr);
        peer_known = 1;
        {
            u8 q[32];
            int k = header(q, P_WELCOME);
            memcpy(q + k, p + 6, 8); k += 8;   /* (their nonce) */
            memcpy(q + k, &t0, 8); k += 8;
            q[k++] = (u8)delay;
            send_packet(q, k);
        }
        break;
    case P_WELCOME:   /* (joiner) */
        if (mode != M_JOIN || n < 23) return;
        {
            u64 their_nonce;
            memcpy(&their_nonce, p + 6, 8);
            if (their_nonce != nonce || peer_known == 2) return;
            memcpy(&t0, p + 14, 8);
            delay = p[22];
            peer_known = 2;
            fprintf(stderr, "[netplay] joined: player 2, %d frames of delay\n", delay);
        }
        break;
    case P_INPUT: {
        if (n < 15) return;
        u32 first = get32(p + 6), count = p[10];
        if (n < 15 + (int)(count * sizeof(LksInput))) return;
        for (u32 i = 0; i < count; i++) {
            u32 f = first + i;
            if (f < have[!me] || f >= have[!me] + RING / 2) continue;
            if (f == have[!me]) {
                memcpy(&in_of[!me][f % RING], p + 15 + i * sizeof(LksInput), sizeof(LksInput));
                have[!me] = f + 1;
            }
        }
        break;
    }
    case P_HASH: {
        if (n < 19) return;
        u32 f = get32(p + 6), total = get32(p + 10), off = get32(p + 14), k = p[18];
        if (n < 19 + (int)k * 12 || total > MAX_PAGES / 4) return;
        if (f != theirs_frame) { theirs_frame = f; ntheirs = 0; theirs_total = total; }
        if (off != ntheirs) return;   /* (a piece lost or out of order: this frame is not compared) */
        for (u32 i = 0; i < k; i++) { theirs[ntheirs].page = get32(p + 19 + i * 12); memcpy(&theirs[ntheirs].hash, p + 23 + i * 12, 8); ntheirs++; }
        if (ntheirs == theirs_total) their_hashes_done();
        break;
    }
    case P_BYE:
        fprintf(stderr, "[netplay] the other player left\n");
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
            struct sockaddr_in from = {0};
            from.sin_family = AF_INET;
            from.sin_addr.s_addr = htonl(lobby_queue[k].src);
            from.sin_port = htons(lobby_queue[k].port);
            memcpy(p, lobby_queue[k].data, (size_t)n);
            ReleaseSRWLockExclusive(&lq_lock);
            on_packet(p, n, &from);
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
        if (n > 0) on_packet(p, n, &from);
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

/* At the first input step: wait for the other PC (the host for a joiner, a joiner for the host). */
static int connect_peer(void) {
    const char *w = getenv("SSBU_NETPLAY_WAIT");
    double wait_s = w && *w ? atof(w) : via_lobby ? 3600.0 : 300.0;   /* (through the lobby: as long as the other takes to start) */
    u64 until = rt_now_ns() + (u64)(wait_s * 1e9), next_hello = 0, next_note = 0, next_room = 0;
    int in_lobby = 0, room_ready = 0;
    fprintf(stderr, mode == M_HOST ? "[netplay] waiting for player 2 (up to %.0f s)\n" : "[netplay] joining the host (up to %.0f s)\n", wait_s);
    while (rt_now_ns() < until) {
        if (via_lobby && !room_ready) {   /* (first in the lobby, then the room: made by the host, found and joined by the other) */
            if (!in_lobby) {
                if (net_local_ip()) { in_lobby = 1; fprintf(stderr, "[netplay] in the lobby as %u.%u.%u.%u\n", net_local_ip() >> 24, net_local_ip() >> 16 & 255, net_local_ip() >> 8 & 255, net_local_ip() & 255); }
                else if (net_unreachable() && rt_now_ns() >= next_room) { fprintf(stderr, "[netplay] the lobby does not answer (internet? the lobby's address?)\n"); next_room = rt_now_ns() + 30000000000ull; }
            }
            if (in_lobby && rt_now_ns() >= next_room) {
                if (mode == M_HOST) room_ready = ldn_direct_host(room_code);
                else if ((peer_vip = ldn_direct_join(room_code)) != 0) { room_ready = 1; peer_known = 1; }
                next_room = rt_now_ns() + 2000000000ull;
            }
            if (!room_ready) { Sleep(50); continue; }
        }
        if (mode == M_JOIN && rt_now_ns() >= next_hello) {
            u8 p[32];
            int n = header(p, P_HELLO);
            memcpy(p + n, &nonce, 8); n += 8;
            send_packet(p, n);
            next_hello = rt_now_ns() + 200000000ull;
        }
        poll_net(20);
        if (mode == M_HOST && peer_known) return 1;
        if (mode == M_JOIN && peer_known == 2) return 1;
        if (rt_now_ns() >= next_note) {
            if (next_note) fprintf(stderr, "[netplay] still waiting...\n");
            next_note = rt_now_ns() + 10000000000ull;
        }
    }
    fprintf(stderr, "[netplay] nobody came: netplay off\n");
    return 0;
}

/* ---- the switches --------------------------------------------------------------------------- */
int lks_probe(void) {
    static int once;
    if (!__atomic_exchange_n(&once, 1, __ATOMIC_ACQ_REL)) {
        const char *np = getenv("SSBU_NETPLAY"), *ls = getenv("SSBU_LOCKSTEP");
        if (np && !strcmp(np, "host")) mode = M_HOST;
        else if (np && !strcmp(np, "join")) mode = M_JOIN;
        else if (ls && !strcmp(ls, "record")) mode = M_RECORD;
        else if (ls && !strcmp(ls, "replay")) mode = M_REPLAY;
        const char *d = getenv("SSBU_NETPLAY_DELAY"), *fr = getenv("SSBU_LOCKSTEP_FRAMES"), *fn = getenv("SSBU_LOCKSTEP_FILE");
        if (d && *d) delay = atoi(d) < 0 ? 0 : atoi(d) > 30 ? 30 : atoi(d);
        if (fr && *fr) frames_max = (u32)atoi(fr);
        const char *name = fn && *fn ? fn : "lockstep.bin";
        if (mode == M_RECORD || mode == M_REPLAY) {
            file = fopen(name, mode == M_RECORD ? "wb" : "rb");
            if (!file) { fprintf(stderr, "[lockstep] cannot open %s: off\n", name); mode = M_OFF; }
            else if (mode == M_REPLAY && fread(&t0, 8, 1, file) != 1) { fprintf(stderr, "[lockstep] %s is empty: off\n", name); mode = M_OFF; }
        }
        const char *rc = getenv("SSBU_NETPLAY_ROOM");
        if ((mode == M_HOST || mode == M_JOIN) && rc && *rc) {   /* (through the online lobby) */
            const char *pt = getenv("SSBU_NETPLAY_PORT");
            if (pt && *pt) lobby_port = (u16)atoi(pt);
            snprintf(room_code, sizeof room_code, "%s", rc);
            via_lobby = 1;
            me = mode == M_JOIN;
            u64 r = rt_clock_ns();
            nonce = r ^ (r << 21) ^ 0x5DEECE66Dull;
            if (!net_configured()) {
                fprintf(stderr, "[netplay] SSBU_NETPLAY_ROOM without a lobby (SSBU_NET_RELAY): off\n");
                via_lobby = 0;
                mode = M_OFF;
            } else
                fprintf(stderr, "[netplay] on: %s, room %s in the lobby %s, %d frames of delay\n", mode == M_HOST ? "host (player 1)" : "join (player 2)",
                        room_code, net_relay_address() ? net_relay_address() : "?", delay);
        } else if (mode == M_HOST || mode == M_JOIN) {
            const char *pt = getenv("SSBU_NETPLAY_PORT"), *pe = getenv("SSBU_NETPLAY_PEER");
            u16 port = pt && *pt ? (u16)atoi(pt) : 7810;
            if (!open_socket(mode == M_HOST ? port : 0)) mode = M_OFF;
            else if (mode == M_JOIN && !(pe && *pe && resolve(pe, port, &peer))) {
                fprintf(stderr, "[netplay] SSBU_NETPLAY_PEER: the host's address is missing or unknown: off\n");
                mode = M_OFF;
            } else if (mode == M_JOIN) peer_known = 1;
            me = mode == M_JOIN;
            u64 r = rt_clock_ns();
            nonce = r ^ (r << 21) ^ 0x5DEECE66Dull;
            if (mode == M_HOST) fprintf(stderr, "[netplay] on: host (player 1), UDP port %u, %d frames of delay\n", port, delay);
            if (mode == M_JOIN) fprintf(stderr, "[netplay] on: join (player 2), the host at %s:%u\n", inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
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
    if (mode == M_REPLAY || mode == M_HOST || mode == M_JOIN) print_tally(mode == M_REPLAY ? "lockstep" : "netplay");
    if (mode == M_RECORD && file) { fclose(file); file = NULL; }
    if ((mode == M_HOST || mode == M_JOIN) && peer_known) { u8 p[8]; send_packet(p, header(p, P_BYE)); }
}

/* (its own thread, from the start of netplay) "alive" every 250 ms, whatever this game's main thread is doing */
static DWORD WINAPI alive_main(void *unused) {
    (void)unused;
    SetThreadDescription(GetCurrentThread(), L"netplay alive");
    while (!stopped) {
        u8 p[8];
        send_packet(p, header(p, P_ALIVE));
        Sleep(250);
    }
    return 0;
}

/* The game's input step begins: frame + 1, and its pads. */
void lks_input_step(void) {
    if (stopped) return;
    if (!started) {
        main_thread = GetCurrentThreadId();
        read_regions();
        if (mode == M_RECORD) { t0 = rt_clock_ns() * 12 / 625; fwrite(&t0, 8, 1, file); }
        if ((mode == M_HOST || mode == M_JOIN) && !connect_peer()) { stopped = 1; return; }
        if (mode == M_HOST || mode == M_JOIN) CloseHandle(CreateThread(NULL, 0, alive_main, NULL, 0, NULL));
        started = 1;
        frame = 0;
        memset(in_of, 0, sizeof in_of);
        have[0] = have[1] = mode == M_HOST || mode == M_JOIN ? (u32)delay : 0;
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
        LksInput local;
        hid_local_input(&local);
        if (have[me] == frame + (u32)delay) { in_of[me][have[me] % RING] = local; have[me]++; }
        send_inputs();
        u64 waited_from = rt_now_ns(), next_send = waited_from + 10000000ull;
        while (have[!me] <= frame) {
            poll_net(2);
            if (stopped) { stop("[netplay] netplay off"); return; }
            u64 now = rt_now_ns();
            if (now >= next_send) { send_inputs(); next_send = now + 10000000ull; }
            u64 heard = last_heard_ns > waited_from ? last_heard_ns : waited_from;   /* (its game may be busy - loading shaders - but alive: wait) */
            if (now - heard > 15000000000ull) { stop("[netplay] the other player is not answering: netplay off (player 2 stands still)"); return; }
        }
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
    if (!active() || id > 1) return 0;
    if (mode == M_RECORD && id == 1) return 0;   /* (recording: player 2 is whatever it is - the CPU) */
    if (mode == M_REPLAY && id == 1) return 0;
    *in = in_of[id][frame % RING];
    *sampling = 1000 + frame;
    return 1;
}

int lks_style(u32 id, u32 *style) {
    if (!active() || (mode != M_HOST && mode != M_JOIN)) return 0;
    *style = id <= 1 ? 1u /* full key */ : 0;
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

u64 lks_ticks(u64 now) {
    if (!active() || !on_main()) return now;
    return t0 + (u64)frame * TICKS_PER_FRAME + 16 * ++tick_reads;
}
