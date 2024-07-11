/*
 * Shortcircuit XT - a Surge Synth Team product
 *
 * A fully featured creative sampler, available as a standalone
 * and plugin for multiple platforms.
 *
 * Copyright 2019 - 2024, Various authors, as described in the github
 * transaction log.
 *
 * ShortcircuitXT is released under the Gnu General Public Licence
 * V3 or later (GPL-3.0-or-later). The license is found in the file
 * "LICENSE" in the root of this repository or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Individual sections of code which comprises ShortcircuitXT in this
 * repository may also be used under an MIT license. Please see the
 * section  "Licensing" in "README.md" for details.
 *
 * ShortcircuitXT is inspired by, and shares code with, the
 * commercial product Shortcircuit 1 and 2, released by VemberTech
 * in the mid 2000s. The code for Shortcircuit 2 was opensourced in
 * 2020 at the outset of this project.
 *
 * All source for ShortcircuitXT is available at
 * https://github.com/surge-synthesizer/shortcircuit-xt
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
        : g(std::chrono::system_clock::now().time_since_epoch().count()), pm1(-1.f, 1.f),
          z1(0.f, 1.f), gauss(0.5f, .33333f), u32(0, 0xFFFFFFFF)
    {
    }

    inline float uniformZeroToOne() { return z1(g); }
    inline float uniformPlusMinusOne() { return pm1(g); }
    
    inline float uniformInRange(const float min, const float max)
    {
        return min + uniformZeroToOne() * (max - min);
    }

    inline float halfNormalInRange(const float min, const float max)
    {
        return min + fabsf(gauss(g)) * (max - min);
    }
    inline float normalInRange(const float min, const float max)
    {
        return min + gauss(g)  * (max - min);
    }
    
    inline uint32_t uniformU32() { return u32(g); }
    
    inline int uniformIntInRange(const int min, const int max)
    {
        std::uniform_int_distribution<int> intdist(min, max);
        return intdist(g);
    }

  private:
    std::minstd_rand g;
    std::uniform_real_distribution<float> pm1, z1;
    std::normal_distribution<float> gauss;
    std::uniform_int_distribution<uint32_t> u32;
};
} // sst::basic_blocks::dsp

#endif // SST_RNG_H
