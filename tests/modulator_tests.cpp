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
#include <cstdio>
#include <cstdlib>

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

// ---------------------------------------------------------------------------
// FXModControl golden-value tests
// ---------------------------------------------------------------------------
// Run with SBBFXMOD_GOLDEN=1 to print new arrays instead of checking them.
// ---------------------------------------------------------------------------

static constexpr int FXGOLDEN_BS = 32;
static constexpr int FXGOLDEN_WARMUP = 20; // blocks discarded before recording
static constexpr int FXGOLDEN_RECORD = 30; // blocks whose start-of-block value is captured
static constexpr float FXGOLDEN_SR = 48000.f;
static constexpr float FXGOLDEN_TOL = 1e-5f;

static bool fxmodGoldenPrintMode() { return std::getenv("SBBFXMOD_GOLDEN") != nullptr; }

static void fxmodGoldenCheckOrPrint(const char *label,
                                    const std::array<float, FXGOLDEN_RECORD> &got,
                                    const std::array<float, FXGOLDEN_RECORD> &expected)
{
    if (fxmodGoldenPrintMode())
    {
        std::printf("// %s\n        {", label);
        for (int i = 0; i < FXGOLDEN_RECORD; ++i)
        {
            if (i > 0 && i % 5 == 0)
                std::printf("\n         ");
            std::printf("%.9ff%s", got[i], i + 1 < FXGOLDEN_RECORD ? ", " : "");
        }
        std::printf("};\n");
    }
    else
    {
        for (int i = 0; i < FXGOLDEN_RECORD; ++i)
        {
            INFO(label << " block[" << i << "]: got=" << got[i] << " expected=" << expected[i]);
            REQUIRE(got[i] == Approx(expected[i]).margin(FXGOLDEN_TOL));
        }
    }
}

// Runs the LFO for FXGOLDEN_WARMUP blocks (discarded), then records the
// start-of-block value() for FXGOLDEN_RECORD consecutive blocks.
// rate=0.03 means the phase advances 0.03 per block (~3 full cycles over
// the 100-block test window, giving good coverage of every wave shape).
template <smod::RandomBehavior RB = smod::rnd_dual_stereo>
static std::array<float, FXGOLDEN_RECORD> fxmodRunGolden(int mwave, float rate, float depth,
                                                         float phase_offset)
{
    using mc_t = smod::FXModControl<FXGOLDEN_BS, RB>;
    mc_t mc(FXGOLDEN_SR, 1.f / FXGOLDEN_SR);
    mc.rng.reseed(0xDEADBEEFu);

    for (int b = 0; b < FXGOLDEN_WARMUP; ++b)
    {
        mc.processStartOfBlock(mwave, rate, depth, phase_offset);
        for (int s = 0; s < FXGOLDEN_BS; ++s)
            mc.process();
    }

    std::array<float, FXGOLDEN_RECORD> got{};
    for (int b = 0; b < FXGOLDEN_RECORD; ++b)
    {
        mc.processStartOfBlock(mwave, rate, depth, phase_offset);
        got[b] = mc.value();
        for (int s = 0; s < FXGOLDEN_BS; ++s)
            mc.process();
    }
    return got;
}

