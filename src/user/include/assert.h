#pragma once

#ifdef NDEBUG
#  define assert(cond) ((void)0)
#else
void _assert_fail(const char *expr, const char *file, int line);
#  define assert(cond) ((cond) ? (void)0 : _assert_fail(#cond, __FILE__, __LINE__))
#endif
