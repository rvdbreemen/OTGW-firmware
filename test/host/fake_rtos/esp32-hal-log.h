// TASK-1736 host fake. cbuf.cpp includes this after cbuf.h: route its temporary
// malloc/free (the copy in cbuf::resize) through the test's allocation tracker.
#pragma once
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
void *track_malloc(size_t n);
void track_free(void *p);
#ifdef __cplusplus
}
#endif
#define malloc(n) track_malloc(n)
#define free(p) track_free(p)
#define log_e(...) ((void)0)
