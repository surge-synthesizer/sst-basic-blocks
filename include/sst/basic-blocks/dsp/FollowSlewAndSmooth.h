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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_FOLLOWSLEWANDSMOOTH_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_FOLLOWSLEWANDSMOOTH_H

#include <algorithm>
#include <utility>

namespace sst::basic_blocks::dsp
{
/*
 * An LPF on the abs of the signal; equivalent roughly
 * to the BogAudio PucketEnvelopeFollower
 */
struct LowPassEnvelopeFollower
{
    float yp[2]{0, 0}, xp[2]{0, 0};
    float a[3]{1., 0, 0}, b[3]{1., 0, 0};
    float xc[3]{0, 0, 0}, yc[3]{0, 0, 0};
    LowPassEnvelopeFollower() { reset(); }

    void setSensitivity01(float sens01, float sampleRate)
    {
        static constexpr float maxCutoff = 10000.0f;
        static constexpr float minCutoff = 100.0f;
        auto s01 = std::clamp(sens01, 0.f, 1.f);
        auto co = (maxCutoff - minCutoff) * s01 + minCutoff;

        // todo - setup coefficients here
        static constexpr float Q{0.001};
        auto omega = 2 * M_PI * co / sampleRate;
        auto alpha = sin(omega) / (2 * Q);
        auto cosw = cos(omega);

        a[0] = 1 + alpha;
        a[1] = -2 * cosw;
        a[2] = 1 - alpha;
        b[0] = (1 - cosw) / 2;
        b[1] = 2 * b[0];
        b[2] = b[0];
        resetCoeff();
    }

    void reset()
    {
        a[0] = 1.;
        a[1] = 0.;
        a[2] = 0.;
        b[0] = 1.;
        b[1] = 0.;
        b[2] = 0.;
        yp[0] = 0;
        yp[1] = 0;
        xp[0] = 0;
        xp[1] = 0;
        resetCoeff();
    }

    void resetCoeff()
    {
        auto oa0 = 1.0 / a[0];
        xc[0] = b[0] * oa0;
        xc[1] = b[1] * oa0;
        xc[2] = b[2] * oa0;

        yc[0] = 0;
        yc[1] = -a[1] * oa0;
        yc[2] = -a[2] * oa0;
    }

    float step(float x)
    {
        x = std::fabs(x);
        auto r = xc[0] * x + xc[1] * xp[0] + xc[2] * xp[1] + yc[1] * yp[0] + yc[2] * yp[1];

        yp[1] = yp[0];
        yp[0] = r;
        xp[1] = xp[0];
        xp[0] = x;

        return r;
    }
};

struct SlewLimiter
{
    float delta{0};
    float last{0};

    void setParams(float ms, float range, float sampleRate)
    {
        delta = range / ((ms / 1000.0f) * sampleRate);
    }

    void setLast(float l) { last = l; }
    void reset() { setLast(0.f); }

    float step(float x)
    {
        float res = x;
        if (x > last)
        {
            res = std::min(last + delta, x);
        }
        else if (x < last)
        {
            res = std::max(last - delta, x);
        }

        last = res;
        return res;
    }
};

struct RunningAverage
{
    float *storage{nullptr};
    size_t nPoints{0};
    size_t head{0}, tail{0};
    float avg{0}, oneOverN{1};
    RunningAverage() {}

    void setStorage(float *s, size_t np)
    {
        reset();
        storage = s;
        nPoints = np;
        oneOverN = 1.0 / (nPoints - 1);
    }

    void reset()
    {
        std::fill(storage, storage + nPoints, 0.f);
        head = 0;
        tail = 1;
        avg = 0.f;
    }

    float step(float x)
    {
        storage[head] = x;

        avg += (storage[head] - storage[tail]) * oneOverN;
        head++;
        if (head >= nPoints)
            head = 0;
        tail++;
        if (tail >= nPoints)
            tail = 0;

        return avg;
    }
};
} // namespace sst::basic_blocks::dsp

#endif // BACONMUSIC_ENVELOPEFOLLOWER_H
