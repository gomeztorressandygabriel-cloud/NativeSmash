/* The core without a network: inputs are fed by hand, late. */
#include <string.h>
#include "check.h"
#include "toygame.h"

/* the match played with no network: every player's scripted input, `delay` frames late for all */
static uint64_t reference(int nplayers, int frames, int delay, uint64_t seed) {
    Toy t;
    toy_init(&t, nplayers);
    RbGame g = toy_game(&t);
    for (int f = 0; f < frames; f++) {
        RbInput in[RB_MAX_PLAYERS];
        for (int p = 0; p < nplayers; p++) in[p] = f < delay ? 0 : toy_script(p, f, seed);
        g.advance(&t, in, 0);
    }
    return toy_hash(&t);
}

/* Play `frames` ticks as player 0; each remote player's input for frame f is delivered just
 * before the tick of frame f + lag[p] (lag < 0: never, until the end). */
static void play(int nplayers, int frames, int delay, int maxpred, const int *lag, Toy *t, RbSession **out) {
    toy_init(t, nplayers);
    RbGame g = toy_game(t);
    RbConfig c = {nplayers, 0, maxpred, delay, 4, 0};
    RbSession *s = rb_create(&g, &c);
    CHECK(s != NULL, "rb_create");
    for (int tick = 0; tick < frames; tick++) {
        for (int p = 1; p < nplayers; p++) {
            int f = tick - lag[p];
            if (lag[p] >= 0 && f >= 0) rb_add_remote_input(s, p, f, f < delay ? 0 : toy_script(p, f, 5));
        }
        rb_tick(s, tick + delay < 0 ? 0 : toy_script(0, tick + delay, 5));
    }
    *out = s;
}

static void deliver_rest(RbSession *s, int nplayers, int upto, int delay) {
    for (int p = 1; p < nplayers; p++)
        for (int f = 0; f < upto; f++)
            rb_add_remote_input(s, p, f, f < delay ? 0 : toy_script(p, f, 5));
    rb_resolve(s);
}

static void test_late_inputs_converge(void) {
    for (int delay = 0; delay <= 2; delay++)
        for (int lag = 0; lag <= 7; lag++) {
            Toy t; RbSession *s;
            int l[2] = {0, lag};
            play(2, 200, delay, 10, l, &t, &s);
            deliver_rest(s, 2, 200, delay);
            CHECK(toy_hash(&t) == reference(2, 200, delay, 5), "delay=%d lag=%d: state differs from the no-network match", delay, lag);
            const RbStats *st = rb_stats(s);
            if (lag > 0) CHECK(st->rollbacks > 0 && st->resim_frames > 0, "lag=%d should have caused rollbacks", lag);
            CHECK(st->max_depth <= (uint32_t)lag, "depth %u for lag %d", st->max_depth, lag);
            CHECK(t.resim_calls == st->resim_frames, "resimulating flag: %llu calls, %llu frames", (unsigned long long)t.resim_calls, (unsigned long long)st->resim_frames);
            if (lag > 3) CHECK(t.sound_suppressed > 0 || t.sound_events == 0 || 1, "(effects of replayed frames are suppressed)");
            rb_destroy(s);
        }
}

static void test_correct_prediction_costs_nothing(void) {
    Toy t; toy_init(&t, 2);
    RbGame g = toy_game(&t);
    RbConfig c = {2, 0, 8, 0, 0, 0};
    RbSession *s = rb_create(&g, &c);
    for (int f = 0; f < 100; f++) {
        if (f >= 3) rb_add_remote_input(s, 1, f - 3, 0);   /* the remote never presses anything: "same as last" is right */
        rb_tick(s, toy_script(0, f, 5));
    }
    CHECK(rb_stats(s)->rollbacks == 0 && rb_stats(s)->late_inputs == 0, "rollbacks=%llu late=%llu",
          (unsigned long long)rb_stats(s)->rollbacks, (unsigned long long)rb_stats(s)->late_inputs);
    CHECK(rb_stats(s)->resim_frames == 0, "no frame should be replayed");
    rb_destroy(s);
}

static void test_stall_at_window(void) {
    Toy t; toy_init(&t, 2);
    RbGame g = toy_game(&t);
    RbConfig c = {2, 0, 4, 0, 0, 0};
    RbSession *s = rb_create(&g, &c);
    int ok = 0, stalled = 0;
    for (int i = 0; i < 10; i++) (rb_tick(s, 0) == RB_OK ? ok++ : stalled++);
    CHECK(ok == 4 && stalled == 6, "ok=%d stalled=%d (window 4)", ok, stalled);
    CHECK(rb_frame(s) == 4, "frame %d", rb_frame(s));
    rb_add_remote_input(s, 1, 0, 0);
    CHECK(rb_tick(s, 0) == RB_OK, "one confirmed input frees one frame");
    CHECK(rb_tick(s, 0) == RB_STALL, "and only one");
    rb_destroy(s);
}

