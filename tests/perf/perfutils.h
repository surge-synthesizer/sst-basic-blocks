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

#ifndef SST_BASIC_BLOCK_TESTS_PERF_PERFUTILS_H
#define SST_BASIC_BLOCK_TESTS_PERF_PERFUTILS_H

#include <iostream>
#include <chrono>
#include <string>

namespace perf
{
struct TimeGuard
{
    std::string m;
    int d;
    std::chrono::high_resolution_clock::time_point t;
    TimeGuard(const std::string &msg, const std::string &f, int l, int divisor = 1) : d(divisor)
    {
        m = f + ":" + std::to_string(l) + " " + msg;
        t = std::chrono::high_resolution_clock::now();
    }
    ~TimeGuard()
    {
        auto e = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(e - t).count();
        std::cout << m << " " << us << " us; pct=" << 100.0 * us / d << "%" << std::endl;
    }
};
}; // namespace perf

#endif