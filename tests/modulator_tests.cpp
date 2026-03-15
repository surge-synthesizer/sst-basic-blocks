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

#include <iostream>

#include "catch2.hpp"
#include "sst/basic-blocks/simd/setup.h"
#include "sst/basic-blocks/modulators/FXModControl.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "sst/basic-blocks/modulators/StepLFO.h"
#include "sst/basic-blocks/modulators/AHDSRShapedSC.h"
#include "sst/basic-blocks/tables/ExpTimeProvider.h"
#include "test_utils.h"

namespace smod = sst::basic_blocks::modulators;

TEST_CASE("Mod LFO Is Well Behaved", "[mod]")
{
    for (int m = smod::FXModControl<32>::mod_sine; m <= smod::FXModControl<32>::mod_snh; ++m)
    {
        DYNAMIC_SECTION("Mod Control Bounded For Type " << m)
        {
            for (int tries = 0; tries < 500; ++tries)
            {
                float rate = (float)rand() / (float)RAND_MAX * 10;
                float phase_offset = 0.0999 * (tries % 10);
                float depth = 1.0;

                INFO("200 Samples at " << rate << " with po " << phase_offset);
                smod::FXModControl<32> mc(48000, 1.0 / 48000);
                for (int s = 0; s < 200; ++s)
                {
                    mc.processStartOfBlock(m, rate, depth, phase_offset);
                    REQUIRE(mc.value() <= 1.0);
                    REQUIRE(mc.value() >= -1.0);
                }
            }
        }
    }
}

static constexpr int bs{8};
static constexpr double tbs{1.0 / bs};

struct SRProvider
{
    static constexpr double sampleRate{48000};
    static constexpr double samplerate{48000};
    static constexpr double sampleRateInv{1.0 / sampleRate};
    float envelope_rate_linear_nowrap(float f) const { return tbs * sampleRateInv * pow(2.f, -f); }
};

TEST_CASE("You can at least make a step LFO", "[mod]")
{
    sst::basic_blocks::tables::EqualTuningProvider e;
    e.init();
    sst::basic_blocks::modulators::StepLFO<16> step(e);
    sst::basic_blocks::modulators::StepLFO<16>::Storage stor;
    REQUIRE(step.output == 0);
}
#if 0
// Well it turns out random isn't strictly bounded, so this test is no good, but we are
// close to boudned now so leave it here in case we want to tweak more
TEST_CASE("Random SimpleLFO is Bounded")
{
    SRProvider sr;
    using slfo_t = sst::basic_blocks::modulators::SimpleLFO<SRProvider, bs>;

    sst::basic_blocks::dsp::RNG urng;
    for (auto tries = 0; tries < 10000; ++tries)
    {
        auto sd = urng.unifU32();
        auto def = urng.unifPM1() * 0.95;
        auto rt = urng.unif01() * 6 - 2;

        // sd=2663274685; rt=1.37236; def=0.00200983;
        urng.reseed(sd);
        INFO("sd=" << sd << "; rt=" << rt << "; def=" << def << ";");
        auto lfo = slfo_t(&sr, urng);
        lfo.attack(slfo_t::Shape::SMOOTH_NOISE);
        for (int i = 0; i < 1000; ++i)
        {
            lfo.process_block(rt, def, slfo_t::Shape::SMOOTH_NOISE);
            for (int j = 0; j < bs; ++j)
            {
                // std::cout << lfo.outputBlock[j] << std::endl;
                REQUIRE(lfo.outputBlock[j] - 1.0 <= 5e-5);
                REQUIRE(lfo.outputBlock[j] + 1.0 >= -5e-5);
            }
        }
    }
}
#endif

