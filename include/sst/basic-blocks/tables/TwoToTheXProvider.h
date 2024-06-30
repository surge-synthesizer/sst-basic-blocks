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

#ifndef INCLUDE_SST_BASIC_BLOCKS_TABLES_TWOTOTHEXPROVIDER_H
#define INCLUDE_SST_BASIC_BLOCKS_TABLES_TWOTOTHEXPROVIDER_H

#include <cmath>
#include <iostream>

namespace sst::basic_blocks::tables
{
struct TwoToTheXProvider
{
    bool isInit{false};
    static constexpr int intBase{-15};
    static constexpr int providerRange{32};

    float baseValue[providerRange]{};

    static constexpr int nInterp{1001};
    float table_two_to_the alignas(16)[nInterp];

    void init()
    {
        if (isInit)
            return;

        for (int i = 0; i < providerRange; i++)
        {
            baseValue[i] = pow(2.0, i + intBase);
        }

        for (auto i = 0U; i < nInterp; ++i)
        {
            double frac = i * 1.0 / (nInterp - 1);
            table_two_to_the[i] = pow(2.0, frac);
        }

        isInit = true;
    }

    float twoToThe(float x) const
    {
        auto xb = std::clamp(x - intBase, 0.f, providerRange * 1.f);
        auto e = (int16_t)xb;
        float a = xb - (float)e;

        float pow2pos = a * (nInterp - 1);
        int pow2idx = (int)pow2pos;
        float pow2frac = pow2pos - pow2idx;
        float pow2v =
            (1 - pow2frac) * table_two_to_the[pow2idx] + pow2frac * table_two_to_the[pow2idx + 1];

        return baseValue[e] * pow2v;
    }
};
} // namespace sst::basic_blocks::tables

#endif // SST_BASIC_BLOCKS_TWOTOTHEXPROVIDER_H
