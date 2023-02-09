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


#ifndef SST_BASIC_BLOCKS_DSP_PANLAWS_H
#define SST_BASIC_BLOCKS_DSP_PANLAWS_H

#include <cmath>

namespace sst::basic_blocks::dsp::pan_laws
{
typedef float panmatrix_t[4]; // L R RinL LinR

// These are shamelessly borrowed/copied/adapted from MixMaster. Thanks marc!
inline void sinCos(float &destSin, float &destCos, float theta)
{
    destSin = theta + std::pow(theta, 3) * (-0.166666667f + theta * theta * 0.00833333333f);
    theta = float(M_PI * 0.5) - theta;
    destCos = theta + std::pow(theta, 3) * (-0.166666667f + theta * theta * 0.00833333333f);
}
static constexpr float sqrt2{1.414213562373095};
inline void sinCosSqrt2(float &destSin, float &destCos, float theta)
{
    sinCos(destSin, destCos, theta);
    destSin *= sqrt2;
    destCos *= sqrt2;
}

inline void monoLinear(float pan, panmatrix_t &res)
{
    res[3] = pan * 2.f;
    res[0] = 2.f - res[3];
}
inline void monoEqualPower(float pan, panmatrix_t &res)
{
    res[1] = 0;
    res[2] = 0;
    sinCosSqrt2(res[3], res[0], pan * float(M_PI * 0.5));
}

inline void stereoEqualPower(float pan, panmatrix_t &res)
{
    if (pan == 0.5f)
    {
        res[0] = 1;
        res[1] = 1;
        res[2] = 0;
        res[3] = 0;
    }
    else if (pan > 0.5f)
    {
        res[1] = 1.f;
        res[2] = 0.f;
        sinCos(res[3], res[0], (pan - 0.5f) * float(M_PI));
    }
    else
    {
        sinCos(res[1], res[2], pan * float(M_PI));
        res[0] = 1.0f;
        res[3] = 0.0f;
    }
}
} // namespace sst::surgext_rack::dsp::pan_laws
#endif // SURGEXTRACK_PANLAWS_H
