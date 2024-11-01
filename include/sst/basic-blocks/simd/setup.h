/*
 * sst-basic-blocks - an open source library of core audio utilities
 * built by Surge Synth Team.
 *
 * Provides a collection of tools useful on the audio thread for blocks,
 * modulation, etc... or useful for adapting code to multiple environments.
 *
 * Copyright 2023, various authors, as described in the GitHub
 * transaction log. Parts of this code are derived from similar
 * functions original in Surge or ShortCircuit.
 *
 * sst-basic-blocks is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html.
 *
 * A very small number of explicitly chosen header files can also be
 * used in an MIT/BSD context. Please see the README.md file in this
 * repo or the comments in the individual files. Only headers with an
 * explicit mention that they are dual licensed may be copied and reused
 * outside the GPL3 terms.
 *
 * All source in sst-basic-blocks available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

#ifndef INCLUDE_SST_BASIC_BLOCKS_SIMD_SETUP_H
#define INCLUDE_SST_BASIC_BLOCKS_SIMD_SETUP_H

/**
 * \page the sst/basic-blocks/simd/setup.h header providces a set of macros and defines
 * for simd inclusion which we can use across all of our properties, which conditionally
 * sets up the https://github.com/simd-everywhere/simde in various ways, and defines some
 * macros which, especiall with the introduction of windows arm64ec builds (which define
 * the sse intrinsics, but define them as emulation points) allows us to actually get
 * native code on all platforms
 *
 * This header is controlled by a few important macros
 * SST_SIMD_OMIT_NATIVE_ALIASES - on ARM etc platforms do not eject native aliases
 *
 * This header also defines a few important macros
 */

#if (defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) ||                                  \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define SST_SIMD_NATIVE_X86
#endif

#if defined(_M_ARM64EC)
#define SST_SIMD_ARM64EC
#endif

#if defined(__aarch64__) || defined(__arm64) || defined(__arm64__) || defined(_M_ARM64) ||         \
    defined(_M_ARM64EC)
#define SST_SIMD_ARM64
#endif

/*
 * Include the appropriate intrinsic header
 */
#ifdef SST_SIMD_ARM64EC
#include <intrin.h>
#endif

#ifdef SST_SIMD_NATIVE_X86
#include <emmintrin.h>
#endif

/*
 * Include SIMDE
 */
#ifndef SIMDE_UNAVAILABLE
#ifdef SST_SIMD_ARM64EC
#include <cmath>
#endif

#ifndef SST_SIMD_NATIVE_X86
#ifndef SST_SIMD_OMIT_NATIVE_ALIASES
#define SIMDE_ENABLE_NATIVE_ALIASES
#endif
#endif

#include "simde/x86/sse2.h"
#endif

#if defined(SST_SIMD_NATIVE_X86) || defined(SIMDE_UNAVAILABLE)
#define SIMD_MM(x) _mm_##x
#define SIMD_M128 __m128
#else
#define SIMD_MM(x) simde_mm_##x
#define SIMD_M128 simde__m128
#endif

#endif // SETUP_H
