#ifndef CHECK_H
#define CHECK_H
#include <stdio.h>
extern int g_failures, g_checks;
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_failures++; printf("    FAIL %s:%d: %s\n      ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define RUN(fn) do { int before = g_failures; printf("[ RUN ] %s\n", #fn); fn(); printf("[ %s ] %s\n", g_failures == before ? " OK " : "FAIL", #fn); } while (0)
#endif
