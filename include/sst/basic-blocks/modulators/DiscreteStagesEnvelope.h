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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_DISCRETESTAGESENVELOPE_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_DISCRETESTAGESENVELOPE_H

namespace sst::basic_blocks::modulators
{
enum DPhaseStrategies
{
    /*
     * SR Provider provides envelope_rate_linear_nowrap(f) = blockSize * 2^-f / sampleRate
     * and we scale into units in the 2^x space
     */
    ENVTIME_2TWOX,
    /*
     * SRProvider isn't consulted. Instead we use an exp table (probably. For now use exp)
     */
    ENVTIME_EXP
};
struct TenSecondRange
{
    // 0.0039s -> 10s
    static constexpr DPhaseStrategies phaseStrategy{ENVTIME_2TWOX};
    static constexpr float etMin{-8}, etMax{3.32192809489};
};

struct ThirtyTwoSecondRange
{
    // 0.0039s -> 32s
    static constexpr DPhaseStrategies phaseStrategy{ENVTIME_2TWOX};
    static constexpr float etMin{-8}, etMax{5};
};

struct TwoMinuteRange
{
    // 0.0039s -> 120s
    static constexpr DPhaseStrategies phaseStrategy{ENVTIME_2TWOX};
    static constexpr float etMin{-8}, etMax{6.90689059561};
};

struct TwentyFiveSecondExp
{
    static constexpr DPhaseStrategies phaseStrategy{ENVTIME_EXP};
    static constexpr double A{0.6931471824646}, B{10.1267113685608}, C{-2.0}, D{1000.0};
};

template <int BLOCK_SIZE, typename RangeProvider> struct DiscreteStagesEnvelope
{
    static constexpr float etminV()
    {
        if constexpr (RangeProvider::phaseStrategy == ENVTIME_2TWOX)
        {
            return RangeProvider::etMin;
        }
        else
        {
            return 0.f;
        }
    }
    static constexpr float etmaxV()
    {
        if constexpr (RangeProvider::phaseStrategy == ENVTIME_2TWOX)
        {
            return RangeProvider::etMax;
        }
        else
        {
            return 1.f;
        }
    }
    static constexpr float etMin{etminV()}, etMax{etmaxV()}, etScale{etMax - etMin};

    static_assert((BLOCK_SIZE >= 8) & !(BLOCK_SIZE & (BLOCK_SIZE - 1)),
                  "Block size must be power of 2 8 or above.");
    static constexpr float BLOCK_SIZE_INV{1.f / BLOCK_SIZE};

    DiscreteStagesEnvelope()
    {
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            outputCache[i] = 0;
            outputCacheCubed[0] = 0;
        }
    }

    float output{0}, outputCubed{0}, eoc_output{0};
    float outputCache alignas(16)[BLOCK_SIZE], outBlock0{0.f};
    float outputCacheCubed alignas(16)[BLOCK_SIZE];
    int current{BLOCK_SIZE};
    int eoc_countdown{0};

    enum Stage
    {
        s_delay, // skipped in ADSR
        s_attack,
        s_decay,   // Skipped in DAHD
        s_sustain, // Ignored in DAHD
        s_hold,
        s_release,
        s_analog_residual_decay,
        s_analog_residual_release,
        s_eoc,
        s_complete
    } stage{s_complete};

    void resetCurrent()
    {
        current = BLOCK_SIZE;
        eoc_output = 0;
        eoc_countdown = 0;
    }

    bool preBlockCheck()
    {
        if (stage == s_complete)
        {
            output = 0;
            return true;
        }

        if (stage == s_eoc)
        {
            output = 0;
            eoc_output = 1;

            eoc_countdown--;
            if (eoc_countdown == 0)
            {
                eoc_output = 0;
                stage = s_complete;
            }
            return true;
        }
        eoc_output = 0;

        if ((stage == s_analog_residual_release || stage == s_analog_residual_decay) &&
            eoc_countdown)
        {
            eoc_output = 1;
            eoc_countdown--;
        }
        return false;
    }

    float shapeTarget(float target, int ashape, int dshape, int rshape = 0)
    {
        if (stage == s_attack)
        {
            switch (ashape)
            {
            case 0:
                target = sqrt(target);
                break;
            case 2:
                target = target * target * target;
                break;
            }
        }
        else if (stage == s_decay)
        {
            switch (dshape)
            {
            case 0:
                target = sqrt(target);
                break;
            case 2:
                target = target * target * target;
                break;
            }
        }
        else if (stage == s_release || stage == s_analog_residual_release)
        {
            switch (rshape)
            {
            case 0:
                target = sqrt(target);
                break;
            case 2:
                target = target * target * target;
                break;
            }
        }
        return target;
    }

    void updateBlockTo(float target)
    {
        float dO = (target - outBlock0) * BLOCK_SIZE_INV;
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            outputCache[i] = outBlock0 + dO * i;
            outputCacheCubed[i] = outputCache[i] * outputCache[i] * outputCache[i];
        }
        outBlock0 = target;
        current = 0;
    }

    void updateBlockToNoCube(float target)
    {
        float dO = (target - outBlock0) * BLOCK_SIZE_INV;
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            outputCache[i] = outBlock0 + dO * i;
        }
        outBlock0 = target;
        current = 0;
    }

    void step()
    {
        output = outputCache[current];
        outputCubed = outputCacheCubed[current];
        current++;
    }

    void immediatelySilence()
    {
        output = 0;
        outputCubed = 0;
        stage = s_complete;
        eoc_output = 0;
        eoc_countdown = 0;
        current = BLOCK_SIZE;
        outBlock0 = 0;
        memset(outputCache, 0, sizeof(outputCache));
        memset(outputCacheCubed, 0, sizeof(outputCacheCubed));
    }

    float rateFrom01(float r01)
    {
        // EXP works entirely in normalized params
        if constexpr (RangeProvider::phaseStrategy == DPhaseStrategies::ENVTIME_EXP)
            return r01;
        else
            return r01 * etScale + etMin;
    }
    float rateTo01(float r)
    {
        if constexpr (RangeProvider::phaseStrategy == DPhaseStrategies::ENVTIME_EXP)
            return r;
        else
            return (r - etMin) / etScale;
    }
    float deltaTo01(float d)
    {
        if constexpr (RangeProvider::phaseStrategy == DPhaseStrategies::ENVTIME_EXP)
            return d;
        else
            return d / etScale;
    }
};
} // namespace sst::basic_blocks::modulators

#endif // SURGEXTRACK_FOURSTAGEENVELOPE_H
