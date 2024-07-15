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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_SIMPLELFO_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_SIMPLELFO_H

#include "sst/basic-blocks/dsp/CorrelatedNoise.h"
#include "sst/basic-blocks/dsp/Interpolators.h"
#include "sst/basic-blocks/dsp/RNG.h"

#include <cmath>
#include <cassert>

namespace sst::basic_blocks::modulators
{

// For context on SRProvider see the ADSRDAHD Envelope
template <typename SRProvider, int BLOCK_SIZE> struct SimpleLFO
{
    SRProvider *srProvider{nullptr};

    sst::basic_blocks::dsp::RNG &objrngRef;
    sst::basic_blocks::dsp::RNG objrng{0}; // this is unused in ext ref so seed only if used
    std::function<float()> urng = []() { return 0; };

    static_assert((BLOCK_SIZE >= 8) & !(BLOCK_SIZE & (BLOCK_SIZE - 1)),
                  "Block size must be power of 2 8 or above.");
    static constexpr float BLOCK_SIZE_INV{1.f / BLOCK_SIZE};

    float rngState[2]{0, 0};
    float rngHistory[4]{0, 0, 0, 0};

    float rngCurrent{0};

    SimpleLFO(SRProvider *s, sst::basic_blocks::dsp::RNG &extRng) : srProvider(s), objrngRef(extRng)
    {
        urng = [this]() -> float { return objrngRef.unifPM1(); };

        for (int i = 0; i < BLOCK_SIZE; ++i)
            outputBlock[i] = 0;

        rngState[0] = urng();
        rngState[1] = urng();
        for (int i = 0; i < 4; ++i)
        {
            rngCurrent = dsp::correlated_noise_o2mk2_suppliedrng(rngState[0], rngState[1], 0, urng);
            rngHistory[3 - i] = rngCurrent;
        }
    }

    //  Move towards this so we can remove the rng member above
    //  [[deprecated("Use the two-arg constructor with an external RNG")]]
    SimpleLFO(SRProvider *s) : srProvider(s), objrngRef(objrng)
    {
        objrng.reseedWithClock();
        urng = [this]() -> float { return objrng.unifPM1(); };

        for (int i = 0; i < BLOCK_SIZE; ++i)
            outputBlock[i] = 0;

        rngState[0] = urng();
        rngState[1] = urng();
        for (int i = 0; i < 4; ++i)
        {
            rngCurrent = dsp::correlated_noise_o2mk2_suppliedrng(rngState[0], rngState[1], 0, urng);
            rngHistory[3 - i] = rngCurrent;
        }
    }

    enum Shape
    {
        SINE,
        RAMP,
        DOWN_RAMP,
        TRI,
        PULSE,
        SMOOTH_NOISE,
        SH_NOISE,
        RANDOM_TRIGGER
    };

    float lastTarget{0};
    float outputBlock[BLOCK_SIZE];
    float phase{0};

    inline float bend1(float x, float d)
    {
        auto a = 0.5 * std::clamp(d, -3.f, 3.f);
        x = x - a * x * x + a;
        x = x - a * x * x + a;
        return x;
    }

    inline void attackForDisplay(const int lshape)
    {
        attack(lshape);

        urng = [this]() -> float { return objrngRef.forDisplay(); };

        for (int i = 0; i < BLOCK_SIZE; ++i)
            outputBlock[i] = 0;

        rngState[0] = urng();
        rngState[1] = urng();
        for (int i = 0; i < 4; ++i)
        {
            rngCurrent = dsp::correlated_noise_o2mk2_suppliedrng(rngState[0], rngState[1], 0, urng);
            rngHistory[3 - i] = rngCurrent;
        }
        lastDPhase = 0;
        amplitude = 1;
    }
    // FIXME - make this work for proper attacks
    inline void attack(const int lshape)
    {
        phase = 0;
        lastDPhase = 0;
        for (int i = 0; i < BLOCK_SIZE; ++i)
            outputBlock[i] = 0;
    }

    float lastDPhase{0};
    inline void applyPhaseOffset(float dPhase)
    {
        if (dPhase != lastDPhase)
        {
            phase += dPhase - lastDPhase;
            if (phase > 1)
                phase -= 1;
        }
        lastDPhase = dPhase;
    }

    float amplitude{1.0};
    inline void setAmplitude(float f) { amplitude = f; }
    int rndTrigCountdown{0};

    inline void freeze()
    {
        for (auto &f : outputBlock)
        {
            f = lastTarget;
        }
    }

    inline void process_block(const float r, const float d, const int lshape, bool reverse = false)
    {
        float target{0.f};

        auto frate = srProvider->envelope_rate_linear_nowrap(-r);
        phase += frate * (reverse ? -1 : 1);
        int phaseMidpoint{0};
        bool phaseTurned{false};

        if (phase > 1 || phase < 0)
        {
            if (lshape == SH_NOISE || lshape == SMOOTH_NOISE)
            {
                // The deform can push correlated noise out of bounds
                auto ud = d * 0.8;
                rngCurrent =
                    dsp::correlated_noise_o2mk2_suppliedrng(rngState[0], rngState[1], ud, urng);

                rngHistory[3] = rngHistory[2];
                rngHistory[2] = rngHistory[1];
                rngHistory[1] = rngHistory[0];

                rngHistory[0] = rngCurrent;
            }
            if (phase > 1)
            {
                phase -= 1;
                phaseMidpoint = std::clamp((int)std::round(frate / std::max(phase, 0.00001f)), 0,
                                           BLOCK_SIZE - 1);
                phaseTurned = true;
            }
            else
            {
                phase += 1;
            }
        }
        auto shp = (Shape)(lshape);
        switch (shp)
        {
        case SINE:
            target = bend1(std::sin(2.0 * M_PI * phase), d);
            break;
        case RAMP:
            target = bend1(2 * phase - 1, d);
            break;
        case DOWN_RAMP:
            target = bend1(2 * (1 - phase) - 1, d);
            break;
        case TRI:
        {
            auto tphase = (phase + 0.25);
            if (tphase > 1)
                tphase -= 1;
            target = bend1(-1.f + 4.f * ((tphase > 0.5) ? (1 - tphase) : tphase), d);
            break;
        }
        case PULSE:
            target = (phase < (d + 1) * 0.5) ? 1 : -1;
            break;
        case SMOOTH_NOISE:
            target =
                dsp::cubic_ipol(rngHistory[3], rngHistory[2], rngHistory[1], rngHistory[0], phase);
            break;
        case SH_NOISE:
            target = rngCurrent;
            break;
        case RANDOM_TRIGGER:
        {
            if (phaseTurned)
            {
                if (urng() > (-d))
                {
                    // 10 ms triggers according to spec so thats 1% of sample rate
                    rndTrigCountdown =
                        (int)std::round(0.01 * srProvider->samplerate * BLOCK_SIZE_INV);
                }
            }
            if (rndTrigCountdown > 0)
            {
                rndTrigCountdown--;
                target = 1;
            }
            else
            {
                target = -1;
            }
        }
        break;

        default:
            target = 0.0;
            break;
        }
        target = target * amplitude;
        if (phaseMidpoint > 0 && (shp == PULSE || shp == SH_NOISE || shp == RANDOM_TRIGGER))
        {
            for (int i = 0; i < phaseMidpoint; ++i)
                outputBlock[i] = lastTarget;
            for (int i = phaseMidpoint; i < BLOCK_SIZE; ++i)
                outputBlock[i] = target;
        }
        else
        {
            float dO = (target - lastTarget) * BLOCK_SIZE_INV;
            for (int i = 0; i < BLOCK_SIZE; ++i)
            {
                outputBlock[i] = lastTarget + dO * i;
            }
        }
        lastTarget = target;
    }

  private:
    SimpleLFO(const SimpleLFO &) = delete;
    SimpleLFO &operator=(const SimpleLFO &) = delete;
    SimpleLFO(SimpleLFO &&) = delete;
    SimpleLFO &operator=(SimpleLFO &&) = delete;
};
} // namespace sst::basic_blocks::modulators
#endif // RACK_HACK_SIMPLELFO_H
