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

#ifndef SST_BASIC_BLOCKS_DSP_LANCZOSRESAMPLER_H
#define SST_BASIC_BLOCKS_DSP_LANCZOSRESAMPLER_H


#include <algorithm>
#include <utility>
#include <cmath>
#include <cstring>
#include "sst/basic-blocks/mechanics/simd-ops.h"

namespace sst::basic_blocks::dsp
{
/*
 * See https://en.wikipedia.org/wiki/Lanczos_resampling
 */

template <int blockSize> struct LanczosResampler
{
    static constexpr size_t A = 4;
    static constexpr size_t BUFFER_SZ = 4096;
    static constexpr size_t filterWidth = A * 2;
    static constexpr size_t tableObs = 8192;
    static constexpr double dx = 1.0 / (tableObs);

    // Fixme: Make this static and shared
    static float lanczosTable alignas(16)[tableObs + 1][filterWidth], lanczosTableDX
        alignas(16)[tableObs + 1][filterWidth];
    static bool tablesInitialized;

    // This is a stereo resampler
    float input[2][BUFFER_SZ * 2];
    int wp = 0;
    float sri, sro;
    double phaseI, phaseO, dPhaseI, dPhaseO;

    inline double kernel(double x)
    {
        if (fabs(x) < 1e-7)
            return 1;
        return A * std::sin(M_PI * x) * std::sin(M_PI * x / A) / (M_PI * M_PI * x * x);
    }

    LanczosResampler(float inputRate, float outputRate) : sri(inputRate), sro(outputRate)
    {
        phaseI = 0;
        phaseO = 0;

        dPhaseI = 1.0;
        dPhaseO = sri / sro;

        memset(input, 0, 2 * BUFFER_SZ * sizeof(float));
        if (!tablesInitialized)
        {
            for (size_t t = 0; t < tableObs + 1; ++t)
            {
                double x0 = dx * t;
                for (size_t i = 0; i < filterWidth; ++i)
                {
                    double x = x0 + i - A;
                    lanczosTable[t][i] = kernel(x);
                }
            }
            for (size_t t = 0; t < tableObs; ++t)
            {
                for (size_t i = 0; i < filterWidth; ++i)
                {
                    lanczosTableDX[t][i] =
                        lanczosTable[(t + 1) & (tableObs - 1)][i] - lanczosTable[t][i];
                }
            }
            for (size_t i = 0; i < filterWidth; ++i)
            {
                // Wrap at the end - deriv is the same
                lanczosTableDX[tableObs][i] = lanczosTable[0][i];
            }
            tablesInitialized = true;
        }
    }

    inline void push(float fL, float fR)
    {
        input[0][wp] = fL;
        input[0][wp + BUFFER_SZ] = fL; // this way we can always wrap
        input[1][wp] = fR;
        input[1][wp + BUFFER_SZ] = fR;
        wp = (wp + 1) & (BUFFER_SZ - 1);
        phaseI += dPhaseI;
    }

    inline void readZOH(double xBack, float &L, float &R) const
    {
        double p0 = wp - xBack;
        int idx0 = (int)p0;
        idx0 = (idx0 + BUFFER_SZ) & (BUFFER_SZ - 1);
        if (idx0 <= (int)A)
            idx0 += BUFFER_SZ;
        L = input[0][idx0];
        R = input[1][idx0];
    }

    inline void readLin(double xBack, float &L, float &R) const
    {
        double p0 = wp - xBack;
        int idx0 = (int)p0;
        float frac = p0 - idx0;
        idx0 = (idx0 + BUFFER_SZ) & (BUFFER_SZ - 1);
        if (idx0 <= (int)A)
            idx0 += BUFFER_SZ;
        L = (1.0 - frac) * input[0][idx0] + frac * input[0][idx0 + 1];
        R = (1.0 - frac) * input[1][idx0] + frac * input[1][idx0 + 1];
    }

    inline void read(double xBack, float &L, float &R) const
    {
        double p0 = wp - xBack;
        int idx0 = floor(p0);
        double off0 = 1.0 - (p0 - idx0);

        idx0 = (idx0 + BUFFER_SZ) & (BUFFER_SZ - 1);
        idx0 += (idx0 <= (int)A) * BUFFER_SZ;

        double off0byto = off0 * tableObs;
        int tidx = (int)(off0byto);
        double fidx = (off0byto - tidx);

        auto fl = _mm_set1_ps((float)fidx);
        auto f0 = _mm_load_ps(&lanczosTable[tidx][0]);
        auto df0 = _mm_load_ps(&lanczosTableDX[tidx][0]);

        f0 = _mm_add_ps(f0, _mm_mul_ps(df0, fl));

        auto f1 = _mm_load_ps(&lanczosTable[tidx][4]);
        auto df1 = _mm_load_ps(&lanczosTableDX[tidx][4]);
        f1 = _mm_add_ps(f1, _mm_mul_ps(df1, fl));

        auto d0 = _mm_loadu_ps(&input[0][idx0 - A]);
        auto d1 = _mm_loadu_ps(&input[0][idx0]);
        auto rv = _mm_add_ps(_mm_mul_ps(f0, d0), _mm_mul_ps(f1, d1));
        L = mechanics::sum_ps_to_float(rv);

        d0 = _mm_loadu_ps(&input[1][idx0 - A]);
        d1 = _mm_loadu_ps(&input[1][idx0]);
        rv = _mm_add_ps(_mm_mul_ps(f0, d0), _mm_mul_ps(f1, d1));
        R = mechanics::sum_ps_to_float(rv);
    }

    inline size_t inputsRequiredToGenerateOutputs(size_t desiredOutputs) const
    {
        /*
         * So (phaseI + dPhaseI * res - phaseO - dPhaseO * desiredOutputs) * sri > A + 1
         *
         * Use the fact that dPhaseI = sri and find
         * res > (A+1) - (phaseI - phaseO + dPhaseO * desiredOutputs) * sri
         */
        double res = A + 1 - (phaseI - phaseO - dPhaseO * desiredOutputs);

        return (size_t)std::max(res + 1, 0.0); // Check this calculation
    }

    size_t populateNext(float *fL, float *fR, size_t max);

    /*
     * This is a dangerous but efficient function which more quickly
     * populates blockSize or blockSize << 1 worth of items, but assumes you have
     * checked the range.
     */
    void populateNextBlockSize(float *fL, float *fR);
    void populateNextBlockSizeOS(float *fL, float *fR);

    inline void advanceReadPointer(size_t n) { phaseO += n * dPhaseO; }
    inline void snapOutToIn()
    {
        phaseO = 0;
        phaseI = 0;
    }

    inline void renormalizePhases()
    {
        phaseI -= phaseO;
        phaseO = 0;
    }
};

template <int bs>
float LanczosResampler<bs>::lanczosTable alignas(
    16)[LanczosResampler<bs>::tableObs + 1][LanczosResampler<bs>::filterWidth];
template <int bs>
float LanczosResampler<bs>::lanczosTableDX alignas(
    16)[LanczosResampler<bs>::tableObs + 1][LanczosResampler<bs>::filterWidth];

template <int bs> bool LanczosResampler<bs>::tablesInitialized{false};

template <int bs> inline size_t LanczosResampler<bs>::populateNext(float *fL, float *fR, size_t max)
{
    int populated = 0;
    while (populated < max && (phaseI - phaseO) > A + 1)
    {
        read((phaseI - phaseO), fL[populated], fR[populated]);
        phaseO += dPhaseO;
        populated++;
    }
    return populated;
}

template <int bs> void LanczosResampler<bs>::populateNextBlockSize(float *fL, float *fR)
{
    double r0 = phaseI - phaseO;
    for (int i = 0; i < bs; ++i)
    {
        read(r0 - i * dPhaseO, fL[i], fR[i]);
    }
    phaseO += (bs << 1) * dPhaseO;
}

template <int bs> void LanczosResampler<bs>::populateNextBlockSizeOS(float *fL, float *fR)
{
    double r0 = phaseI - phaseO;
    for (int i = 0; i < bs << 1; ++i)
    {
        read(r0 - i * dPhaseO, fL[i], fR[i]);
    }
    phaseO += (bs << 1) * dPhaseO;
}
}
#endif
