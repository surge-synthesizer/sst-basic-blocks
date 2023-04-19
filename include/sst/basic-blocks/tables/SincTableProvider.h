/*
* sst-basic-blocks - a Surge Synth Team product
*
* Provides basic tools and blocks for use on the audio thread in
* synthesis, including basic DSP and modulation functions
*
* Copyright 2019 - 2023, Various authors, as described in the github
* transaction log.
*
* sst-basic-blocks is released under the Gnu General Public Licence
* V3 or later (GPL-3.0-or-later). The license is found in the file
* "LICENSE" in the root of this repository or at
* https://www.gnu.org/licenses/gpl-3.0.en.html
*
* All source for sst-basic-blocks is available at
* https://github.com/surge-synthesizer/sst-basic-blocks
*/

#ifndef SST_BASIC_BLOCKS_TABLES_SINCTABLEPROVIDER_H
#define SST_BASIC_BLOCKS_TABLES_SINCTABLEPROVIDER_H

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
                double t = -double(i) + double(FIRipol_N / 2.0) + double(j) / double(FIRipol_M) - 1.0;
                double val = (float)(dsp::symmetric_blackman(t, FIRipol_N) * cutoff * dsp::sincf(cutoff * t));
                double val1X =
                    (float)(dsp::symmetric_blackman(t, FIRipol_N) * cutoff1X * dsp::sincf(cutoff1X * t));
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
                double val =
                    (float)(dsp::symmetric_blackman(t, FIRipolI16_N) * cutoffI16 * dsp::sincf(cutoffI16 * t));

                sinctableI16[j * FIRipolI16_N + i] = (short)((float)val * 16384.f);
            }
        }
    }

};

struct ShortcircuitSincTableProvider
{

};
}

#endif // SURGE_SINCTABLEPROVIDER_H
