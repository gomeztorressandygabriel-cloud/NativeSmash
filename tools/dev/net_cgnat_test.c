/* The direct path to a player behind a symmetric (carrier-grade) NAT, without the game:
 * runtime/net_relay.c on its own, as Alpha (whose router lets anything in), and this program as
 * Charlie, whose hellos come from another port than the one the lobby gave. Alpha must take the
 * port they come from, prove the path both ways there and send game packets straight there.
 * Build and run (from the repository):
 *   deps/llvm/bin/clang-cl.exe /nologo -w /winsysroot deps/msvc /I runtime tools/dev/net_cgnat_test.c
 *     /Fe:build/net_cgnat_test.exe /link ws2_32.lib ole32.lib oleaut32.lib
 *   build/net_cgnat_test.exe     (prints PASS or FAIL) */
#include "../../runtime/net_relay.c"

/* ---- what net_relay.c needs from the rest of the runtime ---- */
const RtModule rt_module_main = {"test"};
const char *rt_data_dir = ".";
int display_boost_threads(void) { return 0; }
int settings_online(void) { return 0; }
int settings_line_read(const char *f, char *line, int n) { (void)f; (void)line; (void)n; return 0; }
int settings_line_write(const char *f, const char *line) { (void)f; (void)line; return 0; }
int ldn_in_network(u32 ip) { (void)ip; return 1; }
void ldn_listing(u32 seq, u32 index, const u8 *info) { (void)seq; (void)index; (void)info; }
void ldn_list_end(u32 seq) { (void)seq; }
static int delivered;
void vsock_deliver(u32 src_ip, u16 src_port, u32 dst_ip, u16 dst_port, const void *data, u32 len) {
    (void)src_port; (void)dst_ip; (void)dst_port; (void)data;
    if (src_ip == 0x0A0D0002 && len == 5) delivered++;
}

#define ALPHA 0x0A0D0001u
#define CHARLIE 0x0A0D0002u
#define SECRET 0x1122334455667788ull

static int ok = 1;
static void expect(int cond, const char *what) { printf("  %s: %s\n", cond ? "ok" : "MISSING", what); ok &= cond; }

static SOCKET loopback_socket(void) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(s, (struct sockaddr *)&a, sizeof a);
    DWORD timeout = 300;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
    no_connreset(s);
    return s;
}
static struct sockaddr_in address_of(SOCKET s) {
    struct sockaddr_in a;
    socklen_t len = sizeof a;
    getsockname(s, (struct sockaddr *)&a, &len);
    return a;
}

/* Alpha reads what reached it, as its client thread does. */
static void alpha_reads(void) {
    u8 buf[2048];
    for (;;) {
        struct sockaddr_in from;
        socklen_t flen = sizeof from;
        int n = recvfrom(client_sock, (char *)buf, sizeof buf, 0, (struct sockaddr *)&from, &flen);
        if (n < 0) return;
        if (n >= 4 && buf[0] == MAGIC0 && buf[1] == MAGIC1 && buf[2] == NET_PROTOCOL) direct_packet(buf, n, &from);
    }
}

static void punch(SOCKET from, struct sockaddr_in to, u64 secret, int ack, u64 word) {
    u8 p[PUNCH_SIZE];
    header(p, T_PUNCH);
    put64(p + 4, secret);
    put32(p + 12, CHARLIE);
    put32(p + 16, ALPHA);
    p[20] = (u8)ack;
    put64(p + 21, word);
    sendto(from, (const char *)p, sizeof p, 0, (struct sockaddr *)&to, sizeof to);
}

/* What reached Charlie's socket: the last hello's nonce, whether an ack echoed want, a T_DIRECT. */
static void charlie_reads(SOCKET s, u64 want, u64 *hello_nonce, int *acked, int *directs) {
    u8 buf[2048];
    for (;;) {
        int n = recv(s, (char *)buf, sizeof buf, 0);
        if (n < 4) return;
        if (buf[3] == T_PUNCH && n >= PUNCH_SIZE && get64(buf + 4) == SECRET) {
            if (buf[20] && get64(buf + 21) == want) *acked = 1;
            if (!buf[20]) *hello_nonce = get64(buf + 21);
        } else if (buf[3] == T_DIRECT && directs) (*directs)++;
    }
}

