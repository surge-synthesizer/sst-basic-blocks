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

#include <random>
#include <chrono>

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_RNG_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_RNG_H

namespace sst::basic_blocks::dsp
{
struct RNG
{
    RNG()
        : g(std::chrono::system_clock::now().time_since_epoch().count()), dg(525600 + 8675309),
          pm1(-1.f, 1.f), z1(0.f, 1.f), gauss(0.f, .33333f), u32(0, 0xFFFFFFFF), b(.5)
    {
    }

    RNG(uint32_t seed)
        : g(seed), dg(525600 + 8675309), pm1(-1.f, 1.f), z1(0.f, 1.f), gauss(0.f, .33333f),
          u32(0, 0xFFFFFFFF), b(.5)
    {
    }

    void reseedWithClock() { g.seed(std::chrono::system_clock::now().time_since_epoch().count()); }

    void reseed(uint32_t seed) { g.seed(seed); }

    inline float unif01() { return z1(g); }
    inline float unifPM1() { return pm1(g); }
    inline float unif(const float min, const float max) { return min + unif01() * (max - min); }

    inline float half01() { return fabsf(gauss(g)); }
    inline float normPM1() { return gauss(g); }
    inline float half(const float min, const float max)
    {
        return min + fabsf(gauss(g)) * (max - min);
    }
    inline float norm(const float min, const float max)
    {
        return min + (gauss(g) * 0.5f + 0.5f) * (max - min);
    }

    inline uint32_t unifU32() { return u32(g); }

    inline int unifInt(const int min, const int max)
    {
        std::uniform_int_distribution<int> intdist(min, max - 1);
        return intdist(g);
    } // clang-format problem? Why?

    inline bool boolean() { return b(g); }

    inline float forDisplay() { return pm1(dg); }

  private:
    std::minstd_rand g;  // clock-seeded audio-thread gen
    std::minstd_rand dg; // fixed-seed ui-thread gen (used by SimpleLFO)
    std::uniform_real_distribution<float> pm1, z1;
    std::normal_distribution<float> gauss;
    std::uniform_int_distribution<uint32_t> u32;
    std::bernoulli_distribution b;
};
} // namespace sst::basic_blocks::dsp

#endif // SST_RNG_H
