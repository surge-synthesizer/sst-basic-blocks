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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_FXMODCONTROL_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_FXMODCONTROL_H

#include <cmath>
#include <utility>

#include "sst/basic-blocks/dsp/BlockInterpolators.h"
#include "sst/basic-blocks/dsp/RNG.h"

namespace sst::basic_blocks::modulators
{

template <int blockSize> struct FXModControl
{
  public:
    float samplerate{0}, samplerate_inv{0};
    FXModControl(float sr, float sri) : samplerate(sr), samplerate_inv(sri)
    {
        lfophase = 0.0f;
        lfosandhtarget = 0.0f;

        for (int i = 0; i < LFO_TABLE_SIZE; ++i)
            sin_lfo_table[i] = sin(2.0 * M_PI * i / LFO_TABLE_SIZE);
    }
    FXModControl() : FXModControl(0, 0) {}

    sst::basic_blocks::dsp::RNG rng;

    void setSampleRate(double sr)
    {
        samplerate = sr;
        samplerate_inv = 1.0 / sr;
    }

    enum mod_waves
    {
        mod_sine = 0,
        mod_tri,
        mod_saw,
        mod_noise,
        mod_snh,
        mod_square,
    };

    inline void pre_process(int mwave, float rate, float depth_val, float phase_offset)
    {
        assert(samplerate > 1000);
        bool lforeset = false;
        bool rndreset = false;
        float lfoout = lfoval.v;
        float phofs = fmod(fabs(phase_offset), 1.0);
        float thisrate = std::max(0.f, rate);
        float thisphase;

        if (thisrate > 0)
        {
            lfophase += thisrate;

            if (lfophase > 1)
            {
                lfophase = fmod(lfophase, 1.0);
            }

            thisphase = lfophase + phofs;
        }
        else
        {
            thisphase = phofs;

            if (mwave == mod_noise || mwave == mod_snh)
            {
                thisphase *= 16.f;

                if ((int)thisphase != (int)lfophase)
                {
                    rndreset = true;
                    lfophase = (float)((int)thisphase);
                }
            }
        }

        if (thisphase > 1)
        {
            thisphase = fmod(thisphase, 1.0);
        }

        /* We want to catch the first time that thisphase trips over the threshold. There's a couple
         * of ways to do this (like have a state variable), but this should work just as well. */
        if ((thisrate > 0 && thisphase - thisrate <= 0) || (thisrate == 0 && rndreset))
        {
            lforeset = true;
        }

        switch (mwave)
        {
        case mod_sine:
        {
            float ps = thisphase * LFO_TABLE_SIZE;
            int psi = (int)ps;
            float psf = ps - psi;
            int psn = (psi + 1) & LFO_TABLE_MASK;

            lfoout = sin_lfo_table[psi] * (1.0 - psf) + psf * sin_lfo_table[psn];

            lfoval.newValue(lfoout);

            break;
        }
        case mod_tri:
        {
            lfoout = (2.f * fabs(2.f * thisphase - 1.f) - 1.f);

            lfoval.newValue(lfoout);

            break;
        }
        case mod_saw: // Gentler than a pure saw, more like a heavily skewed triangle
        {
            auto cutAt = 0.98f;
            float usephase;

            if (thisphase < cutAt)
            {
                usephase = thisphase / cutAt;
                lfoout = usephase * 2.0f - 1.f;
            }
            else
            {
                usephase = (thisphase - cutAt) / (1.0 - cutAt);
                lfoout = (1.0 - usephase) * 2.f - 1.f;
            }

            lfoval.newValue(lfoout);

            break;
        }
        case mod_square: // Gentler than a pure square, more like a trapezoid
        {
            auto cutOffset = 0.02f;
            auto m = 2.f / cutOffset;
            auto c2 = cutOffset / 2.f;

            if (thisphase < 0.5f - c2)
            {
                lfoout = 1.f;
            }
            else if ((thisphase >= 0.5 + c2) && (thisphase <= 1.f - cutOffset))
            {
                lfoout = -1.f;
            }
            else if ((thisphase > 0.5 - c2) && (thisphase < 0.5 + c2))
            {
                lfoout = -m * thisphase + (m / 2);
            }
            else
            {
                lfoout = (m * thisphase) - (2 * m) + m + 1;
            }

            lfoval.newValue(lfoout);

            break;
        }
        case mod_snh:   // Sample & Hold random
        case mod_noise: // noise (Sample & Glide smoothed random)
        {
            if (lforeset)
            {
                lfosandhtarget = rng.unifPM1();
            }

            if (mwave == mod_noise)
            {
                // FIXME - exponential creep up. We want to get there in time related to our rate
                auto cv = lfoval.v;

                // thisphase * 0.98 prevents a glitch when LFO rate is disabled
                // and phase offset is 1, which constantly retriggers S&G
                thisrate = (rate == 0) ? thisphase * 0.98 : thisrate;

                auto diff = (lfosandhtarget - cv) * thisrate * 2;

                if (thisrate >= 0.98f) // this means we skip all the time
                    lfoval.newValue(lfosandhtarget);
                else
                    lfoval.newValue(std::clamp(cv + diff, -1.f, 1.f));
            }
            else
            {
                lfoval.newValue(lfosandhtarget);
            }

            break;
        }
        }

        depth.newValue(depth_val);
    }

    inline float value() const noexcept { return lfoval.v * depth.v; }

    inline void post_process()
    {
        lfoval.process();
        depth.process();
    }

  private:
    dsp::lipol<float, blockSize, true> lfoval{};
    dsp::lipol<float, blockSize, true> depth{};
    float lfophase;
    float lfosandhtarget;

    static constexpr int LFO_TABLE_SIZE = 8192;
    static constexpr int LFO_TABLE_MASK = LFO_TABLE_SIZE - 1;
    float sin_lfo_table[LFO_TABLE_SIZE]{};
};
} // namespace sst::basic_blocks::modulators

#endif // SURGE_FXMODCONTROL_H
