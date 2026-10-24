#include <stdio.h>
#include "check.h"

int g_failures, g_checks;
void test_core_all(void);
void test_net_all(void);
void test_pages_all(void);

int main(void) {
    test_core_all();
    test_pages_all();
    test_net_all();
    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
