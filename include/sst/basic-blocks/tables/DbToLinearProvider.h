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

#ifndef INCLUDE_SST_BASIC_BLOCKS_TABLES_DBTOLINEARPROVIDER_H
#define INCLUDE_SST_BASIC_BLOCKS_TABLES_DBTOLINEARPROVIDER_H

#include <cmath>

namespace sst::basic_blocks::tables
{
struct DbToLinearProvider
{
    static constexpr size_t nPoints{512};
    static_assert(!(nPoints & (nPoints - 1)));

    void init()
    {
        for (auto i = 0U; i < nPoints; i++)
        {
            table_dB[i] = powf(10.f, 0.05f * ((float)i - 384.f));
        }
    }
    float dbToLinear(float db) const
    {
        db += 384;
        int e = (int)db;
        float a = db - (float)e;

        return (1.f - a) * table_dB[e & (nPoints - 1)] + a * table_dB[(e + 1) & (nPoints - 1)];
    }

  private:
    float table_dB[nPoints];
};
} // namespace sst::basic_blocks::tables
#endif // SHORTCIRCUITXT_DBTOLINEARPROVIDER_H
