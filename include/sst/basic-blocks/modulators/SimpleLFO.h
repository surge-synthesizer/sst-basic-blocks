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
#include "sst/basic-blocks/dsp/FastMath.h"

#include <cmath>
#include <cassert>

namespace sst::basic_blocks::modulators
{

// For context on SRProvider see the ADSRDAHD Envelope
template <typename SRProvider, int BLOCK_SIZE, bool clampDeform = false> struct SimpleLFO
{
    SRProvider *srProvider{nullptr};

    sst::basic_blocks::dsp::RNG &objrngRef;
    sst::basic_blocks::dsp::RNG objrng{0}; // this is unused in ext ref so seed only if used
    std::function<float()> urng = []() { return 0.f; };

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

        restartRandomSequence(0.f);
    }

    //  Move towards this so we can remove the rng member above
    //  [[deprecated("Use the two-arg constructor with an external RNG")]]
    SimpleLFO(SRProvider *s) : srProvider(s), objrngRef(objrng)
    {
        objrng.reseedWithClock();
        urng = [this]() -> float { return objrng.unifPM1(); };

        for (int i = 0; i < BLOCK_SIZE; ++i)
            outputBlock[i] = 0;

        restartRandomSequence(0.f);
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
        RANDOM_TRIGGER,
        SAW_TRI_RAMP
    };

    float lastTarget{0};
    float outputBlock[BLOCK_SIZE];
    float phase{0};

    inline void restartRandomSequence(double corr)
    {
        rngState[0] = urng();
        rngState[1] = urng();
        // We have to restart and make sure the correlation filter works so do two things
        // First, warm it up with a quick blast. Second, if it does produce out of bound value
        // then try again.
        for (auto i = 0; i < 50; ++i)
        {
            rngCurrent =
                dsp::correlated_noise_o2mk2_suppliedrng(rngState[0], rngState[1], corr, urng);
        }
        int its{0};
        bool allGood{false};
        while (its < 20 && !allGood)
        {
            allGood = true;
            for (int i = 0; i < 4; ++i)
            {
                rngCurrent =
                    dsp::correlated_noise_o2mk2_suppliedrng(rngState[0], rngState[1], corr, urng);
                rngHistory[3 - i] = rngCurrent;
                allGood = allGood && rngHistory[3 - i] > -1 && rngHistory[3 - i] < 1;
            }
            its++;
        }
    }

