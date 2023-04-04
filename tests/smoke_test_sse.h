//
// Created by Paul Walker on 4/3/23.
//

#ifndef SHORTCIRCUITXT_SMOKE_TEST_SSE_H
#define SHORTCIRCUITXT_SMOKE_TEST_SSE_H

#if defined(__arm64__)
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "simde/x86/sse2.h"
#else
#include <emmintrin.h>
#endif

#endif // SHORTCIRCUITXT_SMOKE_TEST_SSE_H
