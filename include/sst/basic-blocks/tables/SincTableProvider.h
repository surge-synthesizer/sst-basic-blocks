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

#ifndef INCLUDE_SST_BASIC_BLOCKS_TABLES_SINCTABLEPROVIDER_H
#define INCLUDE_SST_BASIC_BLOCKS_TABLES_SINCTABLEPROVIDER_H

#include <cstdint>
#include "sst/basic-blocks/dsp/SpecialFunctions.h"

namespace sst::basic_blocks::tables
{
/*
 * Surge and ShortCircuit use windowed sincs for many of their interpolations
 * in a variety of contexts. This class provides a unified way to generate
 * them and provides pointers to the native layouts the DSP and other code
 * coming from those programs uses.
 */
struct SurgeSincTableProvider
{
    static constexpr int FIRipol_M = 256;
    static constexpr int FIRipol_N = 12;
    static constexpr int FIRipolI16_N = 8;

    float sinctable alignas(16)[(FIRipol_M + 1) * FIRipol_N * 2];
    float sinctable1X alignas(16)[(FIRipol_M + 1) * FIRipol_N];
    int16_t sinctableI16 alignas(16)[(FIRipol_M + 1) * FIRipolI16_N];

    SurgeSincTableProvider()
    {
        float cutoff = 0.455f;
        float cutoff1X = 0.85f;
        float cutoffI16 = 1.0f;
        int j;
        for (j = 0; j < FIRipol_M + 1; j++)
        {
            for (int i = 0; i < FIRipol_N; i++)
            {
                double t =
                    -double(i) + double(FIRipol_N / 2.0) + double(j) / double(FIRipol_M) - 1.0;
                double val = (float)(dsp::symmetric_blackman(t, FIRipol_N) * cutoff *
                                     dsp::sincf(cutoff * t));
                double val1X = (float)(dsp::symmetric_blackman(t, FIRipol_N) * cutoff1X *
                                       dsp::sincf(cutoff1X * t));
                sinctable[j * FIRipol_N * 2 + i] = (float)val;
                sinctable1X[j * FIRipol_N + i] = (float)val1X;
            }
        }
        for (j = 0; j < FIRipol_M; j++)
        {
            for (int i = 0; i < FIRipol_N; i++)
            {
                sinctable[j * FIRipol_N * 2 + FIRipol_N + i] =
                    (float)((sinctable[(j + 1) * FIRipol_N * 2 + i] -
                             sinctable[j * FIRipol_N * 2 + i]) /
                            65536.0);
            }
        }

        for (j = 0; j < FIRipol_M + 1; j++)
        {
            for (int i = 0; i < FIRipolI16_N; i++)
            {
                double t =
                    -double(i) + double(FIRipolI16_N / 2.0) + double(j) / double(FIRipol_M) - 1.0;
                double val = (float)(dsp::symmetric_blackman(t, FIRipolI16_N) * cutoffI16 *
                                     dsp::sincf(cutoffI16 * t));

                sinctableI16[j * FIRipolI16_N + i] = (short)((float)val * 16384.f);
            }
        }
    }
};

struct ShortcircuitSincTableProvider
{
    static constexpr uint32_t FIRipol_M = 256;
    static constexpr uint32_t FIRipol_N = 16;
    static constexpr uint32_t FIRipolI16_N = 16;
    static constexpr uint32_t FIRoffset = 8;

    void init()
    {
        if (initialized)
            return;

        float cutoff = 0.95f;
        float cutoffI16 = 0.95f;
        for (auto j = 0U; j < FIRipol_M + 1; j++)
        {
            for (auto i = 0U; i < FIRipol_N; i++)
            {
                double t =
                    -double(i) + double(FIRipol_N / 2.0) + double(j) / double(FIRipol_M) - 1.0;
                double val = (float)(dsp::symmetric_kaiser(t, FIRipol_N, 5.0) * cutoff *
                                     dsp::sincf(cutoff * t));

                SincTableF32[j * FIRipol_N + i] = val;
            }
        }
        for (auto j = 0U; j < FIRipol_M; j++)
        {
            for (auto i = 0U; i < FIRipol_N; i++)
            {
                SincOffsetF32[j * FIRipol_N + i] = (float)((SincTableF32[(j + 1) * FIRipol_N + i] -
                                                            SincTableF32[j * FIRipol_N + i]) *
                                                           (1.0 / 65536.0));
            }
        }

        for (auto j = 0U; j < FIRipol_M + 1; j++)
        {
            for (auto i = 0U; i < FIRipolI16_N; i++)
            {
                double t =
                    -double(i) + double(FIRipolI16_N / 2.0) + double(j) / double(FIRipol_M) - 1.0;
                double val = (float)(dsp::symmetric_kaiser(t, FIRipol_N, 5.0) * cutoffI16 *
                                     dsp::sincf(cutoffI16 * t));

                SincTableI16[j * FIRipolI16_N + i] = val * 16384;
            }
        }
        for (auto j = 0U; j < FIRipol_M; j++)
        {
            for (auto i = 0U; i < FIRipolI16_N; i++)
            {
                SincOffsetI16[j * FIRipolI16_N + i] =
                    (SincTableI16[(j + 1) * FIRipolI16_N + i] - SincTableI16[j * FIRipolI16_N + i]);
            }
        }

        initialized = true;
    }

    // TODO Rename these when i'm all done
    float SincTableF32[(FIRipol_M + 1) * FIRipol_N];
    float SincOffsetF32[(FIRipol_M)*FIRipol_N];
    short SincTableI16[(FIRipol_M + 1) * FIRipol_N];
    short SincOffsetI16[(FIRipol_M)*FIRipol_N];

  private:
    bool initialized{false};
};
} // namespace sst::basic_blocks::tables

#endif // SURGE_SINCTABLEPROVIDER_H