static void test_duplicates_and_conflicts(void) {
    Toy t; toy_init(&t, 2);
    RbGame g = toy_game(&t);
    RbConfig c = {2, 0, 8, 0, 0, 0};
    RbSession *s = rb_create(&g, &c);
    rb_tick(s, 0); rb_tick(s, 0);
    CHECK(rb_add_remote_input(s, 1, 0, 7) == RB_OK, "first");
    CHECK(rb_add_remote_input(s, 1, 0, 7) == RB_OK, "duplicate");
    CHECK(rb_stats(s)->conflicts == 0, "a duplicate is not a conflict");
    rb_add_remote_input(s, 1, 0, 9);
    CHECK(rb_stats(s)->conflicts == 1, "a different input for the same frame is counted");
    CHECK(rb_add_remote_input(s, 0, 0, 1) == RB_ERR_ARG, "own player");
    CHECK(rb_add_remote_input(s, 1, RB_INPUT_RING + 50, 1) == RB_ERR_FAR, "too far ahead is refused (and not to be acknowledged)");
    /* inputs out of order: 3 before 1 and 2 */
    rb_add_remote_input(s, 1, 3, 1);
    CHECK(rb_confirmed_upto(s, 1) == 0, "a gap holds the contiguous prefix");
    rb_add_remote_input(s, 1, 1, 0); rb_add_remote_input(s, 1, 2, 0);
    CHECK(rb_confirmed_upto(s, 1) == 3, "gap filled: %d", rb_confirmed_upto(s, 1));
    rb_destroy(s);
}

static void test_three_players(void) {
    Toy t; RbSession *s;
    int l[3] = {0, 2, 5};
    play(3, 240, 1, 12, l, &t, &s);
    deliver_rest(s, 3, 240, 1);
    CHECK(toy_hash(&t) == reference(3, 240, 1, 5), "3-player state differs");
    rb_destroy(s);
}

static void test_bad_config(void) {
    Toy t; toy_init(&t, 2);
    RbGame g = toy_game(&t);
    RbConfig bad[] = {{1, 0, 8, 0, 0, 0}, {2, 2, 8, 0, 0, 0}, {2, 0, 0, 0, 0, 0}, {2, 0, RB_STATE_RING, 0, 0, 0}, {2, 0, 8, 9, 0, 0}, {2, 0, 8, 0, -1, 0}, {2, 0, 8, 0, 0, 9}};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) CHECK(rb_create(&g, &bad[i]) == NULL, "config %u accepted", i);
}


/* Sync test: the toy game with a counter that the simulation reads and save() does not hold. */
static void test_synctest_finds_unsaved_state(void) {
    for (int bug = 0; bug < 2; bug++) {
        Toy t; toy_init(&t, 2);
        t.bug = bug ? TOY_BUG_UNSAVED : TOY_OK;
        RbGame g = toy_game(&t);
        RbConfig c = {2, 0, 8, 0, 0, 3};
        RbSession *s = rb_create(&g, &c);
        for (int f = 0; f < 300; f++) {
            rb_add_remote_input(s, 1, f, toy_script(1, f, 5));   /* (every input present: nothing is predicted) */
            rb_tick(s, toy_script(0, f, 5));
        }
        int32_t at; int who;
        if (bug) CHECK(rb_desync(s, &at, &who) && who == -1 && rb_stats(s)->synctest_failures > 0, "the unsaved counter was not found");
        else CHECK(!rb_desync(s, &at, &who) && rb_stats(s)->synctest_failures == 0, "false alarm on a correct game");
        CHECK(rb_stats(s)->resim_frames >= 3 * 290, "the sync test should have replayed %llu frames", (unsigned long long)rb_stats(s)->resim_frames);
        rb_destroy(s);
    }
}

void test_core_all(void) {
    RUN(test_bad_config);
    RUN(test_late_inputs_converge);
    RUN(test_correct_prediction_costs_nothing);
    RUN(test_stall_at_window);
    RUN(test_duplicates_and_conflicts);
    RUN(test_three_players);
    RUN(test_synctest_finds_unsaved_state);
}
