/* bionic has no <execinfo.h> before Android 13 (API 33): backtrace() is runtime/android/backtrace.c's. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int backtrace(void **buffer, int size);
#ifdef __cplusplus
}
#endif