TEST_CASE("FXModControl Golden Values", "[FXModControl][golden]")
{
    using mc_t = smod::FXModControl<FXGOLDEN_BS>;

    SECTION("sine rate=0.03")
    {
        auto got = fxmodRunGolden(mc_t::mod_sine, 0.03f, 1.0f, 0.0f);
        // sine rate=0.03
        static const std::array<float, FXGOLDEN_RECORD> expected{
            -0.587784767f, -0.728968084f, -0.844327331f, -0.929775953f, -0.982286930f,
            -1.000000000f, -0.982287467f, -0.929777145f, -0.844329000f, -0.728970110f,
            -0.587787151f, -0.425781578f, -0.248692542f, -0.062793449f, 0.125330135f,
            0.309014022f,  0.481750906f,  0.637421608f,  0.770511270f,  0.876305163f,
            0.951055527f,  0.992114246f,  0.998026907f,  0.968583822f,  0.904828310f,
            0.809018672f,  0.684549332f,  0.535829306f,  0.368127316f,  0.187384248f};
        fxmodGoldenCheckOrPrint("sine rate=0.03", got, expected);
    }

    SECTION("tri rate=0.03")
    {
        auto got = fxmodRunGolden(mc_t::mod_tri, 0.03f, 1.0f, 0.0f);
        // tri rate=0.03
        static const std::array<float, FXGOLDEN_RECORD> expected{
            -0.600000381f, -0.480000496f, -0.360000610f, -0.240000725f, -0.120000839f,
            -0.000000954f, 0.119998932f,  0.239998817f,  0.359998703f,  0.479998589f,
            0.599998474f,  0.719998360f,  0.839998245f,  0.959998131f,  0.920001984f,
            0.800001979f,  0.680001974f,  0.560001969f,  0.440001965f,  0.320001960f,
            0.200001955f,  0.080001950f,  -0.039998055f, -0.159998059f, -0.279998064f,
            -0.399998069f, -0.519998074f, -0.639998078f, -0.759998083f, -0.879998088f};
        fxmodGoldenCheckOrPrint("tri rate=0.03", got, expected);
    }

    SECTION("saw rate=0.03")
    {
        auto got = fxmodRunGolden(mc_t::mod_saw, 0.03f, 1.0f, 0.0f);
        // saw rate=0.03
        static const std::array<float, FXGOLDEN_RECORD> expected{
            0.224489570f,  0.285714030f,  0.346938491f,  0.408162832f,  0.469387293f,
            0.530611753f,  0.591836214f,  0.653060555f,  0.714285016f,  0.775509477f,
            0.836733937f,  0.897958279f,  0.959182739f,  0.000047684f,  -0.959184706f,
            -0.897960186f, -0.836735725f, -0.775511205f, -0.714286685f, -0.653062224f,
            -0.591837764f, -0.530613244f, -0.469388783f, -0.408164263f, -0.346939802f,
            -0.285715282f, -0.224490821f, -0.163266301f, -0.102041841f, -0.040817320f};
        fxmodGoldenCheckOrPrint("saw rate=0.03", got, expected);
    }

    SECTION("square rate=0.03")
    {
        auto got = fxmodRunGolden(mc_t::mod_square, 0.03f, 1.0f, 0.0f);
        // square rate=0.03 — transition width halved (0.02→0.01) in SSE rewrite
        static const std::array<float, FXGOLDEN_RECORD> expected{
            -1.000000000f, -1.000000000f, -1.000000000f, -1.000000000f, -1.000000000f,
            -1.000000000f, -1.000000000f, -1.000000000f, -1.000000000f, -1.000000000f,
            -1.000000000f, -1.000000000f, -1.000000000f, -1.000000000f, 1.000000000f,
            1.000000000f,  1.000000000f,  1.000000000f,  1.000000000f,  1.000000000f,
            1.000000000f,  1.000000000f,  1.000000000f,  1.000000000f,  1.000000000f,
            1.000000000f,  1.000000000f,  1.000000000f,  1.000000000f,  1.000000000f};
        fxmodGoldenCheckOrPrint("square rate=0.03", got, expected);
    }

    SECTION("snh rate=0.03")
    {
        auto got = fxmodRunGolden<smod::rnd_single>(mc_t::mod_snh, 0.03f, 1.0f, 0.0f);
        // snh rate=0.03 (rnd_single) — new golden values
        static const std::array<float, FXGOLDEN_RECORD> expected{
            0.926174998f, 0.926174998f, 0.926174998f, 0.926174998f, 0.926174998f, 0.926174998f,
            0.926174998f, 0.926174998f, 0.926174998f, 0.926174998f, 0.926174998f, 0.926174998f,
            0.926174998f, 0.926174998f, 0.669818997f, 0.669818997f, 0.669818997f, 0.669818997f,
            0.669818997f, 0.669818997f, 0.669818997f, 0.669818997f, 0.669818997f, 0.669818997f,
            0.669818997f, 0.669818997f, 0.669818997f, 0.669818997f, 0.669818997f, 0.669818997f};
        fxmodGoldenCheckOrPrint("snh rate=0.03", got, expected);
    }

    SECTION("noise rate=0.03")
    {
        auto got = fxmodRunGolden<smod::rnd_single>(mc_t::mod_noise, 0.03f, 1.0f, 0.0f);
        // noise rate=0.03 (rnd_single) — new golden values
        static const std::array<float, FXGOLDEN_RECORD> expected{
            0.657485068f, 0.673607290f, 0.688761890f, 0.703006029f, 0.716396093f, 0.728982389f,
            0.740814209f, 0.751935363f, 0.762390316f, 0.772216678f, 0.781454444f, 0.790137231f,
            0.798299015f, 0.805971324f, 0.797801316f, 0.790123165f, 0.782904148f, 0.776118755f,
            0.769739866f, 0.763745308f, 0.758109510f, 0.752811670f, 0.747831404f, 0.743150830f,
            0.738750875f, 0.734614670f, 0.730726123f, 0.727071166f, 0.723636508f, 0.720407009f};
        fxmodGoldenCheckOrPrint("noise rate=0.03", got, expected);
    }

    SECTION("sine phase_offset=0.25")
    {
        auto got = fxmodRunGolden(mc_t::mod_sine, 0.03f, 1.0f, 0.25f);
        // sine phase_offset=0.25
        static const std::array<float, FXGOLDEN_RECORD> expected{
            -0.809017301f, -0.684547603f, -0.535827577f, -0.368125588f, -0.187382609f,
            -0.000001498f, 0.187379643f,  0.368122786f,  0.535825074f,  0.684545457f,
            0.809015512f,  0.904825926f,  0.968582392f,  0.998026490f,  0.992115021f,
            0.951057434f,  0.876308084f,  0.770515144f,  0.637426317f,  0.481756330f,
            0.309019923f,  0.125336260f,  -0.062787466f, -0.248686731f, -0.425776482f,
            -0.587782919f, -0.728966534f, -0.844326138f, -0.929775357f, -0.982286692f};
        fxmodGoldenCheckOrPrint("sine phase_offset=0.25", got, expected);
    }

    SECTION("sine depth=0.5")
    {
        auto got = fxmodRunGolden(mc_t::mod_sine, 0.03f, 0.5f, 0.0f);
        // sine depth=0.5
        static const std::array<float, FXGOLDEN_RECORD> expected{
            -0.293892384f, -0.364484042f, -0.422163665f, -0.464887977f, -0.491143465f,
            -0.500000000f, -0.491143733f, -0.464888573f, -0.422164500f, -0.364485055f,
            -0.293893576f, -0.212890789f, -0.124346271f, -0.031396724f, 0.062665068f,
            0.154507011f,  0.240875453f,  0.318710804f,  0.385255635f,  0.438152581f,
            0.475527763f,  0.496057123f,  0.499013454f,  0.484291911f,  0.452414155f,
            0.404509336f,  0.342274666f,  0.267914653f,  0.184063658f,  0.093692124f};
        fxmodGoldenCheckOrPrint("sine depth=0.5", got, expected);
    }
}
