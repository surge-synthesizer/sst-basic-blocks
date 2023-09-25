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
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * All source in sst-basic-blocks available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_DPWSAWPULSEOSCILLATOR_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_DPWSAWPULSEOSCILLATOR_H

#include <cstdint>
#include <cmath>
#include "Lag.h"

namespace sst::basic_blocks::dsp
{
/*
 * Use a cubic integrated saw and second derive it at
 * each point. This is basically the math I worked
 * out for the surge modern oscillator. The cubic function
 * which gives a clean saw is phase^3 / 6 - phase / 6.
 * Evaluate it at 3 points and then differentiate it like
 * we do in Surge Modern. The waveform is the same both
 * channels.
 */
struct DPWSawOscillator
{
    void setFrequency(double freqInHz, double sampleRateInv)
    {
        dPhase.newValue(freqInHz * sampleRateInv);
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
        auto res = valueAt(phase, dPhase.v);

        phase += dPhase.v;
        if (phase > 1)
            phase -= 1;

        dPhase.process();
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
    SurgeLag<double, true> dPhase;
};

struct DPWPulseOscillator
{
    DPWPulseOscillator()
    {
        pulseWidth.startValue(0.5);
        pulseWidth.instantize();
    }

    void setFrequency(double freqInHz, double sampleRateInv)
    {
        dPhase.newValue(freqInHz * sampleRateInv);
    }

    void setPulseWidth(double pw) { pulseWidth.newValue(pw); }

    inline double step()
    {
        auto res0 = DPWSawOscillator::valueAt(phase, dPhase.v);
        auto np = phase + pulseWidth.v;
        np = np - (np > 1);
        auto res1 = DPWSawOscillator::valueAt(np, dPhase.v);

        auto res = res0 - res1;

        phase += dPhase.v;
        if (phase > 1)
            phase -= 1;

        dPhase.process();
        pulseWidth.process();
        return res;
    }

    double phase;
    SurgeLag<double, true> dPhase;
    SurgeLag<double, true> pulseWidth;
};
} // namespace sst::basic_blocks::dsp

#endif // INCLUDE_SST_BASIC_BLOCKS_DSP_DPWSAWPULSEOSCILLATOR_H
