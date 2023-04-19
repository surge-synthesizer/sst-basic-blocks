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

#ifndef SST_BASIC_BLOCKS_TABLES_DBTOLINEARPROVIDER_H
#define SST_BASIC_BLOCKS_TABLES_DBTOLINEARPROVIDER_H

namespace sst::basic_blocks::tables
{
struct DbToLinearProvider
{
    static constexpr size_t nPoints{512};
    static_assert(!(nPoints & (nPoints - 1)));

    void init()
    {
        for (int i = 0; i < nPoints; i++)
        {
            table_dB[i] = powf(10.f, 0.05f * ((float)i - 384.f));
        }
    }
    float dbToLinear(float db)
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
