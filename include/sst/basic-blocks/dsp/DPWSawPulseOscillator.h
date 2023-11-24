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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_DPWSAWPULSEOSCILLATOR_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_DPWSAWPULSEOSCILLATOR_H

#include <cstdint>
#include <cmath>
#include "Lag.h"
#include "BlockInterpolators.h"

namespace sst::basic_blocks::dsp
{
struct LagSmoothingStrategy
{
    using smoothValue_t = SurgeLag<double, true>;
    static void setTarget(smoothValue_t &v, float t) { v.newValue(t); }
    static void setValueInstant(smoothValue_t &v, float t)
    {
        v.newValue(t);
        v.instantize();
    }
    static double getValue(smoothValue_t &v) { return v.v; }
    static void process(smoothValue_t &v) { v.process(); }

    static void resetFirstRun(smoothValue_t &v) { v.first_run = true; }
};
template <int blockSize> struct BlockInterpSmoothingStrategy
{
    using smoothValue_t = lipol<double, blockSize, true>;
    static void setTarget(smoothValue_t &v, float t) { v.newValue(t); }
    static void setValueInstant(smoothValue_t &v, float t)
    {
        v.newValue(t);
        v.instantize();
    }
    static double getValue(smoothValue_t &v) { return v.v; }
    static void process(smoothValue_t &v) { v.process(); }
    static void resetFirstRun(smoothValue_t &v) { v.first_run = true; }
};
struct NoSmoothingStrategy
{
    using smoothValue_t = double;
    static void setTarget(smoothValue_t &v, float t) { v = t; }
    static void setValueInstant(smoothValue_t &v, float t) { v = t; }
    static double getValue(smoothValue_t &v) { return v; }
    static void process(smoothValue_t &v) {}

    static void resetFirstRun(smoothValue_t &v) {}
};

/*
 * Use a cubic integrated saw and second derive it at
 * each point. This is basically the math I worked
 * out for the surge modern oscillator. The cubic function
 * which gives a clean saw is phase^3 / 6 - phase / 6.
 * Evaluate it at 3 points and then differentiate it like
 * we do in Surge Modern. The waveform is the same both
 * channels.
 */
template <typename SmoothingStrategy = LagSmoothingStrategy> struct DPWSawOscillator
{
    void retrigger()
    {
        phase = 0;
        SmoothingStrategy::resetFirstRun(dPhase);
    }
    void setFrequency(double freqInHz, double sampleRateInv)
    {
        SmoothingStrategy::setTarget(dPhase, freqInHz * sampleRateInv);
    }

    inline static double valueAt(double p, double dp)
    {
        double res = p * 2 - 1;
        if (p < 3 * dp || p > (1 - 3 * dp))
        {
            double phaseSteps[3];
            for (int q = -1; q <= 1; ++q)
            {
                double ph = p - q * dp;
                ph = ph - std::floor(ph);
                ph = ph * 2 - 1;
                phaseSteps[q + 1] = (ph * ph - 1) * ph / 6.0;
            }

            res = (phaseSteps[0] + phaseSteps[2] - 2 * phaseSteps[1]) / (4 * dp * dp);
        }

        return res;
    }
    inline double step()
    {
        auto res = valueAt(phase, SmoothingStrategy::getValue(dPhase));

        phase += SmoothingStrategy::getValue(dPhase);
        if (phase > 1)
            phase -= 1;

        SmoothingStrategy::process(dPhase);
        return res;
    }

    template <int blockSize> void fillBlock(float out[blockSize])
    {
        for (auto s = 0U; s < blockSize; ++s)
        {
            out[s] = step();
        }
    }

    double phase;
    typename SmoothingStrategy::smoothValue_t dPhase;
};

template <typename SmoothingStrategy = LagSmoothingStrategy> struct DPWPulseOscillator
{
    DPWPulseOscillator() { SmoothingStrategy::setValueInstant(pulseWidth, 0.5); }

    void retrigger()
    {
        phase = 0;
        SmoothingStrategy::resetFirstRun(dPhase);
        SmoothingStrategy::resetFirstRun(pulseWidth);
    }

    void setFrequency(double freqInHz, double sampleRateInv)
    {
        SmoothingStrategy::setTarget(dPhase, freqInHz * sampleRateInv);
    }

    void setPulseWidth(double pw) { SmoothingStrategy::setTarget(pulseWidth, pw); }

    inline double step()
    {
        auto dpv = SmoothingStrategy::getValue(dPhase);
        auto res0 = DPWSawOscillator<SmoothingStrategy>::valueAt(phase, dpv);
        auto np = phase + SmoothingStrategy::getValue(pulseWidth);
        np = np - (np > 1);
        auto res1 = DPWSawOscillator<SmoothingStrategy>::valueAt(np, dpv);

        auto res = res0 - res1;

        phase += dpv;
        if (phase > 1)
            phase -= 1;

        SmoothingStrategy::process(dPhase);
        SmoothingStrategy::process(pulseWidth);
        return res;
    }

    double phase;
    typename SmoothingStrategy::smoothValue_t dPhase;
    typename SmoothingStrategy::smoothValue_t pulseWidth;
};
} // namespace sst::basic_blocks::dsp

#endif // INCLUDE_SST_BASIC_BLOCKS_DSP_DPWSAWPULSEOSCILLATOR_H
