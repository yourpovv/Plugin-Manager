#pragma once

#include <cstdio>

inline int testFailures = 0;

#define check(cond) \
    do { if (!(cond)) { \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        ++testFailures; \
    } } while (0)
