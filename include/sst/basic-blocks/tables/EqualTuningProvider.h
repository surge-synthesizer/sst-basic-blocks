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

#ifndef INCLUDE_SST_BASIC_BLOCKS_TABLES_EQUALTUNINGPROVIDER_H
#define INCLUDE_SST_BASIC_BLOCKS_TABLES_EQUALTUNINGPROVIDER_H

#include <cstddef>
#include <cstdint>

namespace sst::basic_blocks::tables
{
struct EqualTuningProvider
{
    void init()
    {
        for (auto i = 0U; i < tuning_table_size; i++)
        {
            table_pitch[i] = powf(2.f, ((float)i - 256.f) * (1.f / 12.f));
            table_pitch_inv[i] = 1.f / table_pitch[i];
        }

        for (auto i = 0U; i < 1001; ++i)
        {
            double twelths = i * 1.0 / 12.0 / 1000.0;
            table_two_to_the[i] = pow(2.0, twelths);
            table_two_to_the_minus[i] = pow(2.0, -twelths);
        }
    }

    /**
     * note is float offset from note 69 / A440
     * return is 2^(note * 12), namely frequency / 440.0
     */
    float note_to_pitch(float note) const
    {
        auto x = std::clamp(note + 256, 1.e-4f, tuning_table_size - (float)1.e-4);
        // x += 256;
        auto e = (int16_t)x;
        float a = x - (float)e;

        float pow2pos = a * 1000.0;
        int pow2idx = (int)pow2pos;
        float pow2frac = pow2pos - pow2idx;
        float pow2v =
            (1 - pow2frac) * table_two_to_the[pow2idx] + pow2frac * table_two_to_the[pow2idx + 1];
        return table_pitch[e] * pow2v;
    }

  protected:
    static constexpr size_t tuning_table_size = 512;
    float table_pitch alignas(16)[tuning_table_size];
    float table_pitch_inv alignas(16)[tuning_table_size];
    float table_note_omega alignas(16)[2][tuning_table_size];
    // 2^0 -> 2^+/-1/12th. See comment in note_to_pitch
    float table_two_to_the alignas(16)[1001];
    float table_two_to_the_minus alignas(16)[1001];
};
extern EqualTuningProvider equalTuning;
} // namespace sst::basic_blocks::tables

#endif // __SCXT_EQUAL_H