    inline float bend1(float x, float d) const
    {
        if (d == 0)
            return x;
        if constexpr (clampDeform)
            d = std::clamp(d, -3.f, 3.f);
        auto a = 0.5 * d;

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

    bool needsRandomRestart{false};
    inline void attack(const int lshape)
    {
        phase = 0;
        lastDPhase = 0;
        for (int i = 0; i < BLOCK_SIZE; ++i)
            outputBlock[i] = 0;

        if (lshape == SH_NOISE || lshape == SMOOTH_NOISE)
        {
            needsRandomRestart = true;
            phase = 1.000001;
        }
    }

    float lastDPhase{0};
    inline void applyPhaseOffset(float dPhase)
    {
        if (dPhase != lastDPhase)
        {
            phase += dPhase - lastDPhase;
            if (phase > 1 && !needsRandomRestart)
                phase -= 1;
            if (needsRandomRestart)
                phase = std::clamp(phase, 0.f, 1.999999f);
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

    float lastRate{-123485924.0}, lastFRate{0}, lastTSScale{-76543.2f}, lastSR{0};
    inline void process_block(const float r, const float d, const int lshape, bool reverse = false,
                              float tsScale = 1.f, float phaseDeformAngle = 0)
    {
        float target{0.f};

        auto frate = lastFRate;
        if (r != lastRate || tsScale != lastTSScale || lastSR != srProvider->samplerate)
        {
            frate = tsScale * srProvider->envelope_rate_linear_nowrap(-r);
            lastRate = r;
            lastFRate = frate;
            lastTSScale = tsScale;
            lastSR = srProvider->samplerate;
        }
        phase += frate * (reverse ? -1 : 1);
        int phaseMidpoint{0};
        bool phaseTurned{false};

        if (phase > 1 || phase < 0)
        {
            if (lshape == SH_NOISE || lshape == SMOOTH_NOISE)
            {
                // The deform can push correlated noise out of bounds
                auto ud = d * 0.8;
                if (needsRandomRestart)
                {
                    restartRandomSequence(ud);
                    needsRandomRestart = false;
                }
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
        {
            // target = bend1(std::sin(2.0 * M_PI * phase), d);
            // -sin(x-pi) == sin(x) but since phase[0,1] and fast [-pi,pi] this gets us
            float s = 0.f;
            if (phaseDeformAngle == 0)
            {
                s = -dsp::fastsin(2.0 * M_PI * (phase - 0.5));
            }
            else
            {
                auto x = phase;
                auto g = -0.9999 * phaseDeformAngle;
                auto q = x / (1 - g);
                if (q < 0.25)
                {
                    s = -dsp::fastsin(2.0 * M_PI * (q - 0.5));
                }
                else
                {
                    auto m = 0.5 / (1 - 0.5 * (1 - g));
                    auto b = 0.25 * (1 - m * (1 - g));
                    auto r = m * x + b;
                    if (r > 0.25 && r <= 0.75)
                    {
                        s = -dsp::fastsin(2.0 * M_PI * (r - 0.5));
                    }
                    else
                    {
                        auto q2 = q + 1 - 1 / (1 - g);
                        s = -dsp::fastsin(2.0 * M_PI * (q2 - 0.5));
                    }
                }
            }

            target = bend1(s, d);
        }
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
            if (phaseDeformAngle == 0)
            {
                target = (phase < (d + 1) * 0.5) ? 1 : -1;
            }
            else
            {
                auto useRamp = phaseDeformAngle > 0;
                auto dw = std::fabs(phaseDeformAngle);

                // OK so what we want to do is smooth the upswing and downswing
                // the width of the pulse is (d+1)*0.5 but we always want the shorter
                // pulse
                auto pw = (d + 1) * 0.5;
                auto npw = (pw > 0.5) ? (1 - pw) : (pw);
                auto rpw = npw * dw;

                // are we in the upswing period
                if (phase < rpw / 2 || phase + rpw / 2 >= 1)
                {
                    auto q = (phase + rpw / 2);
                    if (q > 1)
                        q -= 1;
                    q = q / rpw;
                    target = 2 * q - 1;
                    if (!useRamp)
                        target = dsp::fastsin(target * M_PI * 0.5);
                }
                // or the downswing
                else if (phase >= pw - rpw / 2 && phase < pw + rpw / 2)
                {
                    auto q = phase - (pw - rpw / 2);
                    if (q > 1)
                        q -= 1;
                    if (q < 0)
                        q += 1;
                    q = q / rpw;
                    target = 2 * (1 - q) - 1;
                    if (!useRamp)
                        target = dsp::fastsin(target * M_PI * 0.5);
                }
                else
                {
                    target = phase < pw ? 1 : -1;
                }
            }
            break;
        case SMOOTH_NOISE:
            target =
                dsp::cubic_ipol(rngHistory[3], rngHistory[2], rngHistory[1], rngHistory[0], phase);
            if (phaseDeformAngle < 0)
            {
                auto lt = dsp::cubic_ipol(rngHistory[3], rngHistory[2], rngHistory[1],
                                          rngHistory[0], std::sqrt(phase));
                target = -phaseDeformAngle * lt + (1 + phaseDeformAngle) * target;
            }
            else if (phaseDeformAngle > 0)
            {
                auto lt = dsp::cubic_ipol(rngHistory[3], rngHistory[2], rngHistory[1],
                                          rngHistory[0], phase * phase * phase * phase);
                target = phaseDeformAngle * lt + (1 - phaseDeformAngle) * target;
            }
            break;
        case SH_NOISE:
            target = rngCurrent;
            if (phaseDeformAngle > 0)
            {
                // we want if phase = 1 current if phase = 0 rngHistory[1]
                auto lt = rngHistory[1] + (rngCurrent - rngHistory[1]) * phase;
                target = phaseDeformAngle * lt + (1 - phaseDeformAngle) * target;
            }
            else if (phaseDeformAngle < 0)
            {
                auto lt = rngCurrent * (1 - phase);
                target = -phaseDeformAngle * lt + (1 + phaseDeformAngle) * target;
            }
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
        case SAW_TRI_RAMP:
        {
            auto q = phaseDeformAngle * 0.5 + 0.5;
            auto res = 0.f;
            if (q == 0)
            {
                res = 1 - phase;
            }
            else if (q == 1)
            {
                res = phase;
            }
            else
            {
                if (phase < q)
                {
                    res = phase / q;
                }
                else
                {
                    res = (q - phase) / (1 - q) + 1;
                }
            }

            target = bend1(2 * res - 1, d);
        }
        break;

        default:
            target = 0.0;
            break;
        }
        target = target * amplitude;
        if (phaseMidpoint > 0 &&
            ((shp == PULSE && phaseDeformAngle == 0) || shp == SH_NOISE || shp == RANDOM_TRIGGER))
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