static void introduce(u32 version, struct sockaddr_in lobby_saw) {
    u8 p[5 + PEER_SIZE] = {0}, *e = p + 5, *a = e + 4 + NAME_LEN + 1;
    put32(p, version);
    p[4] = 1;
    put32(e, CHARLIE);
    memcpy(e + 4, "Charlie", 7);
    e[4 + NAME_LEN] = 1;
    put32(a, ntohl(lobby_saw.sin_addr.s_addr));
    put16(a + 4, ntohs(lobby_saw.sin_port));
    put64(a + 12, SECRET);
    read_peers(p, sizeof p);
}

int main(void) {
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);
    configured = 1;
    client_sock = peer_sock = loopback_socket();
    joined = lobby = 1;
    my_ip = ALPHA;
    struct sockaddr_in alpha_at = address_of(client_sock);
    SOCKET charlie_lobby = loopback_socket(), charlie_out = loopback_socket();   /* the port the lobby saw; the one Alpha sees */
    printf("net_cgnat_test:\n");

    introduce(1, address_of(charlie_lobby));
    expect(npeers == 1 && peers[0].introduced && peers[0].ncand == 1, "Alpha knows Charlie at the lobby's address");

    punch(charlie_out, alpha_at, 0x99, 0, 77);   /* (another pair's secret: nothing) */
    alpha_reads();
    u64 nonce = 0;
    int acked = 0;
    charlie_reads(charlie_out, 77, &nonce, &acked, NULL);
    expect(!acked && !nonce && peers[0].ncand == 1, "a hello with another secret is not answered");

    punch(charlie_out, alpha_at, SECRET, 0, 1234);   /* Charlie's hello, from the port its NAT gave for Alpha */
    alpha_reads();
    charlie_reads(charlie_out, 1234, &nonce, &acked, NULL);
    expect(peers[0].ncand == 3 && peers[0].cand[CAND_SEEN].at.sin_port == address_of(charlie_out).sin_port, "Alpha took the port the hello came from");
    expect(acked, "Alpha answered Charlie's hello there");
    expect(nonce != 0, "Alpha said hello there too");

    punch(charlie_out, alpha_at, SECRET, 1, nonce);   /* Charlie's answer: the path works both ways */
    alpha_reads();
    expect(peers[0].state == P_DIRECT && peers[0].via == CAND_SEEN, "Alpha: direct, via the seen address");

    int directs = 0;
    net_send(ALPHA, 5000, CHARLIE, 5000, "hello", 5);
    charlie_reads(charlie_out, 0, &nonce, &acked, &directs);
    expect(directs == 1, "Alpha's game packets go straight to Charlie's seen port");

    u8 d[DIRECT_HEAD + 12 + 5];
    header(d, T_DIRECT);
    put64(d + 4, SECRET);
    put32(d + 12, CHARLIE); put32(d + 16, ALPHA); put16(d + 20, 5000); put16(d + 22, 5000);
    memcpy(d + 24, "hello", 5);
    sendto(charlie_out, (const char *)d, sizeof d, 0, (struct sockaddr *)&alpha_at, sizeof alpha_at);
    alpha_reads();
    expect(delivered == 1, "Charlie's game packets from there reach Alpha's game");

    introduce(2, address_of(charlie_lobby));   /* (the lobby's next PEERS, the same room) */
    expect(peers[0].state == P_DIRECT && peers[0].ncand == 3 && peers[0].via == CAND_SEEN, "a new PEERS keeps the path");

    peers_tick(now_ms());   /* (hellos while direct go to the seen port) */
    nonce = 0;
    charlie_reads(charlie_out, 0, &nonce, &acked, NULL);
    expect(nonce != 0, "the keep-alive hellos go there");

    puts(ok ? "PASS" : "FAIL");
    return !ok;
}