TEST_CASE("AHDSRShapedSC Stage Transitions Have Small Initial Phase", "[mod]")
{
    using namespace sst::basic_blocks;
    using namespace test_utils;

    TestSRProvider sr;

    using env_t =
        modulators::AHDSRShapedSC<TestSRProvider, blockSize, modulators::TwentyFiveSecondExp>;
    using base_t = modulators::DiscreteStagesEnvelope<blockSize, modulators::TwentyFiveSecondExp>;

    // When a fast stage precedes a slow stage, crossStagePhaseScale (and the explicit 0.02 cap
    // for delay->attack and delay->decay) ensures the new stage starts with a small phase rather
    // than jumping too far in.
    static constexpr float phaseThreshold{0.05f};
    static constexpr int maxBlocks{100000};

    auto runUntilStage = [&](env_t &env, base_t::Stage targetStage, float delay, float a, float h,
                             float d, float s, float r) -> bool {
        for (int i = 0; i < maxBlocks; ++i)
        {
            env.processBlockWithDelay(delay, a, h, d, s, r, 0.f, 0.f, 0.f, true, false);
            if (env.stage == targetStage)
                return true;
        }
        return false;
    };

    SECTION("Short attack into long decay - small phase at decay entry")
    {
        // Very fast attack (0.02) into slow decay (0.8), zero hold.
        // Without the fix this was the class of bug that caused incorrect envelopes.
        env_t env(&sr);
        env.attackFrom(0.f, false);
        REQUIRE(runUntilStage(env, base_t::s_decay, 0.f, 0.02f, 0.f, 0.8f, 0.5f, 0.5f));
        REQUIRE(env.phase < phaseThreshold);
    }

    SECTION("Short delay into attack - small phase at attack entry")
    {
        // delay->attack uses explicit phase = min(phase, 0.02f) cap on top of crossStagePhaseScale
        env_t env(&sr);
        env.attackFromWithDelay(0.f, 0.05f, 0.8f);
        REQUIRE(runUntilStage(env, base_t::s_attack, 0.05f, 0.8f, 0.f, 0.5f, 0.5f, 0.5f));
        REQUIRE(env.phase < phaseThreshold);
    }

    SECTION("Short delay with zero attack into hold - small phase at hold entry")
    {
        env_t env(&sr);
        env.attackFromWithDelay(0.f, 0.05f, 0.f);
        REQUIRE(runUntilStage(env, base_t::s_hold, 0.05f, 0.f, 0.8f, 0.5f, 0.5f, 0.5f));
        REQUIRE(env.phase < phaseThreshold);
    }

    SECTION("Short delay with zero attack and hold into decay - small phase at decay entry")
    {
        // delay->decay also uses the explicit 0.02f cap
        env_t env(&sr);
        env.attackFromWithDelay(0.f, 0.05f, 0.f);
        REQUIRE(runUntilStage(env, base_t::s_decay, 0.05f, 0.f, 0.f, 0.8f, 0.5f, 0.5f));
        REQUIRE(env.phase < phaseThreshold);
    }

    SECTION("Short hold into decay with zero delay and attack - small phase at decay entry")
    {
        env_t env(&sr);
        env.attackFrom(0.f, false);
        REQUIRE(runUntilStage(env, base_t::s_decay, 0.f, 0.f, 0.05f, 0.8f, 0.5f, 0.5f));
        REQUIRE(env.phase < phaseThreshold);
    }

    SECTION("Short hold into decay with long delay and attack - small phase at decay entry")
    {
        // With slow delay and attack (~1.5s each at param 0.8) preceding a short hold (0.05),
        // the hold->decay transition should still land with a small initial phase.
        env_t env(&sr);
        env.attackFromWithDelay(0.f, 0.8f, 0.8f);
        REQUIRE(runUntilStage(env, base_t::s_decay, 0.8f, 0.8f, 0.05f, 0.8f, 0.5f, 0.5f));
        REQUIRE(env.phase < phaseThreshold);
    }
}

TEST_CASE("AHDSRShapedSC TwentyFiveSecondExp dPhase matches ExpTimeProvider", "[mod]")
{
    using namespace sst::basic_blocks;
    using namespace test_utils;

    TestSRProvider sr;

    using env_t =
        modulators::AHDSRShapedSC<TestSRProvider, blockSize, modulators::TwentyFiveSecondExp>;
    env_t env(&sr);

    tables::TwentyFiveSecondExpTable expTable;
    expTable.init();

    // For each parameter value x in (0,1], dPhase from the envelope should imply
    // the same time in seconds as the ExpTimeProvider table.
    // dPhase = blockSize * sampleRateInv * (1/timeInSeconds)
    // => timeInSeconds = blockSize * sampleRateInv / dPhase
    for (float x = 0.01f; x <= 1.0f; x += 0.01f)
    {
        auto dp = env.dPhase(x);
        REQUIRE(dp > 0.f);

        double envTimeSeconds = blockSize * sr.sampleRateInv / dp;
        double tableTimeSeconds = expTable.timeInSecondsFromParam(x);

        INFO("At x=" << x << " envTime=" << envTimeSeconds << " tableTime=" << tableTimeSeconds);
        REQUIRE(envTimeSeconds == Approx(tableTimeSeconds).epsilon(0.01));
    }
}
